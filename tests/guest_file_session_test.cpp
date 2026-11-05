#include "guest_file_session.hpp"
#include <filesystem>
#include <iostream>
using namespace reverse_pc::guest;
int main(int argc,char**argv){try{
  require(argc==2,"Expected a temporary test directory");
  const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
  auto path=[&](const char* name){return (root/name).string();};
  auto write=[](const std::string& name,const Bytes& bytes){std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"Fixture write failed");};
  unsigned checks=0;auto check=[&](bool ok,const char* message){++checks;require(ok,message);};
  auto fails=[&](auto action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,"Invalid guest file operation was accepted");};
  Bytes image(42031616);auto patch=[&](unsigned at,std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)image[at+i]=std::uint8_t(n>>(8*i));};
  patch(0,0xd503201f,4);patch(4,0xd503201f,4);patch(16,image.size(),8);patch(24,2,8);patch(56,0x644d5241,4);
  MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(image);image.clear();image.shrink_to_fit();
  const auto original=guest.snapshot();check(original.size()>32*1024*1024,"Fixture does not cover the old Android transport ceiling");write(path("large.rvg"),original);
  const auto inspected=guest_file_operation(path("large.rvg"),"",0);
  check(inspected.find("\"instructions\":0")!=std::string::npos,"File inspection changed the instruction count");
  check(guest_read_file(path("large.rvg"),MinimalGuest::max_adaptive_snapshot)==original,"Inspection modified its original checkpoint");
  guest_file_operation(path("large.rvg"),path("advanced.rvg"),1,1);guest.run(1);
  check(guest_read_file(path("advanced.rvg"),MinimalGuest::max_adaptive_snapshot)==guest.snapshot(),"File execution differs from direct original guest execution");
  guest_file_operation(path("advanced.rvg"),path("xi.rvg"),2,0,"mem_store 1073750016 33\n");guest.apply_xi_delta("mem_store 1073750016 33\n");
  check(guest_read_file(path("xi.rvg"),MinimalGuest::max_adaptive_snapshot)==guest.snapshot(),"File XI operation differs from the same original memory authority");
  guest_file_operation(path("xi.rvg"),path("input.rvg"),3,0,"hello\n");guest.console_input("hello\n");
  check(guest_read_file(path("input.rvg"),MinimalGuest::max_adaptive_snapshot)==guest.snapshot(),"File console input changed checkpoint semantics");
  const auto stable=guest_read_file(path("advanced.rvg"),MinimalGuest::max_adaptive_snapshot);
  fails([&]{guest_file_operation(path("large.rvg"),path("large.rvg"),1,1);});
  check(guest_read_file(path("large.rvg"),MinimalGuest::max_adaptive_snapshot)==original,"Rejected output alias modified the source");
  fails([&]{guest_file_operation(path("large.rvg"),path("advanced.rvg"),1,250001);});
  auto bad=original;bad.back()^=1;write(path("invalid.rvg"),bad);bad.clear();
  fails([&]{guest_file_operation(path("invalid.rvg"),path("advanced.rvg"),1,1);});
  check(guest_read_file(path("advanced.rvg"),MinimalGuest::max_adaptive_snapshot)==stable,"Rejected checkpoint changed an earlier valid candidate");
  {std::ofstream huge(path("oversize.rvg"),std::ios::binary);huge.seekp(MinimalGuest::max_adaptive_snapshot);huge.put(0);}
  fails([&]{guest_file_operation(path("oversize.rvg"),"",0);});
  std::cout<<"{\"checks\":"<<checks<<",\"checkpoint_bytes\":"<<original.size()<<",\"large_file_transport\":true,\"direct_state_identical\":true,\"android_boot\":false}\n";
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
