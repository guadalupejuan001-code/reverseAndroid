#pragma once
// Host controls only. All CPU, RAM, block and display transitions still belong
// to the existing MinimalGuest / original XI/Ether transactions.
#include "minimal_guest.hpp"
#include "guest_boot_resume_proof.hpp"
#include <chrono>
#include <functional>
#include <filesystem>
#include <fstream>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace reverse_pc::guest {
struct BootCycleLimits {
  std::uint64_t instructions=3000000000, wall_ms=1200000, chunk=50000;
  std::uint64_t checkpoint_ms=60000, checkpoint_instructions=100000000;
  bool stop_after_first_user_instruction=false;
  bool stop_before_user_undefined=false;
};
struct BootCycleReport {
  std::uint64_t initial_instructions=0, elapsed_ms=0, chunks=0;
  std::uint64_t entry_instruction_faults=0,entry_interrupts=0;
  std::string reason="running", error, console;
  bool first_user_instruction_verified=false;
  bool prior_user_instruction_verified=false;
  std::string verified_entry_checkpoint_sha256;
};
using CycleSave=std::function<void(const MinimalGuest&,const BootCycleReport&)>;
using UserEntry=std::function<bool(MinimalGuest&)>;

// Entry code may be demand-paged. The native CPU, not the observer, must
// raise that exception and let the guest kernel populate its own mappings.
// Commit only the exact architectural exception transition, never a guessed
// page, instruction, permission or instruction pointer.
inline RunResult boot_cycle_entry_event(MinimalGuest& guest,const MachineFault* fault){
  require(guest.machine().enabled&&!guest.machine().el&&!guest.machine().waiting,"Entry exception requires an active EL0 boundary");
  const auto pc=guest.registers().pc,count=guest.registers().instructions,exceptions=guest.machine().exceptions;
  if(fault){
    require(fault->syndrome>=UINT64_C(0x82000004)&&fault->syndrome<=UINT64_C(0x82000007)&&fault->address==pc,
            "Entry observation is not a lower-EL instruction translation fault");
    bool same_fault=false;
    try{guest.physical_address(pc,false,true);}
    catch(const MachineFault& actual){same_fault=actual.syndrome==fault->syndrome&&actual.address==fault->address;}
    require(same_fault,"Entry observation does not match the native instruction fetch");
  }
  else require(!(guest.machine().daif&0x80)&&guest.machine().next_irq()!=1023,"Entry interrupt is not pending");
  auto candidate=guest;const auto run=candidate.run(1);
  require(run.executed==1&&candidate.registers().instructions==count+1&&candidate.machine().el==1&&candidate.machine().exceptions==exceptions+1&&candidate.machine().elr==pc,"Entry event did not produce the expected native exception");
  require(candidate.registers().pc==guest.machine().vbar+(fault?0x400u:0x480u),"Native entry event selected a different exception vector");
  if(fault)require(candidate.machine().esr==fault->syndrome&&candidate.machine().far==fault->address,"Native entry fault differs from the observed translation fault");
  guest=std::move(candidate);return run;
}

// A failed run() rolls back the complete CPU/device/XI transaction. Retain
// the exact successful prefix, including its UART, and leave the faulting
// instruction unexecuted for diagnosis and subsequent repair.
inline RunResult boot_cycle_prefix(MinimalGuest& guest,std::uint64_t count,
                                   bool stop_el0,std::string& error,bool stop_undefined=false) {
  try {return guest.run(count,stop_el0,stop_undefined);}
  catch(const std::exception& failure) {
    error=failure.what();const auto before=guest;auto good=before;
    std::uint64_t low=0,high=count;RunResult prefix;
    while(low+1<high){const auto middle=low+(high-low)/2;auto trial=before;
      try{auto run=trial.run(middle,stop_el0,stop_undefined);low=middle;good=std::move(trial);prefix=std::move(run);}
      catch(const std::exception&){high=middle;}
    }
    auto rejected=good;bool failed=false;
    try{rejected.run(1,stop_el0,stop_undefined);}catch(const std::exception&){failed=true;}
    require(failed,"Could not localize the rejected CPU transition");
    guest=std::move(good);return prefix;
  }
}

