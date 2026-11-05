#pragma once
// Host lifetime and persistence around the original MinimalGuest authority.
// RVG, XI/Ether, instruction semantics and the guest clock are unchanged.
#include "guest_file_session.hpp"
#include "guest_incremental_journal.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>

namespace reverse_pc::guest {
class GuestResidentSession {
 public:
  using Clock=std::function<std::uint64_t()>;
  struct Policy {std::uint64_t wall_ms=30000,instructions=50000000;};
  GuestResidentSession(MinimalGuest guest,std::string source,std::size_t bytes,
                       Policy policy,Clock clock)
      :guest_(std::move(guest)),checkpoint_guest_(guest_),source_(std::move(source)),bytes_(bytes),policy_(policy),clock_(std::move(clock)){
    require(policy_.wall_ms&&policy_.instructions&&bool(clock_),"Invalid resident checkpoint policy");
    saved_at_=clock_();saved_instructions_=guest_.registers().instructions;
  }
  static std::uint64_t host_ms(){return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
  static std::unique_ptr<GuestResidentSession> open(const std::string& source,
      std::shared_ptr<const GuestBlockImage> disk={},std::size_t capacity=MinimalGuest::max_adaptive_snapshot){
    require(capacity>=4096&&capacity<=MinimalGuest::max_explicit_adaptive_snapshot,"Invalid resident transport capacity");
    auto journal=std::make_unique<GuestIncrementalJournal>(source,capacity);
    const bool recovered=journal->recover();auto bytes=recovered?journal->recovered_bytes():guest_read_file(source,capacity);const auto size=bytes.size();
    auto guest=MinimalGuest::from_snapshot(bytes,std::move(disk),capacity);
    auto result=std::make_unique<GuestResidentSession>(std::move(guest),source,size,Policy{},host_ms);
    result->recovered_entries_=journal->entries();result->journal_=std::move(journal);return result;
  }
  bool dirty()const noexcept{return dirty_;}
  std::uint64_t revision()const noexcept{return revision_;}
  bool has_journal()const noexcept{return journal_&&journal_->entries();}
  std::string inspect()const{return report(guest_,revision_,dirty_);}
  std::string run(std::uint64_t steps){
    require(steps&&steps<=250000,"Guest block must be 1..250000 instructions");
    return mutate([&](MinimalGuest& guest){const auto run=guest.run(steps);
      return ",\"executed_this_run\":"+std::to_string(run.executed)+",\"stdout\":"+quote(run.output)+",\"stderr\":"+quote(run.error_output)+
        ",\"stop_reason\":"+quote(run.halted?"halted":run.waiting?"waiting_for_device_or_interrupt":"instruction_budget");});
  }
  std::string xi(const std::string& source){
    require(source.size()<=65536,"Guest XI program exceeds 64 KiB");
    return mutate([&](MinimalGuest& guest){return ",\"xi_output\":"+quote(guest.apply_xi_delta(source));});
  }
  std::string input(const std::string& text){
    require(text.size()<=4096,"Guest console input exceeds 4096 bytes");
    return mutate([&](MinimalGuest& guest){guest.console_input(text);return std::string{};});
  }
  std::string stage(const std::string& candidate){
    require(!candidate.empty()&&std::filesystem::weakly_canonical(candidate)!=std::filesystem::weakly_canonical(source_),"Guest output must be a separate candidate");
    staged_=false;const auto size=guest_write_snapshot_candidate(guest_,candidate);
    auto text=report(guest_,revision_,dirty_,",\"staged_revision\":"+std::to_string(revision_)+",\"staged_bytes\":"+std::to_string(size));
    staged_bytes_=size;staged_revision_=revision_;staged_=true;return text;
  }
  std::string acknowledge(std::uint64_t revision){
    require(staged_&&revision==staged_revision_&&revision==revision_,"Stale or missing checkpoint acknowledgement");
    // The caller acknowledges only after AtomicFile publication has succeeded.
    auto saved=guest_;const auto now=clock_();
    if(journal_){journal_->sync_published_base_directory();journal_->retire();}checkpoint_guest_=std::move(saved);recovered_entries_=0;
    bytes_=staged_bytes_;saved_at_=now;saved_instructions_=guest_.registers().instructions;dirty_=false;staged_=false;
    return report(guest_,revision_,false,{},bytes_);
  }
  std::string checkpoint(){
    if(!dirty_)return inspect();auto saved=guest_;
    if(!journal_)journal_=std::make_unique<GuestIncrementalJournal>(source_,guest_.host_snapshot_transport_capacity());
    std::size_t size=0;
    try{size=journal_->commit(guest_,checkpoint_guest_);}
    catch(const GuestJournalCompaction&){return report(guest_,revision_,dirty_,",\"journal_compaction_required\":true");}
    checkpoint_guest_=std::move(saved);bytes_=size;saved_at_=clock_();saved_instructions_=guest_.registers().instructions;dirty_=false;staged_=false;
    return inspect();
  }
  std::string prepare_replacement(){
    require(!dirty_&&!has_journal(),"Compact the resident checkpoint before replacing it");
    if(journal_)journal_->retire();
    require(!std::filesystem::exists(source_+".journal"),"Cannot retire the old checkpoint journal");return inspect();
  }
  std::string display(const std::string& candidate)const{
    const auto rgb=guest_.display_rgb();
    if(!rgb.empty()){
      require(!candidate.empty()&&std::filesystem::weakly_canonical(candidate)!=std::filesystem::weakly_canonical(source_),"Frame output aliases checkpoint");
      std::ofstream out(candidate,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));out.close();require(bool(out),"Cannot write guest frame");
    }
    return report(guest_,revision_,dirty_,std::string(",\"display_ready\":")+(rgb.empty()?"false":"true")+",\"frame_bytes\":"+std::to_string(rgb.size()));
  }
 private:
  bool due(const MinimalGuest& guest,bool dirty)const{
    if(!dirty)return false;const auto now=clock_(),instructions=guest.registers().instructions;
    return (now>=saved_at_&&now-saved_at_>=policy_.wall_ms)||
      (instructions>=saved_instructions_&&instructions-saved_instructions_>=policy_.instructions);
  }
  std::string report(const MinimalGuest& guest,std::uint64_t revision,bool dirty,const std::string& extra={},std::size_t saved_bytes=0)const{
    std::string out="{\"guest\":"+guest.statistics()+",\"resident\":true,\"checkpoint_revision\":"+std::to_string(revision)+
      ",\"checkpoint_dirty\":"+(dirty?"true":"false")+",\"checkpoint_due\":"+(due(guest,dirty)?"true":"false")+
      ",\"last_checkpoint_bytes\":"+std::to_string(saved_bytes?saved_bytes:bytes_)+
      ",\"journal_entries\":"+std::to_string(journal_?journal_->entries():0)+",\"journal_bytes\":"+std::to_string(journal_?journal_->journal_bytes():0)+
      ",\"journal_recovered_entries\":"+std::to_string(recovered_entries_)+",\"journal_rolled_back\":"+(journal_&&journal_->rolled_back()?"true":"false")+
      ",\"journal_cleanup_pending\":"+std::to_string(journal_?journal_->pending_cleanup():0);
    if(!dirty)out+=",\"snapshot_bytes\":"+std::to_string(saved_bytes?saved_bytes:bytes_);
    return out+extra+"}";
  }
  template<class Action>std::string mutate(Action action){
    require(revision_<std::uint64_t(INT64_MAX),"Resident checkpoint revision overflow");
    auto candidate=guest_;const auto extra=action(candidate);
    auto text=report(candidate,revision_+1,true,extra);
    guest_=std::move(candidate);++revision_;dirty_=true;staged_=false;return text;
  }
  MinimalGuest guest_,checkpoint_guest_;std::unique_ptr<GuestIncrementalJournal> journal_;unsigned recovered_entries_=0;
  std::string source_;std::size_t bytes_,staged_bytes_=0;
  Policy policy_;Clock clock_;std::uint64_t saved_at_=0,saved_instructions_=0,revision_=0,staged_revision_=0;
  bool dirty_=false,staged_=false;
};

// Opaque, non-reused handles. No Java long is interpreted as a native pointer.
// The registry serializes all operations, including close and acknowledgement.
class GuestResidentRegistry {
 public:
  std::uint64_t open(const std::string& source,std::shared_ptr<const GuestBlockImage> disk={}){
    std::lock_guard<std::mutex> lock(mutex_);require(next_<=std::uint64_t(INT64_MAX),"Resident handle space exhausted");
    auto guest=GuestResidentSession::open(source,std::move(disk));const auto id=next_;
    sessions_.emplace(id,std::move(guest));++next_;return id;
  }
  std::string operation(std::uint64_t id,unsigned operation,std::uint64_t steps=0,const std::string& text={},const std::string& candidate={},std::uint64_t revision=0){
    std::lock_guard<std::mutex> lock(mutex_);auto& session=find(id);
    switch(operation){
      case 0:return session.inspect();case 1:return session.run(steps);case 2:return session.xi(text);case 3:return session.input(text);
      case 4:return session.stage(candidate);case 5:return session.display(candidate);case 6:return session.acknowledge(revision);case 7:return session.checkpoint();case 8:return session.prepare_replacement();
      default:throw std::invalid_argument("Unsupported resident operation");
    }
  }
  void close(std::uint64_t id){std::lock_guard<std::mutex> lock(mutex_);require(!find(id).dirty(),"Save the resident guest before closing");sessions_.erase(id);}
 private:
  GuestResidentSession& find(std::uint64_t id){const auto at=sessions_.find(id);require(at!=sessions_.end(),"Invalid or closed resident handle");return *at->second;}
  std::mutex mutex_;std::map<std::uint64_t,std::unique_ptr<GuestResidentSession>> sessions_;std::uint64_t next_=1;
};
} // namespace reverse_pc::guest
