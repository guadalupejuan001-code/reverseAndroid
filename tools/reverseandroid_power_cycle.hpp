#pragma once
// Explicit hardware power cycle support. The original guest, XI/Ether memory,
// media, codecs and CPU implementation remain the authorities.
#include "minimal_guest.hpp"
#include <charconv>
#include <sstream>

namespace reverse_pc::guest {
struct PersistentDriveProof {
  std::uint64_t pages=0,bytes=0;
  std::string sha256;
};
template<class Record> PersistentDriveProof persistent_drive_records(const MinimalGuest& guest,Record record){
  const auto first=GuestPciState::disk_delta,last=first+guest.machine().pci.disk_bytes;
  require(guest.machine().pci.enabled&&guest.machine().pci.disk_bytes,"Missing persistent drive");
  const auto stats=guest.runtime().memory().stats();
  require(stats.background_byte==0&&!stats.quantum_bindings,"Power cycle requires the original classical zero-background drive");
  PersistentDriveProof proof;GuestDiskSha256 hash;
  guest.runtime().memory().persistent_snapshot_to([&](const char* text,std::size_t length){
    const std::string_view line(text,length);
    if(line.substr(0,2)!="P "){
      require(line.substr(0,6)=="MXIM2 ","Unsupported memory record during power cycle");return;
    }
    const auto end=line.find(' ',2);require(end!=std::string_view::npos,"Missing page address");
    std::uint64_t page=0;const auto parsed=std::from_chars(line.data()+2,line.data()+end,page);
    require(parsed.ec==std::errc{}&&parsed.ptr==line.data()+end&&page<=(UINT64_MAX>>12),"Invalid page address");
    const auto base=page<<12;if(base<first||base>=last)return;
    require(base+4096<=last,"Drive page exceeds media");
    const auto values=end+1+1024+1;
    require(length>=values+1&&line[end+1+1024]==' '&&line.back()=='\n'&&((length-values-1)&1)==0,"Invalid drive page extent");
    ++proof.pages;proof.bytes+=(length-values-1)/2;hash.update(reinterpret_cast<const std::uint8_t*>(text),length);
    record(base,line.substr(end+1,1024),line.substr(values,length-values-1));
  });
  proof.sha256=hash.finish();return proof;
}
inline PersistentDriveProof retain_persistent_drive(const MinimalGuest& parent,MinimalGuest& fresh){
  require(parent.machine().pci.disk_bytes==fresh.machine().pci.disk_bytes,"Power cycle changed disk size");
  require(fresh.registers().instructions==0&&fresh.runtime().memory().stats().overrides==0,"Power cycle requires a fresh CPU and volatile memory");
  std::string program;program.reserve(2400000);std::size_t commands=0;
  const auto flush=[&](){if(commands){fresh.apply_xi_delta(program);program.clear();commands=0;}};
  const auto nibble=[](char c)->unsigned{require((c>='0'&&c<='9')||(c>='a'&&c<='f'),"Noncanonical drive hex");return c<='9'?unsigned(c-'0'):unsigned(c-'a'+10);};
  const auto byte=[&](std::string_view text,std::size_t at){return (nibble(text[at])<<4)|nibble(text[at+1]);};
  const auto original=persistent_drive_records(parent,[&](std::uint64_t base,std::string_view bitmap,std::string_view values){
    std::size_t used=0;
    for(unsigned index=0;index<512;++index){const auto mask=byte(bitmap,index*2);
      for(unsigned bit=0;bit<8;++bit)if(mask&(1u<<bit)){
        require(used+2<=values.size(),"Truncated drive values");const auto value=byte(values,used);used+=2;
        require(value!=0,"Noncanonical zero drive override");
        program+="mem_store "+std::to_string(base+index*8+bit)+" "+std::to_string(value)+"\n";
        if(++commands==65000)flush();
      }
    }
    require(used==values.size(),"Drive bitmap/value disagreement");
  });flush();
  const auto retained=persistent_drive_records(fresh,[](auto,auto,auto){});
  require(original.pages==retained.pages&&original.bytes==retained.bytes&&original.sha256==retained.sha256,"Persistent drive changed during power cycle");
  require(fresh.runtime().memory().stats().overrides==retained.bytes,"Volatile memory leaked across power cycle");
  return retained;
}
} // namespace reverse_pc::guest