inline BootCycleReport run_boot_cycle(MinimalGuest& guest,const BootCycleLimits& limit,
    const CycleSave& save={},const UserEntry& first_user={},
    const std::function<void(const MinimalGuest&,const BootCycleReport&)>& progress={},
    const std::function<bool()>& interrupted={},
    const BootCycleEntryProof* prior_entry=nullptr) {
  require(limit.instructions&&limit.instructions<=UINT64_C(8000000000),"Cycle instruction budget must be 1..8000000000");
  require(limit.wall_ms&&limit.wall_ms<=1500000,"Cycle wall budget must be 1..1500000 ms");
  require(limit.chunk&&limit.chunk<=1000000&&limit.checkpoint_ms&&limit.checkpoint_instructions,"Invalid cycle transaction/save cadence");
  require(!limit.stop_after_first_user_instruction||bool(first_user),"A userspace milestone stop requires its entry verifier");
  BootCycleReport report;report.initial_instructions=guest.registers().instructions;
  if(prior_entry){
    require(!limit.stop_after_first_user_instruction,"Cannot repeat the entry milestone during a proven continuation");
    report.verified_entry_checkpoint_sha256=boot_cycle_bound_entry_root(*prior_entry,
        GuestSha256::hash(guest.snapshot()),guest.registers().instructions,guest.registers().pc,
        guest.machine().enabled,guest.machine().el,guest.registers().halted,guest.machine().waiting);
    report.prior_user_instruction_verified=true;
  }
  require(report.initial_instructions<=UINT64_MAX-limit.instructions,"Instruction counter would overflow");
  const auto start=std::chrono::steady_clock::now();auto saved_at=start;
  auto saved_steps=report.initial_instructions;
  const auto tick=[&]{report.elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();};
  const auto output=[&](const RunResult& run){require(run.output.size()<=4*1024*1024-report.console.size(),"Cycle UART exceeds 4 MiB evidence bound");report.console+=run.output;};
  if(save)save(guest,report);
  while(true){
    tick();const auto steps=guest.registers().instructions-report.initial_instructions;
    if(guest.registers().halted){report.reason="halted";break;}
    if(interrupted&&interrupted()){report.reason="host_interrupted";break;}
    if(steps>=limit.instructions){report.reason="instruction_budget";break;}
    if(report.elapsed_ms>=limit.wall_ms){report.reason="host_wall_budget";break;}
    if(first_user&&!report.first_user_instruction_verified&&!report.prior_user_instruction_verified&&guest.machine().enabled&&!guest.machine().el){
      const auto count=guest.registers().instructions;
      if(!(guest.machine().daif&0x80)&&guest.machine().next_irq()!=1023){
        try{output(boot_cycle_entry_event(guest,nullptr));++report.entry_interrupts;++report.chunks;}
        catch(const std::exception& failure){report.reason="unverified_userspace_entry";report.error=failure.what();break;}
        catch(const MachineFault& failure){report.reason="unverified_userspace_entry";report.error="Native entry interrupt fault: "+hex(failure.syndrome);break;}
      }else{
      try{
        auto checked=guest;
        require(first_user(checked)&&checked.registers().instructions==count+1&&!checked.machine().el,"First userspace instruction was not verified");
        guest=std::move(checked);
        report.first_user_instruction_verified=true;
        if(limit.stop_after_first_user_instruction){report.reason="first_user_instruction_verified";break;}
      }catch(const std::exception& failure){report.reason="unverified_userspace_entry";report.error=failure.what();break;}
      catch(const MachineFault& failure){
        try{output(boot_cycle_entry_event(guest,&failure));++report.entry_instruction_faults;++report.chunks;}
        catch(const std::exception& rejected){report.reason="unverified_userspace_entry";report.error=rejected.what();break;}
        catch(const MachineFault& rejected){report.reason="unverified_userspace_entry";report.error="Native entry fault delivery failed: "+hex(rejected.syndrome);break;}
      }
      }
    }else{
      std::string failure;const auto count=std::min(limit.chunk,limit.instructions-steps);
      const auto run=boot_cycle_prefix(guest,count,bool(first_user)&&!report.first_user_instruction_verified&&!report.prior_user_instruction_verified,failure,limit.stop_before_user_undefined);
      output(run);++report.chunks;
      if(!failure.empty()){report.reason="architectural_stop";report.error=failure;break;}
      // WFI may be the final instruction of this transaction even though
      // the native machine can wake at its scheduled timer on the next run.
      // Ask that same native path again; only a zero-progress wait blocks.
      if(run.waiting&&!run.executed){report.reason="waiting_for_device_or_interrupt";break;}
      if(!run.executed&&!(first_user&&guest.machine().enabled&&!guest.machine().el)){
        report.reason="no_instruction_progress";break;
      }
    }
    if(report.console.find("Kernel panic - not syncing")!=std::string::npos){report.reason="guest_kernel_panic";report.error="Guest kernel reported a panic";break;}
    tick();if(progress)progress(guest,report);
    const auto now=std::chrono::steady_clock::now();
    if(save&&(now-saved_at>=std::chrono::milliseconds(limit.checkpoint_ms)||guest.registers().instructions-saved_steps>=limit.checkpoint_instructions)){
      save(guest,report);saved_at=std::chrono::steady_clock::now();saved_steps=guest.registers().instructions;
    }
  }
  tick();if(save)save(guest,report);return report;
}

