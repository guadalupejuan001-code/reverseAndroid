#pragma once
// The sole InverseMemory classical override index for REVERSE's opt-in
// adaptive profile. COW 4096-address pages are sparse until a threshold;
// larger exact canonical decimal addresses remain in the original map.
// Neither a second memory authority nor a simulated quantum memory.
#include "mundo_x/cow_sharded_map.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(REVERSE_PC_COMPACT_INDEX_LOOKASIDE) && REVERSE_PC_COMPACT_INDEX_LOOKASIDE
#include <atomic>
#endif

namespace mundo_x::memory_detail {
class AdaptiveOverrideMap {
  static constexpr std::size_t kPageSize=4096;
  static constexpr std::size_t kDenseThreshold=128, kSparseReturn=48;
  struct Dense {
    std::array<std::uint8_t,kPageSize> values{};
    std::array<std::uint64_t,kPageSize/64> mask{};
    std::size_t occupied=0;
    bool present(std::size_t at) const noexcept {
      return (mask[at/64] & (UINT64_C(1) << (at%64)))!=0;
    }
    void mark(std::size_t at) noexcept {mask[at/64]|=UINT64_C(1)<<(at%64);}
    void unmark(std::size_t at) noexcept {mask[at/64]&=~(UINT64_C(1)<<(at%64));}
  };
  struct Page {
    using Element=std::pair<std::uint16_t,std::uint8_t>;
    std::vector<Element> sparse;
    std::shared_ptr<Dense> dense;
    std::size_t size() const noexcept{return dense?dense->occupied:sparse.size();}
    static bool less_offset(const Element& e,std::uint16_t at){return e.first<at;}
    const std::uint8_t* get(std::size_t at) const {
      if(dense)return dense->present(at)?&dense->values[at]:nullptr;
      const auto p=std::lower_bound(sparse.begin(),sparse.end(),std::uint16_t(at),less_offset);
      return p!=sparse.end()&&p->first==at?&p->second:nullptr;
    }
    void insert(std::size_t at,std::uint8_t value){
      if(dense){
        if(dense->present(at))throw std::logic_error("Duplicate Ether dense override");
        auto& d=writable_dense();d.values[at]=value;d.mark(at);++d.occupied;return;
      }
      const auto p=std::lower_bound(sparse.begin(),sparse.end(),std::uint16_t(at),less_offset);
      if(p!=sparse.end()&&p->first==at)throw std::logic_error("Duplicate Ether sparse override");
      sparse.insert(p,{std::uint16_t(at),value});
      if(sparse.size()>=kDenseThreshold)promote();
    }
    void assign(std::size_t at,std::uint8_t value){
      if(dense){
        if(!dense->present(at))throw std::logic_error("Missing Ether dense override");
        writable_dense().values[at]=value;return;
      }
      const auto p=std::lower_bound(sparse.begin(),sparse.end(),std::uint16_t(at),less_offset);
      if(p==sparse.end()||p->first!=at)throw std::logic_error("Missing Ether sparse override");
      p->second=value;
    }
    void erase(std::size_t at){
      if(dense){
        if(!dense->present(at))throw std::logic_error("Missing Ether dense override");
        auto& d=writable_dense();d.unmark(at);--d.occupied;
        if(d.occupied<=kSparseReturn){
          std::vector<Element> restored;restored.reserve(d.occupied);
          for(std::size_t i=0;i<kPageSize;++i)if(d.present(i))
            restored.emplace_back(std::uint16_t(i),d.values[i]);
          sparse=std::move(restored);dense.reset();
        }
        return;
      }
      const auto p=std::lower_bound(sparse.begin(),sparse.end(),std::uint16_t(at),less_offset);
      if(p==sparse.end()||p->first!=at)throw std::logic_error("Missing Ether sparse override");
      sparse.erase(p);
    }
    template<class Fn>void for_each(Fn&& emit) const {
      if(dense){for(std::size_t i=0;i<kPageSize;++i)if(dense->present(i))emit(i,dense->values[i]);}
      else for(const auto& e:sparse)emit(e.first,e.second);
    }
   private:
    Dense& writable_dense(){
      if(!dense.unique())dense=std::make_shared<Dense>(*dense);
      return *dense;
    }
    void promote(){
      auto candidate=std::make_shared<Dense>();
      for(const auto& e:sparse){candidate->mark(e.first);candidate->values[e.first]=e.second;++candidate->occupied;}
      dense=std::move(candidate);sparse.clear();sparse.shrink_to_fit();
    }
  };
  struct State {
    std::map<std::uint64_t,std::shared_ptr<Page>> pages;
    CowShardedMap<std::uint8_t> large_addresses;
    std::size_t size=0;
#if defined(REVERSE_PC_COMPACT_INDEX_LOOKASIDE) && REVERSE_PC_COMPACT_INDEX_LOOKASIDE
    // Only dictionary lookup metadata. Values still come from the original
    // authoritative COW Page; neither bytes nor projections are cached.
    static std::uint64_t new_lookup_identity() noexcept {
      static std::atomic<std::uint64_t> next{1};
      auto id=next.load(std::memory_order_relaxed);
      while(id!=UINT64_MAX){
        if(next.compare_exchange_weak(id,id+1,std::memory_order_relaxed))return id;
      }
      return 0; // Exhaustion disables lookasides instead of reusing an identity.
    }
    const std::uint64_t lookup_identity=new_lookup_identity();
    std::uint64_t lookup_epoch=0;
    State()=default;
    State(const State& other):pages(other.pages),large_addresses(other.large_addresses),size(other.size){}
    void invalidate_lookups() noexcept {if(lookup_epoch!=UINT64_MAX)++lookup_epoch;}
#else
    void invalidate_lookups() noexcept {}
#endif
  };
 public:
  std::size_t size() const noexcept{return state_->size;}
  bool empty() const noexcept{return state_->size==0;}
  const std::uint8_t* get(const std::string& address) const {
    std::uint64_t numeric=0;
    if(!numeric_address(address,numeric))return state_->large_addresses.get(address);
    return get(numeric);
  }
  // A numeric spelling of the same canonical address in the same index.
  // It does not retain a projection or add another byte authority.
  const std::uint8_t* get(std::uint64_t numeric) const {
#if defined(REVERSE_PC_COMPACT_INDEX_LOOKASIDE) && REVERSE_PC_COMPACT_INDEX_LOOKASIDE
    if(state_->lookup_identity&&state_->lookup_epoch!=UINT64_MAX){
      struct Lookup {std::uint64_t identity=0,epoch=0,number=0;const Page* page=nullptr;};
      static thread_local std::array<Lookup,16> lookups{};
      const auto number=numeric>>12;
      auto& entry=lookups[(number^(number>>4))&15];
      if(entry.identity!=state_->lookup_identity||entry.epoch!=state_->lookup_epoch||entry.number!=number){
        const auto it=state_->pages.find(number);
        entry={state_->lookup_identity,state_->lookup_epoch,number,it==state_->pages.end()?nullptr:it->second.get()};
      }
      return entry.page?entry.page->get(std::size_t(numeric&4095)):nullptr;
    }
#endif
    const auto it=state_->pages.find(numeric>>12);
    return it==state_->pages.end()?nullptr:it->second->get(std::size_t(numeric&4095));
  }
  bool insert(const std::string& address,const std::uint8_t& value){
    if(get(address))return false;
    if(state_->size==std::numeric_limits<std::size_t>::max())
      throw std::length_error("Ether override count overflow");
    std::uint64_t numeric=0;
    if(!numeric_address(address,numeric)){
      auto& s=writable();
      if(!s.large_addresses.insert(address,value))return false;
      ++s.size;return true;
    }
    auto& s=writable();
    writable_page(s,numeric>>12).insert(std::size_t(numeric&4095),value);
    ++s.size;return true;
  }
  void assign_existing(const std::string& address,const std::uint8_t& value){
    if(!get(address))throw std::out_of_range("missing Ether Memory entry");
    std::uint64_t numeric=0;
    if(!numeric_address(address,numeric)){
      writable().large_addresses.assign_existing(address,value);return;
    }
    auto& s=writable();writable_page(s,numeric>>12).assign(std::size_t(numeric&4095),value);
  }
  bool erase(const std::string& address){
    if(!get(address))return false;
    std::uint64_t numeric=0;
    if(!numeric_address(address,numeric)){
      auto& s=writable();if(!s.large_addresses.erase(address))return false;
      --s.size;return true;
    }
    auto& s=writable();const auto page_number=numeric>>12;
    auto& page=writable_page(s,page_number);page.erase(std::size_t(numeric&4095));
    --s.size;if(!page.size()){s.invalidate_lookups();s.pages.erase(page_number);}
    return true;
  }
  // Numeric entry points address the same canonical keys and COW pages.
  bool insert(std::uint64_t address,const std::uint8_t& value){
    if(get(address))return false;
    if(state_->size==std::numeric_limits<std::size_t>::max())throw std::length_error("Ether override count overflow");
    auto& s=writable();writable_page(s,address>>12).insert(std::size_t(address&4095),value);++s.size;return true;
  }
  void assign_existing(std::uint64_t address,const std::uint8_t& value){
    if(!get(address))throw std::out_of_range("missing Ether Memory entry");
    auto& s=writable();writable_page(s,address>>12).assign(std::size_t(address&4095),value);
  }
  bool erase(std::uint64_t address){
    if(!get(address))return false;
    auto& s=writable();const auto page_number=address>>12;auto& page=writable_page(s,page_number);page.erase(std::size_t(address&4095));--s.size;
    if(!page.size()){s.invalidate_lookups();s.pages.erase(page_number);}return true;
  }
  template<class Fn>void for_each_sorted(Fn&& emit) const {
    for(const auto& entry:state_->pages){
      const auto base=entry.first<<12;
      entry.second->for_each([&](std::size_t offset,std::uint8_t value){
        emit(std::pair<std::string,std::uint8_t>{std::to_string(base+offset),value});
      });
    }
    // All canonical numbers larger than uint64_t sort after numeric pages.
    state_->large_addresses.for_each_sorted(emit);
  }
  template<class Fn>void for_each_page_record(Fn&& emit) const {
    for(const auto& entry:state_->pages){
      std::string bitmap(kPageSize/8,'\0'),values;
      values.reserve(entry.second->size());
      entry.second->for_each([&](std::size_t at,std::uint8_t value){
        bitmap[at/8]=char(static_cast<unsigned char>(bitmap[at/8])|(1u<<(at%8)));
        values.push_back(char(value));
      });
      emit(entry.first,bitmap,values);
    }
  }
  // Host checkpoint metadata: unchanged COW leaves are never re-encoded.
  template<class Fn>void for_each_page_extent(Fn&& emit) const {
    for(const auto& entry:state_->pages)emit(entry.first,entry.second->size());
  }
  template<class Fn>void for_each_changed_page_record(const AdaptiveOverrideMap& previous,Fn&& emit) const {
    auto old=previous.state_->pages.begin(),now=state_->pages.begin();
    while(old!=previous.state_->pages.end()||now!=state_->pages.end()){
      if(now==state_->pages.end()||(old!=previous.state_->pages.end()&&old->first<now->first)){
        emit(old->first,std::string{},std::string{},true);++old;continue;
      }
      const bool same=old!=previous.state_->pages.end()&&old->first==now->first;
      if(!same||old->second!=now->second){
        std::string bitmap(kPageSize/8,'\0'),values;values.reserve(now->second->size());
        now->second->for_each([&](std::size_t at,std::uint8_t value){
          bitmap[at/8]=char(static_cast<unsigned char>(bitmap[at/8])|(1u<<(at%8)));values.push_back(char(value));
        });
        emit(now->first,bitmap,values,false);
      }
      if(same)++old;++now;
    }
  }
  template<class Fn>void for_each_large_address(Fn&& emit) const {
    state_->large_addresses.for_each_sorted(std::forward<Fn>(emit));
  }
  void load_page_record(std::uint64_t page_number,const std::string& bitmap,const std::string& values){
    if(page_number>(UINT64_MAX>>12)||bitmap.size()!=kPageSize/8)
      throw std::invalid_argument("Invalid adaptive Ether page extent");
    Page page;std::size_t offset=0;
    for(std::size_t i=0;i<kPageSize;++i)if((static_cast<unsigned char>(bitmap[i/8])>>(i%8))&1u){
      if(offset>=values.size())throw std::invalid_argument("Truncated adaptive Ether page payload");
      page.insert(i,static_cast<std::uint8_t>(values[offset++]));}
    if(!page.size()||offset!=values.size())
      throw std::invalid_argument("Invalid adaptive Ether page length");
    auto& s=writable();
    if(s.pages.count(page_number))throw std::invalid_argument("Duplicate adaptive Ether page");
    if(s.size>std::numeric_limits<std::size_t>::max()-page.size())
      throw std::length_error("Ether override count overflow");
    s.size+=page.size();s.invalidate_lookups();s.pages.emplace(page_number,std::make_shared<Page>(std::move(page)));
  }
  std::size_t allocated_pages() const noexcept{return state_->pages.size();}
 private:
  std::shared_ptr<State> state_=std::make_shared<State>();
  static bool numeric_address(const std::string& address,std::uint64_t& result) noexcept {
    const auto* start=address.data();const auto* end=start+address.size();
    const auto parsed=std::from_chars(start,end,result);
    return parsed.ec==std::errc()&&parsed.ptr==end;
  }
  State& writable(){
    if(!state_.unique())state_=std::make_shared<State>(*state_);
    return *state_;
  }
  static Page& writable_page(State& s,std::uint64_t page_number){
    auto it=s.pages.find(page_number);
    if(it==s.pages.end()){s.invalidate_lookups();return *s.pages.emplace(page_number,std::make_shared<Page>()).first->second;}
    if(!it->second.unique()){s.invalidate_lookups();it->second=std::make_shared<Page>(*it->second);}
    return *it->second;
  }
};
} // namespace mundo_x::memory_detail
