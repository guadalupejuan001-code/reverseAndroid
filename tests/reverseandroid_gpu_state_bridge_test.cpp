#define main original_pci_test_main
#include "minimal_guest_pci_test.cpp"
#undef main
#include "gpu-state-bridge.hpp"
#include <optional>
#include <thread>
#include <atomic>
using reverse_pc::integration::GpuStateBridge;

int main(){try{
 Bytes kernel(4096);field(kernel,0,0xd503201f,4);field(kernel,8,0x80000,8);field(kernel,16,kernel.size(),8);field(kernel,56,0x644d5241,4);
 MinimalGuest g(2147483648ULL,0);g.load_linux_image(kernel,{},"",nullptr,true);Client gpu(g,1);
 auto create=command(0x101,40);field(create,24,8,4);field(create,28,2,4);field(create,32,1,4);field(create,36,1,4);
 check(gpu.gpu(create)==0x1100,"Bridge resource create");
 const auto pixels=UINT64_C(0x42000000);g.write(pixels,0xaa112233,4);
 auto attach=command(0x106,48);field(attach,24,8,4);field(attach,28,1,4);field(attach,32,pixels,8);field(attach,40,4,4);
 auto transfer=command(0x105,56);setrect(transfer,1,1);field(transfer,48,8,4);
 check(gpu.gpu(attach)==0x1100&&gpu.gpu(transfer)==0x1100,"Bridge native GPU transfer");
 GpuStateBridge bus(g);auto cpu=bus.snapshot();auto client=bus;
 auto old=bus.acquire(8);check(cpu==client.snapshot()&&old.revision()==cpu->serial,"CPU and GPU see the same QSSD root");
 const auto original=g.snapshot();check(bus.native_snapshot()==original,"Bridge retains complete original native VM state");
 check(old.word(0)==0xaa112233&&old.projected_bytes()==4,"Explicit four-byte projection");
 g.write(pixels,0xbb445566,4);check(gpu.gpu(transfer)==0x1100,"Next CPU/GPU state");
 auto next=bus.publish(g,cpu,"real-cpu-gpu-transition");auto current=client.acquire(8);
 check(next!=cpu&&client.snapshot()==next&&current.word(0)==0xbb445566&&old.word(0)==0xaa112233,"Old GPU work retains its exact complete dependency");
 check(bus.native_snapshot(cpu)==original&&bus.native_snapshot(next)==g.snapshot(),"Both complete native versions remain exact");
 bool conflict=false;try{bus.publish(g,cpu,"stale-writer");}catch(const qssd::StateConflict&){conflict=true;}
 check(conflict&&bus.snapshot()==next,"Stale publication changed current authority");
 auto unref=command(0x102,32);field(unref,24,8,4);check(gpu.gpu(unref)==0x1100,"Retire original resource");
 bus.publish(g,next,"native-resource-unref");
 rejected([&]{bus.acquire(8);},"Retired ID invented in current root");
 check(old.word(0)==0xaa112233&&current.word(0)==0xbb445566,"Retained work lost its true resource version");
 std::atomic<bool> bad{false};std::vector<std::thread> readers;
 for(unsigned t=0;t<4;++t)readers.emplace_back([&]{for(unsigned n=0;n<250;++n)if(old.word(0)!=0xaa112233)bad=true;});
 for(auto& reader:readers)reader.join();check(!bad,"Concurrent GPU queries mixed native state");
 MinimalGuest foreign(2147483648ULL,0);foreign.load_linux_image(kernel,{},"",nullptr,true);
 const auto before=bus.snapshot();rejected([&]{bus.publish(foreign,before,"different-vm");},"Foreign VM silently published");
 check(bus.snapshot()==before,"Rejected VM changed root");
 std::optional<GpuStateBridge::Surface> held;
 {GpuStateBridge temporary(*cpu->value->vm);held.emplace(temporary.acquire(8));}
 check(held->word(0)==0xaa112233,"GPU view died with producer");
 MinimalGuest coherent=*cpu->value->vm;
 coherent.apply_xi_delta("mem_bind "+std::to_string(GuestPciState::gpu_pixels)+" 50000\nh 50000\n");
 GpuStateBridge quantum(coherent);auto unresolved=quantum.acquire(8);
 const auto quantum_state=quantum.native_snapshot();
 rejected([&]{unresolved.word(0);},"Bridge silently measured unresolved original XI state");
 check(unresolved.projected_bytes()==0&&quantum.native_snapshot()==quantum_state,"Failed projection altered original quantum authority");
 rejected([&]{bus.acquire(8,quantum.snapshot());},"Foreign QSSD authority accepted");
 std::cout<<"{\"qssd_complete_native_VM_bridge\":\"PASS\",\"checks\":"<<checks
   <<",\"logical_RAM_bytes\":2147483648,\"whole_original_snapshots_retained\":true,\"concurrent_word_queries\":1000,\"native_GPU_renderer_acceleration_claimed\":false,\"android_boot_verified\":false}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
