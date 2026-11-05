#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace reverse_pc::guest {
struct GuestVirtQueue {std::uint64_t desc=0,avail=0,used=0;unsigned size=256,enabled=0,last=0,used_index=0;};
struct GuestVirtDevice {
  std::uint64_t features=0;std::uint32_t bar=0;
  unsigned command=0,feature_select=0,driver_select=0,status=0,isr=0,queue_select=0;
  std::array<GuestVirtQueue,2> queues{};
};
struct GuestGpuResource {
  unsigned id=0,format=0,width=0,height=0;
  struct Entry {std::uint64_t address=0,length=0;};
  std::vector<Entry> backing;
};
struct GuestPciState {
  static constexpr std::uint64_t ecam=0x30000000,ecam_size=0x1000000,mmio=0x10000000,mmio_size=0x10000000;
  static constexpr std::uint64_t disk_delta=UINT64_C(1)<<60,gpu_pixels=UINT64_C(2)<<60;
  bool enabled=false;std::uint64_t disk_bytes=0,flushes=0;
  std::array<GuestVirtDevice,2> devices{};std::array<GuestGpuResource,4> resources{};
  unsigned scanout=0,scan_x=0,scan_y=0,scan_w=0,scan_h=0;
  unsigned cursor=0,cursor_x=0,cursor_y=0,hot_x=0,hot_y=0;
  GuestPciState(){devices[0].bar=0x10000000;devices[1].bar=0x10010000;}
  bool present(unsigned d) const{return enabled&&(d==1||disk_bytes);}
  bool irq(unsigned d) const{return present(d)&&devices[d].isr&&!(devices[d].command&1024);}
  std::vector<std::uint8_t> save() const {
    std::vector<std::uint8_t> b;auto put=[&](std::uint64_t v){for(unsigned i=0;i<8;i++)b.push_back(std::uint8_t(v>>(8*i)));};
    put(1);put(enabled);put(disk_bytes);put(flushes);
    for(auto& d:devices){put(d.features);put(d.bar);for(auto v:{d.command,d.feature_select,d.driver_select,d.status,d.isr,d.queue_select})put(v);for(auto& q:d.queues){put(q.desc);put(q.avail);put(q.used);put(q.size);put(q.enabled);put(q.last);put(q.used_index);}}
    for(auto& r:resources){put(r.id);put(r.format);put(r.width);put(r.height);put(r.backing.size());for(auto& e:r.backing){put(e.address);put(e.length);}}
    for(auto v:{scanout,scan_x,scan_y,scan_w,scan_h,cursor,cursor_x,cursor_y,hot_x,hot_y})put(v);return b;
  }
  static GuestPciState restore(const std::vector<std::uint8_t>& b){
    std::size_t at=0;auto take=[&](){if(at>b.size()||b.size()-at<8)throw std::runtime_error("Truncated PCI state");std::uint64_t v=0;for(unsigned i=0;i<8;i++)v|=std::uint64_t(b[at++])<<(8*i);return v;};
    auto small=[&](std::uint64_t max){auto v=take();if(v>max)throw std::runtime_error("Invalid PCI state field");return unsigned(v);};
    if(take()!=1)throw std::runtime_error("Unsupported PCI state version");GuestPciState p;p.enabled=small(1);p.disk_bytes=take();p.flushes=take();
    if(!p.enabled||p.disk_bytes>(UINT64_C(1)<<40)||p.disk_bytes%512)throw std::runtime_error("Invalid PCI disk extent");
    for(auto& d:p.devices){d.features=take();d.bar=std::uint32_t(small(UINT32_MAX));if(d.bar!=0xffffc000&&d.bar&&(d.bar<mmio||d.bar>mmio+mmio_size-0x4000||(d.bar&0x3fff)))throw std::runtime_error("Invalid PCI BAR");d.command=small(0x406);if(d.command&~0x406u)throw std::runtime_error("Invalid PCI command");d.feature_select=small(UINT32_MAX);d.driver_select=small(UINT32_MAX);d.status=small(255);d.isr=small(3);d.queue_select=small(65535);
      for(auto& q:d.queues){q.desc=take();q.avail=take();q.used=take();q.size=small(256);q.enabled=small(1);q.last=small(65535);q.used_index=small(65535);if(q.enabled&&(!q.size||(q.size&(q.size-1))))throw std::runtime_error("Invalid virtqueue size");}}
    for(auto& r:p.resources){r.id=small(UINT32_MAX);r.format=small(134);r.width=small(1280);r.height=small(1024);const auto n=small(2048);if(!r.id&&(r.width||r.height||n))throw std::runtime_error("Invalid empty GPU resource");if(r.id&&(!r.width||!r.height||!(r.format==1||r.format==2||r.format==67||r.format==134)))throw std::runtime_error("Unsupported GPU resource");for(unsigned i=0;i<n;i++){auto a=take(),l=take();if(!l||l>1024*1024)throw std::runtime_error("Invalid GPU backing");r.backing.push_back({a,l});}}
    p.scanout=small(UINT32_MAX);p.scan_x=small(1280);p.scan_y=small(1024);p.scan_w=small(1280);p.scan_h=small(1024);
    p.cursor=small(UINT32_MAX);p.cursor_x=small(UINT32_MAX);p.cursor_y=small(UINT32_MAX);p.hot_x=small(63);p.hot_y=small(63);
    for(unsigned i=0;i<p.resources.size();i++)for(unsigned j=0;j<i;j++)if(p.resources[i].id&&p.resources[i].id==p.resources[j].id)throw std::runtime_error("Duplicate GPU resource ID");
    bool scan=!p.scanout,cursor=!p.cursor;
    for(auto& r:p.resources){if(r.id==p.scanout&&p.scanout)scan=p.scan_x<=r.width&&p.scan_y<=r.height&&p.scan_w<=r.width-p.scan_x&&p.scan_h<=r.height-p.scan_y;if(r.id==p.cursor&&p.cursor)cursor=r.width<=64&&r.height<=64&&p.hot_x<r.width&&p.hot_y<r.height;}
    if(!scan||!cursor)throw std::runtime_error("Invalid scanout/cursor resource");
    if(at!=b.size())throw std::runtime_error("Trailing PCI state");return p;
  }
};
} // namespace reverse_pc::guest