inline std::string boot_cycle_json(const MinimalGuest& guest,const BootCycleReport& report,
                                  const std::string& sha,const std::string& provenance="{}") {
  std::ostringstream out;out<<"{\"initial_instructions\":"<<report.initial_instructions
    <<",\"advanced_instructions\":"<<guest.registers().instructions-report.initial_instructions
    <<",\"guest\":"<<guest.statistics()<<",\"stop_reason\":"<<quote(report.reason)
    <<",\"error\":"<<quote(report.error)<<",\"console\":"<<quote(report.console)
    <<",\"checkpoint_sha256\":"<<quote(sha)<<",\"host_elapsed_ms\":"<<report.elapsed_ms
    <<",\"completed_chunks\":"<<report.chunks
    <<",\"first_user_instruction_verified\":"<<(report.first_user_instruction_verified?"true":"false")
    <<",\"prior_user_instruction_verified\":"<<(report.prior_user_instruction_verified?"true":"false")
    <<",\"verified_entry_checkpoint_sha256\":"<<quote(report.verified_entry_checkpoint_sha256)
    <<",\"entry_instruction_faults\":"<<report.entry_instruction_faults<<",\"entry_interrupts\":"<<report.entry_interrupts
    <<",\"provenance\":"<<provenance<<"}";return out.str();
}

// Two complete immutable generations plus one atomic pointer. A crash during
// the next save cannot select an unfinished snapshot or mix state and report.
// Pruning is best effort after publication; failed removals are retried later.
// This Linux host utility is not part of Android's UI or its guest state.
class BootCycleJournal {
 public:
  using CleanupAction=std::function<void(const std::filesystem::path&,std::error_code&)>;
  explicit BootCycleJournal(std::filesystem::path root,CleanupAction cleanup={}):root_(std::move(root)),cleanup_(std::move(cleanup)){
    require(std::filesystem::create_directory(root_),"Cycle journal output must be a new directory");
    sync_directory(root_.parent_path().empty()?std::filesystem::path("."):root_.parent_path());
  }
  void publish(const MinimalGuest& guest,const BootCycleReport& report,const std::string& provenance="{}"){
    const auto number=sequence_+1;const auto name="generation-"+std::to_string(number);
    const auto pending=root_/(".pending-"+std::to_string(number)),target=root_/name;
    require(std::filesystem::create_directory(pending),"Cannot stage cycle generation");
    const auto sha=write_snapshot(pending/"checkpoint.rvg",guest);
    const auto evidence=boot_cycle_json(guest,report,sha,provenance);
    write(pending/"report.json",evidence.data(),evidence.size());sync_directory(pending);
    std::filesystem::rename(pending,target);sync_directory(root_);
    const std::string pointer="{\"generation\":"+quote(name)+",\"checkpoint_sha256\":"+quote(sha)+"}\n";
    const auto pointer_file=root_/(".latest-"+std::to_string(number));
    write(pointer_file,pointer.data(),pointer.size());
    std::filesystem::rename(pointer_file,root_/"latest.json");sync_directory(root_);sequence_=number;
    if(number>2)cleanup_pending_.push_back(root_/("generation-"+std::to_string(number-2)));
    cleanup_error_.clear();
    for(auto at=cleanup_pending_.begin();at!=cleanup_pending_.end();){
      std::error_code error;
      if(cleanup_)cleanup_(*at,error);else std::filesystem::remove_all(*at,error);
      if(error){cleanup_error_=error;++at;}else at=cleanup_pending_.erase(at);
    }
  }
  unsigned sequence()const{return sequence_;}
  std::size_t pending_cleanup()const{return cleanup_pending_.size();}
  const std::error_code& cleanup_error()const{return cleanup_error_;}
 private:
  static std::string write_snapshot(const std::filesystem::path& path,const MinimalGuest& guest){
    const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,"Cannot create checkpoint component");
    bool open=true;GuestSha256 sha;
    try{
      guest.snapshot_to([&](const std::uint8_t* bytes,std::size_t length){
        std::size_t done=0;
        while(done<length){const auto n=::write(fd,bytes+done,length-done);if(n<0&&errno==EINTR)continue;
          require(n>0,"Incomplete checkpoint component");done+=std::size_t(n);}
        sha.update(bytes,length);
      });
      const int synced=::fsync(fd),closed=::close(fd);open=false;
      require(synced==0&&closed==0,"Cannot persist checkpoint component");return sha.finish();
    }catch(...){if(open)::close(fd);throw;}
  }
  static void sync_directory(const std::filesystem::path& path){
    const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY);require(fd>=0,"Cannot open checkpoint directory");
    const int result=::fsync(fd);::close(fd);require(result==0,"Cannot sync checkpoint directory");
  }
  static void write(const std::filesystem::path& path,const void* data,std::size_t length){
    const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,"Cannot create checkpoint component");
    const auto* bytes=static_cast<const std::uint8_t*>(data);std::size_t done=0;
    while(done<length){const auto n=::write(fd,bytes+done,length-done);if(n<0&&errno==EINTR)continue;
      if(n<=0){::close(fd);throw std::runtime_error("Incomplete checkpoint component");}done+=std::size_t(n);}
    const int synced=::fsync(fd),closed=::close(fd);require(synced==0&&closed==0,"Cannot persist checkpoint component");
  }
  std::filesystem::path root_;unsigned sequence_=0;
  CleanupAction cleanup_;std::vector<std::filesystem::path> cleanup_pending_;std::error_code cleanup_error_;
};
} // namespace reverse_pc::guest

