// Protocol fixture only. These test pixels are never Android screen evidence.
#include "minimal_guest_pci_demo.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace reverse_pc::guest;
int main(int argc,char** argv){try{
  require(argc==3,"Expected two new fixture snapshot paths");
  const auto save=[](const char* path,const Bytes& bytes){require(!std::filesystem::exists(path),"Fixture file exists");std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));out.close();require(bool(out),"Cannot write fixture");};
  MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(pci_graphics_witness_image(),{},"",{},true);save(argv[2],guest.snapshot());guest.run(4000);require(guest.registers().halted,"Protocol fixture did not finish");save(argv[1],guest.snapshot());
  std::cout<<"{\"protocol_fixture_only\":true,\"android_boot_verified\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
