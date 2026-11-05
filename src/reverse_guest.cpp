#include "minimal_guest_demo.hpp"
#include "minimal_guest_system_demo.hpp"
#include "minimal_guest_fp_demo.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace fs=std::filesystem;
using namespace reverse_pc::guest;
namespace {
Bytes read_file(const fs::path& path,std::size_t limit) {
  const auto size=fs::file_size(path); require(size<=limit,"Input exceeds file budget");
  std::ifstream input(path,std::ios::binary); require(bool(input),"Cannot open input file");
  Bytes result(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(result.size()));
  require(input.gcount()==static_cast<std::streamsize>(result.size()),"Input file was truncated"); return result;
}
void save_file(const fs::path& path,const Bytes& bytes) {
  if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
  auto temporary=path; temporary+=".pending";
  std::ofstream output(temporary,std::ios::binary|std::ios::trunc); require(bool(output),"Cannot create guest state file");
  output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
  output.close(); require(bool(output),"Cannot write guest state file"); fs::rename(temporary,path);
}
std::uint64_t integer(const std::string& text) {
  require(!text.empty() && text.find_first_not_of("0123456789")==std::string::npos,"Expected a positive decimal integer");
  std::uint64_t value=0;
  for (char c:text) { require(value<=(UINT64_MAX-unsigned(c-'0'))/10,"Integer overflow");value=value*10+unsigned(c-'0'); }
  return value;
}
std::string elapsed(std::chrono::steady_clock::time_point start) {
  const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
  std::ostringstream out;out<<",\"host_elapsed_ns\":"<<ns;
#if defined(__linux__)
  struct rusage usage{};
  if (getrusage(RUSAGE_SELF,&usage)==0) out<<",\"host_peak_rss_kib\":"<<usage.ru_maxrss;
#endif
  return out.str();
}
void report(const MinimalGuest& guest,const RunResult& run,std::chrono::steady_clock::time_point start) {
  std::cout<<"{\"guest\":"<<guest.statistics()<<",\"executed_this_run\":"<<run.executed
      <<",\"status\":"<<quote(run.halted?"exited":run.waiting?"waiting":"yielded")<<",\"stdout\":"<<quote(run.output)
      <<",\"stderr\":"<<quote(run.error_output)<<",\"snapshot_bytes\":"<<guest.snapshot().size()<<elapsed(start)<<"}\n";
}
void help() {
  std::cout<<"REVERSE ARM64 compatibility module (Android boot is not implemented)\n"
      "reverse_guest demo [output.rvg]\n"
      "reverse_guest demo-init output.rvg\n"
      "reverse_guest fp-demo [output.rvg]\n"
      "reverse_guest fp-demo-init output.rvg\n"
      "reverse_guest init program.elf output.rvg [ram_bytes] [max_overrides]\n"
      "reverse_guest system-demo-init output.rvg\n"
      "reverse_guest system-init kernel.elf output.rvg [ram_bytes] [max_overrides]\n"
      "reverse_guest linux-init Image output.rvg [initrd]\n"
      "reverse_guest android-cuttlefish-init Image combined_initrd.bin output.rvg\n"
      "reverse_guest android-cuttlefish-dtb output.dtb [initrd_bytes]\n"
      "reverse_guest run input.rvg output.rvg [steps]\n"
      "reverse_guest inspect input.rvg\n"
      "reverse_guest xi input.rvg delta.xi output.rvg\n"
      "reverse_guest rxe input.rvg delta.rxe output.rvg\n"
      "reverse_guest input input.rvg text.txt output.rvg\n"
      "reverse_guest device-tree output.dtb [ram_bytes]\n"
      "reverse_guest peek input.rvg address [bytes]\n";
}
} // namespace
int main(int argc,char** argv) {
  try {
    const auto start=std::chrono::steady_clock::now();
    if (argc<2) { help();return 2; }
    const std::string operation=argv[1];
    if (operation=="--help") { help();return 0; }
    if(operation=="device-tree"){
      require(argc==3||argc==4,"device-tree requires output DTB and optional RAM size");
      const auto ram=argc==4?integer(argv[3]):128*1024*1024;
      require(ram>=64*1024*1024&&ram<=MinimalGuest::max_ram-MachineState::ram_base,"Invalid system RAM size");
      save_file(argv[2],guest_device_tree(ram));std::cout<<"{\"device_tree\":\"reverse-arm64-v1\"}\n";return 0;
    }
    if(operation=="android-cuttlefish-dtb"){
      require(argc==3||argc==4,
        "android-cuttlefish-dtb requires output DTB and optional initrd bytes");
      const auto rd=argc==4?integer(argv[3]):0;
      require(rd<=MinimalGuest::max_linux_initrd && rd<=64*1024*1024-2*1024*1024,
        "DTB initrd bytes exceed 128 MiB guest memory layout");
      save_file(argv[2],guest_device_tree(
        128*1024*1024,MachineState::ram_base+64*1024*1024,rd,
        guest_android_cuttlefish_bootargs()));
      std::cout<<"{\"device_tree\":\"reverse-arm64-android-bootargs-v1\","
                  "\"android_boot\":false,\"adb\":false}\n";
      return 0;
    }
    if(operation=="android-cuttlefish-init"){
      require(argc==5,
        "android-cuttlefish-init requires Image, combined_initrd.bin and output RVG");
      // The same original XI/Ether runtime, not QEMU or a second guest.
      // 0 explicitly chooses adaptive InverseMemory; 256 MiB RVG transport.
      MinimalGuest guest(128*1024*1024,0);
      guest.load_linux_image(
        read_file(argv[2],MinimalGuest::max_linux_kernel),
        read_file(argv[3],MinimalGuest::max_linux_initrd),
        guest_android_cuttlefish_bootargs());
      save_file(argv[4],guest.snapshot());
      std::cout<<guest.statistics()<<'\n';
      return 0;
    }
    if(operation=="system-demo-init"){
      require(argc==3,"system-demo-init requires output state");MinimalGuest guest(128*1024*1024);
      guest.load_elf(system_demo_elf(),true);save_file(argv[2],guest.snapshot());std::cout<<guest.statistics()<<'\n';return 0;
    }
    if(operation=="fp-demo"||operation=="fp-demo-init"){
      require(argc==3||(argc==2&&operation=="fp-demo"),"fp-demo accepts a state filename; fp-demo-init requires one");
      MinimalGuest guest;guest.load_elf(fp_demo_elf());RunResult run;
      if(operation=="fp-demo")run=guest.run(1000);
      if(argc==3)save_file(argv[2],guest.snapshot());report(guest,run,start);return 0;
    }
    if(operation=="linux-init"){
      require(argc==4||argc==5,"linux-init requires Image, output state and optional initrd");
      MinimalGuest guest(128*1024*1024,1048576);
      guest.load_linux_image(read_file(argv[2],MinimalGuest::max_image),argc==5?read_file(argv[4],MinimalGuest::max_image):Bytes{});
      save_file(argv[3],guest.snapshot());std::cout<<guest.statistics()<<'\n';return 0;
    }
    if (operation=="demo-init") {
      require(argc==3,"demo-init requires an output state filename");
      MinimalGuest guest;guest.load_elf(demo_elf());save_file(argv[2],guest.snapshot());
      std::cout<<guest.statistics()<<'\n';return 0;
    }
    if (operation=="demo") {
      require(argc==2||argc==3,"demo accepts an optional state filename");
      MinimalGuest guest;guest.load_elf(demo_elf());const auto run=guest.run(100);
      require(guest.read(0x8000,8)==42 && run.halted && run.exit_code==0,"ARM64 demo did not compute 42");
      if (argc==3) save_file(argv[2],guest.snapshot());
      report(guest,run,start);return 0;
    }
    if (operation=="init"||operation=="system-init") {
      require(argc>=4&&argc<=6,"init requires ELF and output state, optional RAM/override budget");
      MinimalGuest guest(argc>=5?integer(argv[4]):operation=="init"?UINT64_C(1)<<30:128*1024*1024,argc==6?integer(argv[5]):65536);
      guest.load_elf(read_file(argv[2],MinimalGuest::max_image),operation=="system-init"); save_file(argv[3],guest.snapshot());
      std::cout<<guest.statistics()<<'\n';return 0;
    }
    require(argc>=3,"Guest state filename required");
    auto guest=MinimalGuest::from_snapshot(read_file(argv[2],MinimalGuest::max_adaptive_snapshot));
    if (operation=="inspect") { require(argc==3,"inspect accepts one state filename");std::cout<<guest.statistics()<<'\n';return 0; }
    if (operation=="run") {
      require(argc==4||argc==5,"run requires input and output state, optional step budget");
      const auto run=guest.run(argc==5?integer(argv[4]):10000);save_file(argv[3],guest.snapshot());report(guest,run,start);return 0;
    }
    if (operation=="xi"||operation=="rxe") {
      require(argc==5,"xi/rxe requires input state, program and output state");
      const auto program=read_file(argv[3],16*1024*1024);
      if (operation=="xi") guest.apply_xi_delta(std::string(program.begin(),program.end()));
      else guest.apply_rxe_delta(program);
      save_file(argv[4],guest.snapshot());std::cout<<guest.statistics()<<'\n';return 0;
    }
    if(operation=="input"){
      require(argc==5,"input requires input state, serial text file and output state");const auto text=read_file(argv[3],4096);
      guest.console_input(std::string(text.begin(),text.end()));save_file(argv[4],guest.snapshot());std::cout<<guest.statistics()<<'\n';return 0;
    }
    if (operation=="peek") {
      require(argc==4||argc==5,"peek requires a byte address and optional count");
      const auto first=integer(argv[3]),count=argc==5?integer(argv[4]):1;
      require(count>0&&count<=256&&first>=guest.ram_base()&&first-guest.ram_base()<=guest.logical_ram()&&count<=guest.logical_ram()-(first-guest.ram_base()),"peek range is invalid");
      std::cout<<"{\"address\":"<<quote(std::to_string(first))<<",\"bytes\":[";
      for (std::uint64_t i=0;i<count;++i) {if(i)std::cout<<',';std::cout<<unsigned(guest.read8(first+i));}
      std::cout<<"]}\n";return 0;
    }
    throw std::runtime_error("Unknown guest command");
  } catch (const std::exception& error) {std::cerr<<"reverse_guest: "<<error.what()<<'\n';return 2;}
}
