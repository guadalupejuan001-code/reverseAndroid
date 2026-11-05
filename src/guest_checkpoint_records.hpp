#pragma once
// Host journal records. snapshot(), snapshot_to() and from_snapshot() remain
// the independent canonical authorities; no CPU or quantum transition lives here.
#include "minimal_guest.hpp"
#include <map>

namespace reverse_pc::guest {
struct GuestCheckpointRecords {
  using Components=std::map<std::string,std::string>;
  static Components components(const MinimalGuest& g){
    Components out;out["C"]=g.runtime_.core_snapshot();
    const auto system=g.machine_.enabled?g.machine_.save():Bytes{},fp=g.floating_.save();
    out["S"]=std::string(system.begin(),system.end());out["F"]=std::string(fp.begin(),fp.end());
    Bytes h{'R','V','G',std::uint8_t(!fp.empty()?'3':g.machine_.enabled?'2':'1'),13,10,26,10};
    put(h,g.ram_);put(h,g.override_limit_);put(h,g.registers_.pc);put(h,g.registers_.sp);
    put(h,g.registers_.instructions);put(h,g.registers_.nzcv);put(h,g.registers_.halted?1:0);put(h,g.registers_.exit_code);
    for(auto value:g.registers_.x)put(h,value);
    put(h,g.image_bytes());put(h,out["C"].size());put(h,g.runtime_.memory().persistent_snapshot_size());
    if(g.machine_.enabled||!fp.empty())put(h,system.size());if(!fp.empty())put(h,fp.size());
    out["H"]=std::string(h.begin(),h.end());return out;
  }
  static std::size_t size(const MinimalGuest& g,const Components& c){
    const auto capacity=g.override_limit_?MinimalGuest::max_snapshot:g.adaptive_snapshot_capacity_;
    std::size_t total=8;
    for(const auto length:{c.at("H").size(),g.image_bytes(),c.at("C").size(),
        g.runtime_.memory().persistent_snapshot_size(),c.at("S").size(),c.at("F").size()}){
      require(total<=capacity&&length<=capacity-total,"Guest snapshot exceeds configured host transport capacity");total+=length;
    }return total;
  }
  template<class Emit>static std::size_t patch(const MinimalGuest& now,const MinimalGuest& previous,Emit&& emit){
    require(now.image_==previous.image_&&now.override_limit_==previous.override_limit_,"Checkpoint backing/profile changed");
    const auto next=components(now),old=components(previous);const auto bytes=size(now,next);
    for(const auto& entry:next)if(entry.second!=old.at(entry.first))emit(entry.first,entry.second,false);
    now.runtime_.memory().checkpoint_patch_since(previous.runtime_.memory(),std::forward<Emit>(emit));return bytes;
  }
};
} // namespace reverse_pc::guest
