#include "reverseandroid_power_cycle.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace reverse_pc::guest;
int main(){try{
  const auto path=std::filesystem::path("build/reverseandroid/power-cycle-test.raw");
  {std::ofstream out(path,std::ios::binary);const Bytes b(131072,53);out.write(reinterpret_cast<const char*>(b.data()),b.size());}
  const auto disk=GuestBlockImage::open(path.string());Bytes kernel(4096);
  const auto field=[&](unsigned at,std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)kernel.at(at+i)=std::uint8_t(n>>(i*8));};
  field(0,0xd503201f,4);field(8,0x80000,8);field(16,kernel.size(),8);field(56,0x644d5241,4);
  MinimalGuest parent(128*1024*1024,0),fresh(128*1024*1024,0);
  for(auto* g:{&parent,&fresh})g->load_linux_image(kernel,{},"console=ttyAMA0",disk,true);
  std::string input;for(unsigned i=0;i<65001;++i)input+="mem_store "+std::to_string(GuestPciState::disk_delta+i)+" "+std::to_string(1+i%255)+"\n";
  input+="mem_store "+std::to_string(GuestPciState::disk_delta+131071)+" 17\n";
  input+="mem_store 1090519040 7\nmem_store "+std::to_string(GuestPciState::gpu_pixels)+" 9\n";parent.apply_xi_delta(input);
  const auto original=parent.snapshot_sha256();const auto proof=retain_persistent_drive(parent,fresh);
  require(proof.bytes==65002&&proof.pages==17,"Lost drive boundary or batch");
  require(parent.snapshot_sha256()==original,"Power cycle mutated parent");
  require(fresh.runtime().memory().read_numeric(1090519040,fresh.runtime().core()).value==0,"Old volatile RAM retained");
  require(fresh.runtime().memory().read_numeric(GuestPciState::gpu_pixels,fresh.runtime().core()).value==0,"Old GPU contents retained");
  for(const auto at:{0u,4095u,4096u,65000u,131071u}){
    const auto key=GuestPciState::disk_delta+at;
    require(parent.runtime().memory().read_numeric(key,parent.runtime().core()).value==fresh.runtime().memory().read_numeric(key,fresh.runtime().core()).value,"Drive byte changed");
  }
  require(fresh.registers().instructions==0,"Copy advanced CPU");
  bool rejected=false;try{retain_persistent_drive(parent,fresh);}catch(const std::exception&){rejected=true;}require(rejected,"Nonfresh recipient accepted");
  std::cout<<"{\"persistent_drive_copy\":true,\"volatile_ram_and_gpu_reset\":true,\"parent_unchanged\":true,\"batch_boundary_and_last_disk_byte\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
