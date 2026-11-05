// Own ECAM + modern virtio PCI. Protocol contracts: Linux v6.12 UAPI.
// All DMA, disk deltas and device pixels use the existing XI/Ether authority.
inline std::uint64_t MinimalGuest::pci_read(std::uint64_t address,unsigned size){
  auto& p=machine_.pci;require(size==1||size==2||size==4||size==8,"Invalid PCI access size");
  auto packed=[&](const Bytes& b,unsigned at){require(at<=b.size()&&size<=b.size()-at,"PCI register access crosses region");return get(b,at,size);};
  auto field=[](Bytes& b,unsigned at,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)b.at(at+i)=std::uint8_t(v>>(8*i));};
  if(address>=GuestPciState::ecam){
    const auto a=address-GuestPciState::ecam;const auto fn=unsigned(a>>12);const unsigned d=fn==8?0:fn==16?1:2,off=unsigned(a&4095);
    if(d==2||!p.present(d))return low_mask(size*8);
    auto& v=p.devices[d];Bytes b(4096);field(b,0,0x1af4,2);field(b,2,d?0x1050:0x1042,2);field(b,4,v.command,2);field(b,6,0x10|(p.irq(d)?8:0),2);
    field(b,8,d?0x03800001:0x01000001,4);field(b,0x10,v.bar,4);field(b,0x2c,0x1af4,2);field(b,0x2e,d?16:2,2);b[0x34]=0x40;b[0x3c]=std::uint8_t(48+d);b[0x3d]=1;
    const unsigned cap[4]={0x40,0x50,0x64,0x74};const unsigned types[4]={1,2,3,4};const unsigned offsets[4]={0,0x1000,0x2000,0x3000};
    for(unsigned i=0;i<4;i++){const auto c=cap[i];b[c]=9;b[c+1]=i==3?0:std::uint8_t(cap[i+1]);b[c+2]=i==1?20:16;b[c+3]=std::uint8_t(types[i]);field(b,c+8,offsets[i],4);field(b,c+12,i==2?1:i==3?(d?16:8):256,4);if(i==1)field(b,c+16,4,4);}
    return packed(b,off);
  }
  for(unsigned d=0;d<2;d++)if(p.present(d)){
    auto& v=p.devices[d];if(!(v.command&2)||address<v.bar||address-v.bar>=0x4000)continue;
    const auto off=unsigned(address-v.bar);
    if(off<256){Bytes b(256);const auto features=(UINT64_C(1)<<32)|(d?0:UINT64_C(1)<<9);const auto count=d?2u:1u;
      field(b,0,v.feature_select,4);field(b,4,v.feature_select<2?features>>(32*v.feature_select):0,4);field(b,8,v.driver_select,4);field(b,12,v.driver_select<2?v.features>>(32*v.driver_select):0,4);
      field(b,16,65535,2);field(b,18,count,2);b[20]=std::uint8_t(v.status);field(b,22,v.queue_select,2);
      if(v.queue_select<count){const auto& q=v.queues[v.queue_select];field(b,24,q.size,2);field(b,26,65535,2);field(b,28,q.enabled,2);field(b,30,v.queue_select,2);field(b,32,q.desc,8);field(b,40,q.avail,8);field(b,48,q.used,8);}return packed(b,off);
    }
    if(off==0x2000){const auto isr=v.isr;v.isr=0;return isr;}
    if(off>=0x3000&&off+size<=(d?0x3010u:0x3008u)){Bytes b(d?16:8);if(d)field(b,8,1,4);else field(b,0,p.disk_bytes/512,8);return packed(b,off-0x3000);}
    return 0;
  }
  return low_mask(size*8);
}
inline void MinimalGuest::pci_write(std::uint64_t address,std::uint64_t value,unsigned size){
  auto& p=machine_.pci;require(size==1||size==2||size==4||size==8,"Invalid PCI access size");
  if(address>=GuestPciState::ecam){
    const auto a=address-GuestPciState::ecam;const auto fn=unsigned(a>>12);const unsigned d=fn==8?0:fn==16?1:2,off=unsigned(a&4095);
    if(d==2||!p.present(d))return;auto& v=p.devices[d];
    if(off==4&&(size==2||size==4)){v.command=unsigned(value)&0x406;return;}
    if(off==0x10&&size==4){const auto bar=std::uint32_t(value)&0xffffc000;
      require(bar==0xffffc000||!bar||(bar>=GuestPciState::mmio&&bar<=GuestPciState::mmio+GuestPciState::mmio_size-0x4000),"PCI BAR outside declared aperture");v.bar=bar;return;}
    return; // Read-only configuration and unused BARs remain zero.
  }
  for(unsigned d=0;d<2;d++)if(p.present(d)){
    auto& v=p.devices[d];if(!(v.command&2)||address<v.bar||address-v.bar>=0x4000)continue;const auto off=unsigned(address-v.bar);
    if(off>=0x1000&&off<0x1100){require(size==2||size==4,"Virtqueue notify requires 16/32-bit access");pci_notify(d,unsigned(value)&65535);return;}
    if(off>=0x3000)return;
    if(off==0&&size==4){v.feature_select=std::uint32_t(value);return;}
    if(off==8&&size==4){v.driver_select=std::uint32_t(value);return;}
    if(off==12&&size==4){require(!(v.status&8),"Features changed after negotiation");if(v.driver_select<2){const auto shift=32*v.driver_select;v.features=(v.features&~(UINT64_C(0xffffffff)<<shift))|(std::uint64_t(std::uint32_t(value))<<shift);}return;}
    if(off==16||off==26)return; // No MSI-X capability is advertised.
    if(off==20&&size==1){const unsigned next=unsigned(value)&255;
      if(!next){const auto bar=v.bar,command=v.command;v=GuestVirtDevice{};v.bar=bar;v.command=command;
        if(d){for(unsigned i=0;i<p.resources.size();i++){const auto& r=p.resources[i];for(std::uint64_t j=0;j<std::uint64_t(r.width)*r.height*4;j++)logical_device_store(GuestPciState::gpu_pixels+(std::uint64_t(i)<<24)+j,0);}p.resources={};p.scanout=p.scan_w=p.scan_h=p.cursor=0;++p.flushes;}return;}
      v.status=next;const auto allowed=(UINT64_C(1)<<32)|(d?0:UINT64_C(1)<<9);
      if((next&8)&&((v.features&~allowed)||!(v.features&(UINT64_C(1)<<32))))v.status&=~8u;
      require(!(v.status&4)||(v.status&11)==11,"DRIVER_OK before successful feature negotiation");return;
    }
    if(off==22&&size==2){v.queue_select=unsigned(value)&65535;return;}
    if(v.queue_select>=(d?2u:1u))return;auto& q=v.queues[v.queue_select];
    if(off==24&&size==2){require(!q.enabled&&value&&value<=256&&!(value&(value-1)),"Invalid active virtqueue resize");q.size=unsigned(value);return;}
    if(off==28&&size==2){require(value==1&&!q.enabled&&(v.status&8),"Unsupported virtqueue enable/reset");require(!(q.desc&15)&&!(q.avail&1)&&!(q.used&3),"Unaligned virtqueue addresses");range(q.desc,q.size*16);range(q.avail,6+q.size*2);range(q.used,6+q.size*8);q.enabled=1;return;}
    if(off>=32&&off+size<=56){require(!q.enabled&&(size==4||size==8),"Active or invalid virtqueue address write");
      std::uint64_t* slot=off<40?&q.desc:off<48?&q.avail:&q.used;const unsigned sub=off<40?off-32:off<48?off-40:off-48;
      require((sub==0&&(size==4||size==8))||(sub==4&&size==4),"Virtqueue address write crosses register");const auto mask=low_mask(size*8)<<(8*sub);*slot=(*slot&~mask)|((value<<(8*sub))&mask);return;}
    return;
  }
}
inline void MinimalGuest::dma_write(std::uint64_t address,std::uint64_t value,unsigned size){
  range(address,size);invalidate_translation_hints();invalidate_dma_code(address,size);machine_.exclusive_address=UINT64_MAX;machine_.exclusive_size=0;
  for(unsigned i=0;i<size;i++)logical_device_store(address+i,std::uint8_t(value>>(8*i))^backing(address+i));
}
inline void MinimalGuest::pci_notify(unsigned device,unsigned queue){
  auto& p=machine_.pci;auto& v=p.devices[device];
  if((v.command&4)==0||(v.status&15)!=15||(v.status&0xc0))return;
  require(queue<(device?2u:1u),"Invalid virtqueue notification");auto& q=v.queues[queue];require(q.enabled,"Notify on disabled virtqueue");
  range(q.desc,q.size*16);range(q.avail,6+q.size*2);range(q.used,6+q.size*8);
  const unsigned available=unsigned(read(q.avail+2,2));const auto count=std::uint16_t(available-q.last);require(count<=q.size,"Virtqueue producer overran ring");
  struct Descriptor {std::uint64_t address;unsigned length,flags;};
  for(unsigned request=0;request<count;request++){
    const auto head=unsigned(read(q.avail+4+2*(q.last%q.size),2));unsigned index=head;std::array<bool,256> seen{};std::vector<Descriptor> chain;Bytes input;std::uint64_t writable=0;bool writing=false;
    for(;;){require(index<q.size&&!seen[index],"Cyclic or invalid virtqueue descriptor");seen[index]=true;
      const auto at=q.desc+index*16,address=read(at,8);const auto length=unsigned(read(at+8,4)),flags=unsigned(read(at+12,2));
      require(!(flags&~3u)&&length<=1024*1024,"Unsupported indirect or oversized descriptor");range(address,length);chain.push_back({address,length,flags});
      if(flags&2){writing=true;writable+=length;require(writable<=1024*1024,"Virtqueue response exceeds device budget");}
      else{require(!writing&&input.size()+length<=1024*1024,"Invalid virtqueue descriptor ordering or request budget");for(unsigned i=0;i<length;i++)input.push_back(read8(address+i));}
      if(!(flags&1))break;index=unsigned(read(at+14,2));
    }
    Bytes response;unsigned used=0;
    if(!device){
      require(input.size()>=16&&!chain.empty()&&(chain.back().flags&2)&&chain.back().length==1,"Invalid block request header/status");
      const auto type=unsigned(get(input,0,4));const auto sector=get(input,8,8);const auto data_size=writable-1;unsigned status=0;
      if(type==0||type==1){const auto length=type?input.size()-16:std::size_t(data_size);
        if(length%512||sector>p.disk_bytes/512||length>p.disk_bytes-sector*512)status=1;
        else{require(block_image_&&block_image_->size()==p.disk_bytes,"Disk input is not attached");const auto offset=sector*512;auto base=block_image_->read(offset,length);
          if(!type){require(input.size()==16,"Block read contains unexpected input payload");response.resize(length);for(std::size_t i=0;i<length;i++)response[i]=base[i]^logical_device_byte(GuestPciState::disk_delta+offset+i);}
          else{require(writable==1,"Block write has unexpected writable data");for(std::size_t i=0;i<length;i++)logical_device_store(GuestPciState::disk_delta+offset+i,input[16+i]^base[i]);}
        }
      }else if(type==4){require(input.size()==16&&writable==1,"Invalid flush request");}
      else if(type==8){if(data_size<20)status=1;else{const std::string serial="REVERSE-XI-DISK-0001";response.assign(serial.begin(),serial.end());}}
      else status=2;
      // Status occupies the final descriptor, including failed reads with no data.
      std::size_t copied=0;for(std::size_t c=0;c+1<chain.size();c++)if(chain[c].flags&2){for(unsigned i=0;i<chain[c].length&&copied<response.size();i++)dma_write(chain[c].address+i,response[copied++],1);}
      require(copied==response.size(),"Short block response descriptors");dma_write(chain.back().address,status,1);used=unsigned(copied+1);
    }else{
      require(input.size()>=24,"Truncated GPU control header");const auto type=unsigned(get(input,0,4)),flags=unsigned(get(input,4,4));
      require(!(flags&~1u)&&get(input,16,4)==0,"Unsupported GPU context/fence flags");
      auto need=[&](std::size_t n){require(input.size()>=n,"Truncated GPU command");};
      unsigned result=0x1100;response.resize(24);auto out=[&](unsigned at,std::uint64_t x,unsigned n){for(unsigned i=0;i<n;i++)response.at(at+i)=std::uint8_t(x>>(8*i));};
      auto resource=[&](unsigned id)->int{for(unsigned i=0;i<p.resources.size();i++)if(id&&p.resources[i].id==id)return int(i);return -1;};
      auto rect=[&](const GuestGpuResource& r){return get(input,24,4)<=r.width&&get(input,28,4)<=r.height&&get(input,32,4)<=r.width-get(input,24,4)&&get(input,36,4)<=r.height-get(input,28,4);};
      auto clear=[&](unsigned i){const auto& r=p.resources[i];for(std::uint64_t j=0;j<std::uint64_t(r.width)*r.height*4;j++)logical_device_store(GuestPciState::gpu_pixels+(std::uint64_t(i)<<24)+j,0);};
      auto present=[&](unsigned rx,unsigned ry,unsigned rw,unsigned rh){const int i=resource(p.scanout);if(i<0)return;const auto& r=p.resources[unsigned(i)];const auto left=std::max(rx,p.scan_x),top=std::max(ry,p.scan_y),right=std::min(rx+rw,p.scan_x+p.scan_w),bottom=std::min(ry+rh,p.scan_y+p.scan_h);for(unsigned y=top;y<bottom;y++)for(unsigned x=left;x<right;x++)for(unsigned c=0;c<4;c++)logical_device_store(GuestPciState::gpu_pixels+0x10000000+((std::uint64_t(y-p.scan_y)*p.scan_w+x-p.scan_x)*4+c),logical_device_byte(GuestPciState::gpu_pixels+(std::uint64_t(i)<<24)+((std::uint64_t(y)*r.width+x)*4+c)));};
      if(queue==1){ // Cursor queue is consumed without a response buffer.
        need(56);require(type==0x300||type==0x301,"Unsupported GPU cursor command");
        require(get(input,24,4)==0,"Invalid cursor scanout");p.cursor_x=unsigned(get(input,28,4));p.cursor_y=unsigned(get(input,32,4));
        if(type==0x300){const auto id=unsigned(get(input,40,4));const int i=resource(id);if(id){require(i>=0,"Invalid cursor resource");const auto& r=p.resources[unsigned(i)];require(r.width<=64&&r.height<=64&&get(input,44,4)<r.width&&get(input,48,4)<r.height,"Invalid cursor geometry");}p.cursor=id;p.hot_x=unsigned(get(input,44,4))&63;p.hot_y=unsigned(get(input,48,4))&63;}++p.flushes;response.clear();
      }else if(type==0x100){response.resize(24+16*24);result=0x1101;out(32,800,4);out(36,600,4);out(40,1,4);}
      else if(type==0x101){need(40);const auto id=unsigned(get(input,24,4)),format=unsigned(get(input,28,4)),w=unsigned(get(input,32,4)),h=unsigned(get(input,36,4));int slot=-1;for(unsigned i=0;i<p.resources.size();i++)if(!p.resources[i].id){slot=int(i);break;}
        if(!id||resource(id)>=0||!w||!h||w>1280||h>1024||!(format==1||format==2||format==67||format==134))result=0x1205;else if(slot<0)result=0x1201;else{auto& r=p.resources[unsigned(slot)];r.id=id;r.format=format;r.width=w;r.height=h;clear(unsigned(slot));}}
      else if(type==0x102){need(32);const auto id=unsigned(get(input,24,4));const int i=resource(id);if(i<0)result=0x1203;else{clear(unsigned(i));p.resources[unsigned(i)]={};if(p.cursor==id)p.cursor=0;if(p.scanout==id){p.scanout=p.scan_w=p.scan_h=0;++p.flushes;}}}
      else if(type==0x106){need(32);const int i=resource(unsigned(get(input,24,4)));const auto n=unsigned(get(input,28,4));if(i<0)result=0x1203;else if(n>2048||input.size()!=32+std::size_t(n)*16)result=0x1205;else{
        auto& r=p.resources[unsigned(i)];std::vector<GuestGpuResource::Entry> entries;std::uint64_t total=0;
        for(unsigned j=0;j<n;j++){
          const auto a=get(input,32+j*16,8),length=get(input,40+j*16,4);
          require(length,"GPU backing entry exceeds device budget");range(a,length);
          // Normalize contiguous guest extents to the existing snapshot's
          // <=1-MiB entries. The original decoder and 2048-entry bound stay.
          constexpr std::uint64_t chunk=1024*1024;
          require((length+chunk-1)/chunk<=2048-entries.size(),"GPU backing exceeds normalized entry budget");
          total+=length;
          for(std::uint64_t offset=0;offset<length;offset+=chunk)entries.push_back({a+offset,std::min(chunk,length-offset)});
        }
        if(total<std::uint64_t(r.width)*r.height*4)result=0x1205;else r.backing=std::move(entries);
      }}
      else if(type==0x107){need(32);const int i=resource(unsigned(get(input,24,4)));if(i<0)result=0x1203;else p.resources[unsigned(i)].backing.clear();}
      else if(type==0x103){need(48);const auto id=unsigned(get(input,44,4));const int i=resource(id);if(get(input,40,4))result=0x1202;else if(!id){p.scanout=p.scan_w=p.scan_h=0;++p.flushes;}else if(i<0)result=0x1203;else if(!rect(p.resources[unsigned(i)]))result=0x1205;else{p.scanout=id;p.scan_x=unsigned(get(input,24,4));p.scan_y=unsigned(get(input,28,4));p.scan_w=unsigned(get(input,32,4));p.scan_h=unsigned(get(input,36,4));present(p.scan_x,p.scan_y,p.scan_w,p.scan_h);++p.flushes;}}
      else if(type==0x105){need(56);const int i=resource(unsigned(get(input,48,4)));if(i<0)result=0x1203;else{const auto& r=p.resources[unsigned(i)];if(!rect(r)||r.backing.empty())result=0x1205;else{const auto offset=get(input,40,8),x=get(input,24,4),y=get(input,28,4),w=get(input,32,4),h=get(input,36,4);std::uint64_t total=0;for(auto& e:r.backing)total+=e.length;
          require(offset<=total&&(!h||!w||((h-1)*r.width*4+w*4<=total-offset)),"GPU transfer exceeds backing");
          for(std::uint64_t yy=0;yy<h;yy++){auto pos=offset+yy*r.width*4;std::size_t entry=0;while(entry<r.backing.size()&&pos>=r.backing[entry].length){pos-=r.backing[entry].length;++entry;}for(std::uint64_t xx=0;xx<w*4;xx++){require(entry<r.backing.size(),"Missing GPU backing byte");const auto a=r.backing[entry].address+pos;logical_device_store(GuestPciState::gpu_pixels+(std::uint64_t(i)<<24)+((y+yy)*r.width+x)*4+xx,read8(a));if(++pos==r.backing[entry].length){++entry;pos=0;}}}
        }}}
      else if(type==0x104){need(48);const int i=resource(unsigned(get(input,40,4)));if(i<0)result=0x1203;else if(!rect(p.resources[unsigned(i)]))result=0x1205;else if(p.scanout==get(input,40,4)){present(unsigned(get(input,24,4)),unsigned(get(input,28,4)),unsigned(get(input,32,4)),unsigned(get(input,36,4)));++p.flushes;}}
      else result=0x1200;
      if(!response.empty()){out(0,result,4);out(4,flags&1,4);out(8,get(input,8,8),8);require(response.size()<=writable,"Short GPU response descriptors");std::size_t copied=0;for(const auto& d:chain)if(d.flags&2)for(unsigned i=0;i<d.length&&copied<response.size();i++)dma_write(d.address+i,response[copied++],1);used=unsigned(copied);}
    }
    dma_write(q.used+4+8*(q.used_index%q.size),head,4);dma_write(q.used+8+8*(q.used_index%q.size),used,4);q.used_index=(q.used_index+1)&65535;dma_write(q.used+2,q.used_index,2);q.last=(q.last+1)&65535;
    if(!(read(q.avail,2)&1))v.isr|=1;
  }
}
inline Bytes MinimalGuest::display_rgb() const {
  const auto& p=machine_.pci;if(!p.enabled||!p.scanout||!p.scan_w||!p.scan_h)return {};
  unsigned format=0;for(const auto& r:p.resources)if(r.id==p.scanout)format=r.format;require(format,"Scanout references missing resource");
  Bytes out(std::size_t(p.scan_w)*p.scan_h*3);const bool bgr=format==1||format==2;
  for(std::size_t i=0;i<out.size()/3;i++)for(unsigned c=0;c<3;c++)out[3*i+c]=logical_device_byte(GuestPciState::gpu_pixels+0x10000000+4*i+(bgr?2-c:c));
  for(unsigned i=0;i<p.resources.size();i++){const auto& r=p.resources[i];if(!p.cursor||r.id!=p.cursor)continue;for(unsigned y=0;y<r.height;y++)for(unsigned x=0;x<r.width;x++){const auto xx=std::int64_t(p.cursor_x)+x-p.hot_x,yy=std::int64_t(p.cursor_y)+y-p.hot_y;if(xx<0||yy<0||xx>=p.scan_w||yy>=p.scan_h)continue;const auto base=GuestPciState::gpu_pixels+(std::uint64_t(i)<<24)+(std::uint64_t(y)*r.width+x)*4;const unsigned alpha=r.format==2||r.format==134?255:logical_device_byte(base+3);for(unsigned c=0;c<3;c++){auto& dst=out[(std::size_t(yy)*p.scan_w+std::size_t(xx))*3+c];const auto value=logical_device_byte(base+((r.format==1||r.format==2)?2-c:c));dst=std::uint8_t((value*alpha+dst*(255-alpha)+127)/255);}}}return out;
}
