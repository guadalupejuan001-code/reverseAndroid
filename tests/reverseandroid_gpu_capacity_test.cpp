#define main original_pci_test_main
#include "minimal_guest_pci_test.cpp"
#undef main
#include "guest_file_session.hpp"

int main(int argc,char** argv){try{
#ifndef REVERSE_GPU_CAPACITY_CANDIDATE
 if(argc==3&&std::string(argv[1])=="--reject-extended"){
   bool refused=false;
   try{auto g=MinimalGuest::from_snapshot_view(guest_read_file(argv[2],MinimalGuest::max_view_snapshot),nullptr,MinimalGuest::max_view_snapshot);}
   catch(const std::exception& e){refused=std::string(e.what()).find("Unsupported PCI state version")!=std::string::npos;}
   require(refused,"Old decoder must reject the declared new GPU subformat");
   std::cout<<"{\"old_decoder_refuses_new_GPU_subformat\":true}\n";return 0;
 }
#endif
 require(argc==2,"Expected fixture output");
 Bytes kernel(4096);field(kernel,0,0xd503201f,4);field(kernel,8,0x80000,8);field(kernel,16,kernel.size(),8);field(kernel,56,0x644d5241,4);
 MinimalGuest g(2147483648ULL,0);g.load_linux_image(kernel,{},"",nullptr,true);Client gpu(g,1);
 auto create=command(0x101,40);field(create,28,2,4);field(create,32,1,4);field(create,36,1,4);
 for(unsigned id:{1u,4u,5u,6u}){field(create,24,id,4);check(gpu.gpu(create)==0x1100,"Initial four native GPU resources");}
 field(create,24,7,4);const auto seventh=gpu.gpu(create);field(create,24,8,4);const auto eighth=gpu.gpu(create);
#ifdef REVERSE_GPU_CAPACITY_CANDIDATE
 check(seventh==0x1100&&eighth==0x1100,"Expanded device must allocate both Android-style additional resources");
#else
 check(seventh==0x1201&&eighth==0x1201,"Original device allocation exhaustion reproduction");
#endif
 auto unref=command(0x102,32);field(unref,24,1,4);check(gpu.gpu(unref)==0x1100,"Retire prior resource1");
 const auto pixels=UINT64_C(0x42000000);g.write(pixels,0xaa112233,4);
 auto attach=command(0x106,48);field(attach,24,8,4);field(attach,28,1,4);field(attach,32,pixels,8);field(attach,40,4,4);
 auto transfer=command(0x105,56);setrect(transfer,1,1);field(transfer,48,8,4);
 auto scan=command(0x103,48);setrect(scan,1,1);field(scan,44,8,4);
 auto flush=command(0x104,48);setrect(flush,1,1);field(flush,40,8,4);
#ifdef REVERSE_GPU_CAPACITY_CANDIDATE
 check(gpu.gpu(attach)==0x1100&&gpu.gpu(transfer)==0x1100&&gpu.gpu(scan)==0x1100&&gpu.gpu(flush)==0x1100,"Resource8 complete real DMA/presentation lifecycle");
 check(g.display_rgb()==Bytes({0x11,0x22,0x33})&&g.gpu_resource_word(8,0)==0xaa112233,"Resource8 exact XI pixels and alpha bits");
 check(get(g.machine().pci.save(),0,8)==2,"Extra resources must use declared GPU PCI subformat2");
#else
 check(gpu.gpu(attach)==0x1203&&gpu.gpu(transfer)==0x1203&&gpu.gpu(scan)==0x1203&&gpu.gpu(flush)==0x1203,"Original persistent missing-resource reproduction");
 check(g.machine().pci.resources[0].id==0&&g.machine().pci.resources[1].id==4&&g.machine().pci.resources[2].id==5&&g.machine().pci.resources[3].id==6,"Actual observed resource table reproduced");
#endif
 const auto state=g.snapshot();auto replay=MinimalGuest::from_snapshot_view(state,nullptr,MinimalGuest::max_view_snapshot);
 check(replay.snapshot()==state&&replay.display_rgb()==g.display_rgb(),"Whole native snapshot and original pixels restore");
 save(argv[1],state);
#ifdef REVERSE_GPU_CAPACITY_CANDIDATE
 // Use every physical slot without crossing the reserved scanout plane.
 MinimalGuest full(2147483648ULL,0);full.load_linux_image(kernel,{},"",nullptr,true);Client client(full,1);
 for(unsigned id=1;id<=16;++id){
   field(create,24,id,4);check(client.gpu(create)==0x1100,"All sixteen declared slots must work");
   full.write(pixels,0xbb000000|id,4);field(attach,24,id,4);field(transfer,48,id,4);
   check(client.gpu(attach)==0x1100&&client.gpu(transfer)==0x1100,"Each slot owns exact original XI pixel storage");
   if(id==1){field(scan,44,1,4);check(client.gpu(scan)==0x1100,"Original scanout");}
   check(full.display_rgb()==Bytes({0,0,1}),"Unflushed later slot collided with scanout plane");
   check(full.gpu_resource_word(id,0)==(0xbb000000|id),"Independent slot pixel identity");
 }
 field(create,24,17,4);check(client.gpu(create)==0x1201,"Declared slot bound must remain enforced");
 auto point=full.snapshot();check(MinimalGuest::from_snapshot_view(point,nullptr,MinimalGuest::max_view_snapshot).snapshot()==point,"All sixteen states restore exactly");
 rejected([&]{full.gpu_resource_word(0,0);},"Reserved zero resource projection accepted");
 rejected([&]{full.gpu_resource_word(1,1);},"Out-of-extent resource projection accepted");
 // Metadata remains inside the original 196608-byte machine section budget.
 GuestPciState metadata;metadata.enabled=true;
 for(unsigned slot=0;slot<5;++slot){auto& r=metadata.resources[slot];r.id=slot+1;r.width=r.height=1;r.format=2;}
 for(unsigned slot=0;slot<4;++slot)for(unsigned n=0;n<2048;++n)metadata.resources[slot].backing.push_back({0x40000000,4096});
 const auto bounded=metadata.save();check(bounded.size()<196608&&GuestPciState::restore(bounded).save()==bounded,"Original metadata extent and lossless backing");
 metadata.resources[4].backing.push_back({0x40000000,4096});
 rejected([&]{metadata.save();},"Aggregate metadata budget silently exceeded");
 constexpr unsigned count_offset=32+2*(2+6+2*7)*8;
 auto bad=bounded;field(bad,count_offset,17,8); // count follows both original device records
 rejected([&]{GuestPciState::restore(bad);},"Unsupported GPU table capacity accepted");
#endif
 std::cout<<"{\"protocol_fixture_only\":true,\"checks\":"<<checks<<",\"resource_capacity\":"<<g.machine().pci.resources.size()
   <<",\"resource7_create_response\":"<<seventh<<",\"resource8_create_response\":"<<eighth
   <<",\"logical_RAM_bytes\":"<<g.logical_ram()<<",\"native_snapshot_bytes\":"<<state.size()
   <<",\"whole_state_restore\":true,\"android_boot_verified\":false}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
