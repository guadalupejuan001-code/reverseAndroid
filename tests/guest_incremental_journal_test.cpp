#include "guest_incremental_journal.hpp"
#include "guest_file_session.hpp"
#include "guest_resident_session.hpp"
#include <iostream>
#include <sys/wait.h>
#include <signal.h>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0;
void check(bool value,const char* why){++checks;require(value,why);}
template<class F>void rejects(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,"Expected journal rejection");}
Bytes elf(){
 Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
 put(b,2,2);put(b,183,2);put(b,1,4);put(b,0x1000);put(b,64);put(b,0);put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
 put(b,1,4);put(b,7,4);put(b,0x100);put(b,0x1000);put(b,0x1000);put(b,16);put(b,16);put(b,0x100);b.resize(0x100);
 for(auto op:{0xd2860001u,0x91000400u,0xf9000020u,0x17fffffeu})put(b,op,4);return b;
}
void corrupt(const std::filesystem::path& p){std::fstream f(p,std::ios::binary|std::ios::in|std::ios::out);require(bool(f),"Missing corruption target");f.put('!');}
}
int main(int argc,char** argv){try{
 require(argc==2,"Expected temporary root");std::filesystem::create_directories(argv[1]);
 auto pattern=(std::filesystem::path(argv[1])/"run-XXXXXX").string();std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
 require(::mkdtemp(temporary.data())!=nullptr,"Cannot create isolated journal test directory");const auto root=std::filesystem::path(temporary.data());
 auto path=[&](const std::string& n){return (root/n).string();};
 MinimalGuest guest(128*1024*1024,0);guest.load_elf(elf());std::string fill;
 for(unsigned page=0;page<300;++page)for(unsigned byte=0;byte<128;++byte)fill+="mem_store "+std::to_string(0x100000+4096*page+byte)+" "+std::to_string(byte%251+1)+"\n";
 guest.apply_xi_delta(fill+"h 0\ncx 0 1\nmem_store 18446744073709551616 127\nmem_bind 40000 0\n");
 const auto original=guest.snapshot();guest_write_snapshot_candidate(guest,path("base.rvg"));
 GuestIncrementalJournal journal(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);auto previous=guest;
 guest.run(3000);guest.apply_xi_delta("mem_store 1052672 42\nmem_store 18446744073709551616 4\nmem_bind 50000 8\n");
 check(journal.commit(guest,previous)==guest.snapshot().size(),"Incremental size differs from original RVG");
 check(journal.entries()==1&&journal.journal_bytes()<original.size()/20,"One changed page rewrote unchanged pages");
 GuestIncrementalJournal stale_writer(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);rejects([&]{stale_writer.commit(guest,previous);});
 check(guest_read_file(path("base.rvg"),original.size())==original,"Journal changed its immutable base");
 check(journal.recovered_bytes()==guest.snapshot(),"First journal differs from original snapshot oracle");
 check(MinimalGuest::from_snapshot(journal.recovered_bytes()).snapshot()==guest.snapshot(),"Original decoder disagrees after recovery");
 auto first=guest;previous=guest;std::string erase;
 for(unsigned byte=0;byte<128;++byte)erase+="mem_store "+std::to_string(0x100000+byte)+" 0\n";
 guest.apply_xi_delta(erase+"mem_store 18446744073709551616 0\nmem_unbind 40000\nh 2\ncx 2 3\n");guest.run(1000);
 journal.commit(guest,previous);check(journal.recovered_bytes()==guest.snapshot(),"Deleted page/address/binding or quantum component was lost");
 auto second=guest;GuestIncrementalJournal reopened(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);
 check(reopened.recover()&&reopened.entries()==2,"Committed journal did not reopen");check(reopened.recovered_bytes()==second.snapshot(),"Reopened bytes differ");
 corrupt(path("base.rvg.journal/previous"));GuestIncrementalJournal bad_backup(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);
 check(bad_backup.recover()&&!bad_backup.rolled_back()&&bad_backup.recovered_bytes()==second.snapshot(),"Damaged backup falsely reported loss of the intact latest state");
 // Either verified pointer can reconstruct the base binding if its redundant
 // identity marker is damaged; this must not discard committed guest work.
 const auto identity_path=path("identity.rvg");guest_write_snapshot_candidate(first,identity_path);auto identity_next=first;identity_next.run(1);
 GuestIncrementalJournal identity_writer(identity_path,MinimalGuest::max_adaptive_snapshot);identity_writer.commit(identity_next,first);corrupt(identity_path+".journal/identity");
 GuestIncrementalJournal identity_recovered(identity_path,MinimalGuest::max_adaptive_snapshot);
 check(identity_recovered.recover()&&identity_recovered.recovered_bytes()==identity_next.snapshot(),"Damaged identity marker discarded a verified journal");
 // A complete payload without its head update is not selected.
 for(const auto* phase:{"payload_synced","generation_synced","before_head"}){
  const auto name=path(std::string("crash-")+phase+".rvg");guest_write_snapshot_candidate(first,name);
  GuestIncrementalJournal baseline(name,MinimalGuest::max_adaptive_snapshot);auto before=first,accepted=first;accepted.run(1000);baseline.commit(accepted,before);
  const auto child=::fork();require(child>=0,"Cannot fork journal crash witness");
  if(!child){GuestIncrementalJournal cut(name,MinimalGuest::max_adaptive_snapshot,[&](const std::string& at){if(at==phase)::kill(::getpid(),SIGKILL);});
    cut.recover();auto next=accepted;next.run(1000);cut.commit(next,accepted);::_exit(64);}
  int status=0;::waitpid(child,&status,0);check(WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL,"Crash witness missed publication phase");
  GuestIncrementalJournal restored(name,MinimalGuest::max_adaptive_snapshot);check(restored.recover()&&restored.recovered_bytes()==accepted.snapshot(),"Crash selected an unpublished generation");
  auto recovered=MinimalGuest::from_snapshot(restored.recovered_bytes()),old=recovered;recovered.run(1000);restored.commit(recovered,old);
  check(restored.recovered_bytes()==recovered.snapshot(),"Retry after an orphan failed or duplicated CPU work");
  bool orphan=false;for(const auto& item:std::filesystem::directory_iterator(name+".journal"))if(item.path().filename().string().compare(0,9,".pending-")==0)orphan=true;
  check(!orphan,"Retry retained an unselected incomplete generation");
 }
 // A torn selected tail recovers the preceding committed prefix.
 std::filesystem::resize_file(path("base.rvg.journal/generation-2/data"),7);
 GuestIncrementalJournal torn(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);
 check(torn.recover()&&torn.rolled_back()&&torn.entries()==1,"Torn tail did not select the last complete prefix");check(torn.recovered_bytes()==first.snapshot(),"Torn tail partially changed the recovered guest");
 auto from=MinimalGuest::from_snapshot(torn.recovered_bytes()),old=from;from.run(1000);torn.commit(from,old);
 GuestIncrementalJournal repaired(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);check(repaired.recover()&&repaired.recovered_bytes()==from.snapshot(),"Cannot durably replace a rejected tail");
 // A damaged head uses the independently synced previous pointer.
 corrupt(path("base.rvg.journal/head"));GuestIncrementalJournal bad_head(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);
 check(bad_head.recover()&&bad_head.entries()==1&&bad_head.recovered_bytes()==first.snapshot(),"Corrupt head selected unverified state");
 // A valid explicitly replaced RVG never receives an older journal overlay.
 guest_write_snapshot_candidate(second,path("base.rvg"));GuestIncrementalJournal replacement(path("base.rvg"),MinimalGuest::max_adaptive_snapshot);
 check(!replacement.recover(),"Journal replayed over a different base identity");
 auto replaced=second;replaced.run(1000);replacement.commit(replaced,second);check(replacement.recovered_bytes()==replaced.snapshot(),"Replacement did not establish a fresh base");
 // Bound storage and require compaction before overwriting a full chain.
 const auto bounded=path("bounded.rvg");guest_write_snapshot_candidate(first,bounded);GuestIncrementalJournal full(bounded,MinimalGuest::max_adaptive_snapshot);auto live=first;
 for(unsigned i=0;i<GuestIncrementalJournal::max_entries;++i){auto last=live;live.run(1);full.commit(live,last);}
 auto last=live;live.run(1);rejects([&]{full.commit(live,last);});check(full.entries()==GuestIncrementalJournal::max_entries,"Full journal advanced its head");
 guest_write_snapshot_candidate(live,bounded);full.retire();check(!std::filesystem::exists(bounded+".journal"),"Compaction left a selected old history");
 check(MinimalGuest::from_snapshot(guest_read_file(bounded,MinimalGuest::max_adaptive_snapshot)).snapshot()==live.snapshot(),"Compaction changed original RVG bytes");
 // The legacy bounded representation also reconstructs through its original decoder.
 MinimalGuest legacy(65536,65536);legacy.load_elf(elf());const auto legacy_path=path("legacy.rvg");guest_write_snapshot_candidate(legacy,legacy_path);auto legacy_before=legacy;legacy.run(1000);
 GuestIncrementalJournal legacy_journal(legacy_path,MinimalGuest::max_adaptive_snapshot);legacy_journal.commit(legacy,legacy_before);
 check(legacy_journal.recovered_bytes()==legacy.snapshot(),"Legacy MXIM spelling changed");
 // A blocked destination leaves the earlier selected state usable.
 const auto failed_path=path("failed.rvg");guest_write_snapshot_candidate(first,failed_path);GuestIncrementalJournal failed(failed_path,MinimalGuest::max_adaptive_snapshot);auto fail_next=first;fail_next.run(1);failed.commit(fail_next,first);auto retained=fail_next;fail_next.run(1);
 std::filesystem::rename(failed_path,failed_path+".backup");std::filesystem::create_directory(failed_path);rejects([&]{failed.commit(fail_next,retained);});
 std::filesystem::remove(failed_path);std::filesystem::rename(failed_path+".backup",failed_path);
 GuestIncrementalJournal after_failure(failed_path,MinimalGuest::max_adaptive_snapshot);check(after_failure.recover()&&after_failure.recovered_bytes()==retained.snapshot(),"Failed write discarded the last committed state");
 // Background changes delete every old page/binding without materializing RAM.
 const auto reset_path=path("reset.rvg");guest_write_snapshot_candidate(first,reset_path);auto reset=first;reset.apply_xi_delta("mem_reset 7\n");
 GuestIncrementalJournal reset_journal(reset_path,MinimalGuest::max_adaptive_snapshot);reset_journal.commit(reset,first);
 check(reset_journal.recovered_bytes()==reset.snapshot(),"Memory reset retained deleted compact records");
 // Integration: a clean, closed native handle restores journal progress exactly.
 const auto resident_path=path("resident.rvg");guest_write_snapshot_candidate(first,resident_path);auto expected=first;expected.run(1000);
 GuestResidentRegistry registry;const auto handle=registry.open(resident_path);registry.operation(handle,1,1000);const auto saved=registry.operation(handle,7);
 check(saved.find("\"checkpoint_dirty\":false")!=std::string::npos&&saved.find("\"journal_entries\":1")!=std::string::npos,"Resident checkpoint did not acknowledge its journal");
 rejects([&]{registry.operation(handle,8);});registry.close(handle);
 const auto reopened_handle=registry.open(resident_path);check(registry.operation(reopened_handle,0).find("\"journal_recovered_entries\":1")!=std::string::npos,"Resident open skipped journal recovery");
 const auto candidate=path("resident-export.rvg");registry.operation(reopened_handle,4,0,"",candidate);
 check(guest_read_file(candidate,MinimalGuest::max_adaptive_snapshot)==expected.snapshot(),"Resident recovery changed or repeated CPU work");
 std::filesystem::rename(candidate,resident_path);registry.operation(reopened_handle,6,0,"","",0);registry.operation(reopened_handle,8);registry.close(reopened_handle);
 check(!std::filesystem::exists(resident_path+".journal"),"Canonical publication did not retire its journal");
 // A failed detach is reported rather than falsely acknowledging retirement.
 const auto retirement_path=path("retirement.rvg");guest_write_snapshot_candidate(first,retirement_path);bool block_retirement=false;
 GuestIncrementalJournal retirement(retirement_path,MinimalGuest::max_adaptive_snapshot,[&](const std::string& phase){if(block_retirement&&phase=="before_retire")throw std::runtime_error("Injected retirement failure");});
 auto retirement_next=first;retirement_next.run(1);retirement.commit(retirement_next,first);guest_write_snapshot_candidate(retirement_next,retirement_path);
 block_retirement=true;rejects([&]{retirement.retire();});check(std::filesystem::exists(retirement_path+".journal"),"Failed retirement destroyed its recovery history");
 block_retirement=false;retirement.retire();check(!std::filesystem::exists(retirement_path+".journal"),"Retirement retry left a selected journal");
 std::cout<<"{\"checks\":"<<checks<<",\"same_original_rvg\":true,\"original_decoder\":true,\"changed_pages_only\":true,\"crash_recovery\":true,\"bounded_compaction\":true,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
