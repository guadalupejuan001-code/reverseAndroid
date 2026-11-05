// C43 transport regression: the legacy serializer is the independent oracle.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "minimal_guest_system_demo.hpp"
#include "minimal_guest_pci_demo.hpp"
#include "minimal_guest_fp_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0;
void check(bool ok,const char* why){++checks;require(ok,why);}
template<class F>void rejects(F action){bool bad=false;try{action();}catch(const std::exception&){bad=true;}check(bad,"Expected transport rejection");}
Bytes elf(){
 Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
 put(b,2,2);put(b,183,2);put(b,1,4);put(b,0x1000);put(b,64);put(b,0);put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
 put(b,1,4);put(b,7,4);put(b,0x100);put(b,0x1000);put(b,0x1000);put(b,16);put(b,16);put(b,0x100);b.resize(0x100);
 for(auto op:{0xd503201fu,0x91000400u,0xd503201fu,0x14000000u})put(b,op,4);return b;
}
void verify(MinimalGuest guest,bool advance=false){
 const auto before=guest.snapshot();const auto core=guest.runtime().core_snapshot(),memory=guest.runtime().memory_persistent_snapshot();
 Bytes streamed;std::size_t largest=0;
 const auto size=guest.snapshot_to([&](const std::uint8_t* data,std::size_t n){largest=std::max(largest,n);streamed.insert(streamed.end(),data,data+n);});
 check(streamed==before,"Stream changed the legacy RVG bytes");check(size==before.size(),"Stream size differs");check(largest<=65536,"Unbounded output chunk");
 check(guest.snapshot_sha256()==GuestSha256::hash(before),"Streaming SHA differs from independent legacy SHA");
 check(guest.runtime().core_snapshot()==core&&guest.runtime().memory_persistent_snapshot()==memory,"Serialization changed XI/Ether");
 const auto limit=guest.host_snapshot_transport_capacity();auto a=MinimalGuest::from_snapshot(before,nullptr,limit),b=MinimalGuest::from_snapshot(streamed,nullptr,limit);
 check(a.snapshot()==b.snapshot(),"Legacy decoder restored a different state");
 if(advance){a.run(1);b.run(1);check(a.snapshot()==b.snapshot(),"Continuation after restore differs");}
 std::size_t callbacks=0;rejects([&]{guest.snapshot_to([&](const std::uint8_t*,std::size_t){if(++callbacks==2)throw std::runtime_error("Simulated disk full");});});
 check(guest.snapshot()==before,"Output failure modified the guest");++cases;
}
}
int main(int argc,char** argv){try{
 require(argc==2,"Expected temporary directory");const auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root);
 for(auto limit:{65536u,0u}){
  MinimalGuest guest(1024*1024,limit);guest.load_elf(elf());verify(guest,true);
  guest.apply_xi_delta("mem_store 12288 255\nmem_store 18446744073709551616 127\nh 0\ncx 0 1\nmem_bind 32768 0\n");verify(guest,true);
  for(unsigned page=0;page<4;++page)for(unsigned n=0;n<180;++n)guest.write(0x10000+4096*page+8*n,UINT64_C(0x0102030405060708)^n,8);
  verify(guest,true);guest.run(2);verify(guest,true);
 }
 MinimalGuest system(128*1024*1024,0);system.load_elf(system_demo_elf(),true);verify(system);
 MinimalGuest pci(128*1024*1024,0);pci.load_linux_image(pci_graphics_witness_image(),{},"console=ttyAMA0",nullptr,true);verify(pci);
 MinimalGuest fp(65536);fp.load_elf(fp_demo_elf());fp.run(10);verify(fp,true);
 auto limited=pci;limited.set_host_snapshot_transport_capacity(4096);bool emitted=false;
 rejects([&]{limited.snapshot_to([&](const std::uint8_t*,std::size_t){emitted=true;});});check(!emitted,"Capacity rejected only after output began");
 const auto candidate=(root/"prior-candidate.rvg").string();guest_write_snapshot_candidate(system,candidate);
 const auto stable=guest_read_file(candidate,MinimalGuest::max_adaptive_snapshot);
 rejects([&]{guest_write_snapshot_candidate(limited,candidate);});
 check(guest_read_file(candidate,MinimalGuest::max_adaptive_snapshot)==stable,"Rejected stream replaced a valid candidate");
 for(const auto& entry:std::filesystem::directory_iterator(root))check(entry.path().filename().string().find("pending-")==std::string::npos,"Abandoned staged candidate");
 BootCycleJournal journal(root/"journal");BootCycleReport report;report.reason="instruction_budget";
 for(unsigned n=0;n<3;++n){journal.publish(system,report);const auto p=root/"journal"/("generation-"+std::to_string(n+1));
  check(guest_read_file((p/"checkpoint.rvg").string(),MinimalGuest::max_adaptive_snapshot)==system.snapshot(),"Published generation differs");}
 check(!std::filesystem::exists(root/"journal/generation-1")&&std::filesystem::exists(root/"journal/generation-2"),"Journal retention regressed");
 std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"legacy_rvg_bytes_identical\":true,\"decoder_unchanged\":true,\"max_output_chunk\":65536,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
