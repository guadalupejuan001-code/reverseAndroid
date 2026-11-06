#pragma once
// Optional in-process bridge. The complete immutable original VM is retained;
// this API is not a source-admission gate or an Android graphics driver.
#include "minimal_guest.hpp"
#include "qssd/state_bus.hpp"
#include <atomic>
#include <mutex>

namespace reverse_pc::integration {
class GpuStateBridge {
  using Guest=guest::MinimalGuest;
  struct Root {
    std::shared_ptr<const Guest> vm;
    std::shared_ptr<std::mutex> observations=std::make_shared<std::mutex>();
  };
  using Bus=qssd::StateBus<Root>;
  Bus bus_;
  static Root retain(const Guest& vm){return {std::make_shared<const Guest>(vm)};}
 public:
  using View=typename Bus::View;
  class Surface {
    friend class GpuStateBridge;
    View view_;
    unsigned slot_;
    std::shared_ptr<std::atomic<std::uint64_t>> words_=std::make_shared<std::atomic<std::uint64_t>>(0);
    Surface(View view,unsigned slot):view_(std::move(view)),slot_(slot){}
   public:
    const guest::GuestGpuResource& descriptor()const{return view_->value->vm->machine().pci.resources[slot_];}
    std::uint64_t revision()const{return view_->serial;}
    std::uint64_t authority()const{return view_->authority;}
    std::uint64_t instructions()const{return view_->value->vm->registers().instructions;}
    std::uint32_t word(std::uint64_t index)const{
      std::lock_guard<std::mutex> lock(*view_->value->observations);
      const auto value=view_->value->vm->gpu_resource_word(descriptor().id,index);
      ++*words_;return value;
    }
    std::uint64_t projected_bytes()const{return words_->load()*4;}
  };
  explicit GpuStateBridge(const Guest& initial,std::size_t history=2)
    :bus_(retain(initial),history,[ram=initial.logical_ram(),image=initial.gpu_bridge_image_identity(),
                                  clock=initial.machine().counter_latency](const Root& root){
      if(!root.vm||root.vm->logical_ram()!=ram||root.vm->gpu_bridge_image_identity()!=image)
        throw std::invalid_argument("CPU/GPU bridge epoch changed");
      const auto& c=root.vm->machine().counter_latency;
      if(c.enabled!=clock.enabled||c.seed!=clock.seed||c.instructions_per_tick!=clock.instructions_per_tick)
        throw std::invalid_argument("CPU/GPU bridge clock profile changed");
    }){}
  View snapshot()const{return bus_.snapshot();}
  View publish(const Guest& next,const View& expected,std::string reason){
    return bus_.publish(retain(next),std::move(reason),expected);
  }
  Surface acquire(unsigned resource,const View& retained={})const{
    auto view=retained?retained:bus_.snapshot();
    if(view->authority!=bus_.snapshot()->authority)throw std::invalid_argument("Foreign GPU bridge root");
    const auto& resources=view->value->vm->machine().pci.resources;
    for(unsigned i=0;i<resources.size();++i)if(resource&&resources[i].id==resource)return Surface(std::move(view),i);
    throw std::out_of_range("GPU resource absent in this revision");
  }
  guest::Bytes native_snapshot(const View& retained={})const{
    const auto view=retained?retained:bus_.snapshot();
    if(view->authority!=bus_.snapshot()->authority)throw std::invalid_argument("Foreign GPU bridge root");
    std::lock_guard<std::mutex> lock(*view->value->observations);
    return view->value->vm->snapshot();
  }
};
} // namespace reverse_pc::integration
