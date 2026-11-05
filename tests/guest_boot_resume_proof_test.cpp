#include "guest_boot_resume_proof.hpp"
#include <iostream>
#include <functional>
using namespace reverse_pc::guest;
static unsigned passed=0;
static void check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++passed;}
static void rejects(const std::function<void()>& f,const char* label){bool thrown=false;try{f();}catch(const std::runtime_error&){thrown=true;}check(thrown,label);}
int main(){try{
  const std::string root="4c20cd09e353f82b032c17636ae2d72cec6988df5a5b00afbd756464c4dcd575";
  const std::string c07="845fb40da060147a23449ecfd4e7a3d8b11def8d66696093c6e6728798b82694";
  const auto count=UINT64_C(6350795356),pc=UINT64_C(0xffffffc0800dfa28);
  const BootCycleEntryProof old{root,UINT64_C(6250795356),0x2a4004};
  const BootCycleEntryProof resumed{c07,count,pc,1,root};
  check(boot_cycle_bound_entry_root(old,root,old.instructions,old.pc,true,0,false,false)==root,"old C06 compatibility");
  check(boot_cycle_bound_entry_root(resumed,c07,count,pc,true,1,false,false)==root,"C07 must preserve C06 root");
  check(boot_cycle_bound_entry_root(resumed,c07,count,pc,true,1,false,false)!=c07,"resume is not a new first-entry proof");
  rejects([&]{boot_cycle_bound_entry_root(resumed,root,count,pc,true,1,false,false);},"wrong full snapshot");
  rejects([&]{boot_cycle_bound_entry_root(resumed,c07,count+1,pc,true,1,false,false);},"wrong count");
  rejects([&]{boot_cycle_bound_entry_root(resumed,c07,count,pc+4,true,1,false,false);},"wrong PC");
  rejects([&]{boot_cycle_bound_entry_root(resumed,c07,count,pc,false,1,false,false);},"machine disabled");
  rejects([&]{boot_cycle_bound_entry_root(resumed,c07,count,pc,true,1,true,false);},"halted source");
  rejects([&]{boot_cycle_bound_entry_root(resumed,c07,count,pc,true,1,false,true);},"waiting source");
  for(unsigned el:{0u,2u,3u})rejects([&]{boot_cycle_bound_entry_root(resumed,c07,count,pc,true,el,false,false);},"wrong EL");
  auto p=resumed;p.original_entry_checkpoint_sha256.clear();
  rejects([&]{boot_cycle_bound_entry_root(p,c07,count,pc,true,1,false,false);},"EL1 without EL0 root");
  p=resumed;p.exception_level=2;
  rejects([&]{boot_cycle_bound_entry_root(p,c07,count,pc,true,2,false,false);},"EL2 is unsupported");
  for(const std::string& bad:{std::string{},std::string(63,'a'),std::string(65,'a'),std::string(64,'G'),std::string(64,'A')}){
    p=resumed;p.checkpoint_sha256=bad;
    rejects([&]{boot_cycle_bound_entry_root(p,bad,count,pc,true,1,false,false);},"malformed bound hash");
    if(!bad.empty()){p=resumed;p.original_entry_checkpoint_sha256=bad;
      rejects([&]{boot_cycle_bound_entry_root(p,c07,count,pc,true,1,false,false);},"malformed original root");}
  }
  // An ordinary legacy proof cannot become an EL1 continuation by changing the VM.
  rejects([&]{boot_cycle_bound_entry_root(old,root,old.instructions,old.pc,true,1,false,false);},"legacy EL0 proof at EL1");
  std::cout<<"{\"suite\":\"host_resume_proof_gate\",\"passed\":"<<passed<<",\"guest_execution_performed\":false}\n";
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
