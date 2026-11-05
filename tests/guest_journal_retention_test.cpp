// Host filesystem failures after publication must not invalidate the guest's committed save.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "minimal_guest_pci_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
int main(){try{
 unsigned checks=0;auto check=[&](bool yes,const char* why){++checks;require(yes,why);};
 const auto root=std::filesystem::temp_directory_path()/("reverse-journal-retry-"+std::to_string(::getpid()));
 bool injected_missing=false,deny=false;
 BootCycleJournal journal(root,[&](const std::filesystem::path& path,std::error_code& error){
   if(path.filename()=="generation-1"&&!injected_missing){injected_missing=true;error=std::make_error_code(std::errc::no_such_file_or_directory);return;}
   if(deny){error=std::make_error_code(std::errc::permission_denied);return;}
   std::filesystem::remove_all(path,error);
 });
 MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(pci_graphics_witness_image(),{},"",{},true);BootCycleReport report;
 auto publish=[&]{guest.run(1);const auto before=guest.snapshot();journal.publish(guest,report);check(guest.snapshot()==before,"Publication or cleanup changed the guest");};
 publish();publish();publish();
 check(journal.sequence()==3&&journal.pending_cleanup()==1,"Cleanup error aborted publication or was lost");
 check(journal.cleanup_error()==std::errc::no_such_file_or_directory,"Missing-child diagnostic lost");
 check(std::filesystem::exists(root/"generation-1")&&std::filesystem::exists(root/"generation-3"),"Failed cleanup removed the wrong generation");
 auto validate=[&](unsigned generation){
   const auto path=root/("generation-"+std::to_string(generation));const auto b=guest_read_file((path/"checkpoint.rvg").string(),MinimalGuest::max_adaptive_snapshot);
   check(b==guest.snapshot()&&MinimalGuest::from_snapshot(b).snapshot()==b,"Published state cannot be restored exactly");
   const auto p=guest_read_file((root/"latest.json").string(),4096);const std::string pointer(p.begin(),p.end());
   check(pointer.find("generation-"+std::to_string(generation))!=std::string::npos&&pointer.find(GuestSha256::hash(b))!=std::string::npos,"Pointer no longer selects the fully committed state");
 };
 validate(3);publish();validate(4);
 check(journal.pending_cleanup()==0&&!journal.cleanup_error(),"Deferred cleanup was not retried");
 check(!std::filesystem::exists(root/"generation-1")&&!std::filesystem::exists(root/"generation-2"),"Deferred generations leaked after recovery");
 deny=true;publish();publish();validate(6);
 check(journal.pending_cleanup()==2&&journal.cleanup_error()==std::errc::permission_denied,"Persistent cleanup errors concealed or stopped valid saves");
 deny=false;publish();validate(7);
 check(journal.pending_cleanup()==0&&!journal.cleanup_error(),"Persistent cleanup backlog did not recover");
 for(unsigned n=1;n<=5;n++)check(!std::filesystem::exists(root/("generation-"+std::to_string(n))),"Obsolete generation remains after recovery");
 check(std::filesystem::exists(root/"generation-6")&&std::filesystem::exists(root/"generation-7"),"Recovery removed a retained generation");
 std::error_code cleanup;std::filesystem::remove_all(root,cleanup);check(!cleanup,"Fixture cleanup failed");
 std::cout<<"{\"checks\":"<<checks<<",\"missing_child_retried\":true,\"permission_error_recovered\":true,\"committed_state_preserved\":true,\"android_boot\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
