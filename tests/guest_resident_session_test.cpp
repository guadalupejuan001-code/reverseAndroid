#include "guest_resident_session.hpp"
#include "minimal_guest_pci_demo.hpp"
#include <iostream>
#include <thread>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;require(ok,why);}
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,"Expected session rejection");}
bool field(const std::string& report,const std::string& value){return report.find(value)!=std::string::npos;}
Bytes elf(){
 Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
 put(b,2,2);put(b,183,2);put(b,1,4);put(b,0x1000);put(b,64);put(b,0);put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
 put(b,1,4);put(b,7,4);put(b,0x100);put(b,0x1000);put(b,0x1000);put(b,16);put(b,16);put(b,0x100);b.resize(0x100);
 for(auto op:{0xd2860001u,0x91000400u,0xf9000020u,0x17fffffeu})put(b,op,4);return b;
}
}
int main(int argc,char** argv){try{
 require(argc==2,"Expected a temporary directory");const auto root=std::filesystem::path(argv[1])/std::to_string(::getpid());std::filesystem::create_directories(root);
 auto path=[&](const char* name){return (root/name).string();};auto bytes=[&](const std::string& p){return guest_read_file(p,MinimalGuest::max_adaptive_snapshot);};
 MinimalGuest reference(65536,0);reference.load_elf(elf());
 reference.apply_xi_delta("mem_store 18446744073709551616 127\nh 0\ncx 0 1\nmem_bind 40000 0\n");
 guest_write_snapshot_candidate(reference,path("current.rvg"));const auto original=reference.snapshot();
 auto session=GuestResidentSession::open(path("current.rvg"));
 check(!session->dirty(),"Fresh session is dirty");check(field(session->inspect(),"\"resident\":true"),"Missing live resident report");
 for(unsigned i=0;i<4;++i){session->run(1000);reference.run(1000);}
 check(bytes(path("current.rvg"))==original,"Execution wrote a full checkpoint before flush");
 // Remove the backing checkpoint while running: continued execution must use
 // the already loaded authority, not silently restore it on each request.
 std::filesystem::rename(path("current.rvg"),path("durable-backup.rvg"));
 session->run(1000);reference.run(1000);
 std::filesystem::rename(path("durable-backup.rvg"),path("current.rvg"));
 session->stage(path("stage.rvg"));check(bytes(path("stage.rvg"))==reference.snapshot(),"Resident run differs from direct original CPU");
 check(session->dirty(),"Staging acknowledged durability too early");
 const auto stale=session->revision();session->run(1000);reference.run(1000);
 rejects([&]{session->acknowledge(stale);});check(session->dirty(),"Stale acknowledgement cleared dirty state");
 rejects([&]{session->stage(path("current.rvg"));});check(bytes(path("current.rvg"))==original,"Alias replaced durable source");
 session->stage(path("stage.rvg"));const auto accepted=bytes(path("stage.rvg"));
 auto crash=MinimalGuest::from_snapshot(bytes(path("current.rvg")));check(crash.snapshot()==original,"Unacknowledged state became the crash source");
 std::filesystem::copy_file(path("stage.rvg"),path("current.rvg"),std::filesystem::copy_options::overwrite_existing);
 session->acknowledge(session->revision());check(!session->dirty(),"Published checkpoint remains dirty");
 check(MinimalGuest::from_snapshot(bytes(path("current.rvg"))).snapshot()==reference.snapshot(),"Original decoder restored a different published state");
 const auto before=session->revision();rejects([&]{session->xi("mem_store 12288 99\nunknown_operation\n");});
 check(session->revision()==before&&!session->dirty(),"Rejected XI operation committed session metadata");
 session->stage(path("stage.rvg"));check(bytes(path("stage.rvg"))==accepted,"Rejected XI changed guest bytes");
 session->xi("mem_store 12288 42\n");reference.apply_xi_delta("mem_store 12288 42\n");
 session->stage(path("stage.rvg"));check(bytes(path("stage.rvg"))==reference.snapshot(),"Resident XI differs");
 rejects([&]{session->run(250001);});rejects([&]{session->input(std::string(4097,'a'));});
 rejects([&]{session->stage(path("missing/output.rvg"));});check(session->dirty(),"Failed write discarded accepted blocks");
 session->stage(path("stage.rvg"));check(bytes(path("stage.rvg"))==reference.snapshot(),"Failed write changed guest state");
 const auto rev=session->revision();check(field(session->display(path("frame.rgb")),"\"frame_bytes\":0"),"Empty display changed");check(session->revision()==rev,"Display mutated session revision");

 std::uint64_t now=0;GuestResidentSession timed(reference,path("timed-source.rvg"),accepted.size(),{30000,5000},[&]{return now;});
 timed.run(1000);check(field(timed.inspect(),"\"checkpoint_due\":false"),"Checkpoint due too early");now=30000;
 check(field(timed.inspect(),"\"checkpoint_due\":true"),"Wall deadline ignored");timed.stage(path("timed-stage.rvg"));timed.acknowledge(timed.revision());
 check(field(timed.inspect(),"\"checkpoint_due\":false"),"Acknowledgement did not reset cadence");timed.run(5000);
 check(field(timed.inspect(),"\"checkpoint_due\":true"),"Instruction deadline ignored");

 MinimalGuest fault(65536);fault.load_elf(elf());fault.write(0x1000,0x4fc01062,4);guest_write_snapshot_candidate(fault,path("fault.rvg"));
 auto broken=GuestResidentSession::open(path("fault.rvg"));rejects([&]{broken->run(1);});
 check(!broken->dirty()&&broken->revision()==0,"Failed architectural transaction committed");broken->stage(path("fault-stage.rvg"));
 check(bytes(path("fault-stage.rvg"))==fault.snapshot(),"ISA error changed canonical state");

 GuestResidentRegistry registry;const auto first=registry.open(path("current.rvg"));registry.operation(first,1,1);
 rejects([&]{registry.close(first);});registry.operation(first,4,0,"",path("registry-stage.rvg"));
 std::filesystem::copy_file(path("registry-stage.rvg"),path("current.rvg"),std::filesystem::copy_options::overwrite_existing);
 registry.operation(first,6,0,"","",1);registry.close(first);
 rejects([&]{registry.operation(first,0);});rejects([&]{registry.close(first);});const auto second=registry.open(path("current.rvg"));check(second!=first,"Closed handle was reused");
 std::thread a([&]{registry.operation(second,1,1000);}),b([&]{registry.operation(second,1,1000);});a.join();b.join();
 registry.operation(second,4,0,"",path("threaded.rvg"));auto expected=MinimalGuest::from_snapshot(bytes(path("current.rvg")));expected.run(1000);expected.run(1000);
 check(bytes(path("threaded.rvg"))==expected.snapshot(),"Concurrent registry requests lost guest updates");registry.operation(second,6,0,"","",2);registry.close(second);
 rejects([&]{registry.operation(999999,0);});
 std::cout<<"{\"checks\":"<<checks<<",\"same_original_rvg\":true,\"no_reload_between_blocks\":true,\"durability_acknowledged_after_publication\":true,\"failed_write_preserves_live_state\":true,\"stale_handles_rejected\":true,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
