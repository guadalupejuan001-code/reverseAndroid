// Development probe of real, pinned Android/Linux ARM64 Image and XI/Ether RVG. An unsupported
// instruction/register is a recorded engineering stop, never a claimed boot.
#include "minimal_guest.hpp"
#include <fstream>
#include <chrono>
#include <iostream>
#include <sstream>
using namespace reverse_pc::guest;
int main(int argc,char** argv){try{
  require(argc>=2&&argc<=7,"Expected Image-or-RVG [budget] [checkpoint] [initrd_for_Image] [android-cuttlefish] or RVG [budget] [checkpoint] [--chunk=N] [--wall-ms=N] [--stop-el0=1]");
  const auto begin=std::chrono::steady_clock::now();
  const auto budget=argc>=3?std::stoull(argv[2]):500000;require(budget>0&&budget<=100000000,"Probe budget must be 1..100000000");std::ifstream in(argv[1],std::ios::binary);
  require(bool(in),"Cannot open Image");Bytes image((std::istreambuf_iterator<char>(in)),{});
  require(image.size()<=MinimalGuest::max_adaptive_snapshot,"Linux probe input too large");
  const bool checkpoint=image.size()>=4&&image[0]=='R'&&image[1]=='V'&&image[2]=='G';
  // Host-side controls: one original XI/Ether core and unchanged guest
  // checkpoints. Soft wall budgets check at transaction boundaries, never
  // interrupt an instruction or record a virtual CPU timer in the RVG.
  std::uint64_t run_chunk=10000,max_wall_ms=0;bool stop_el0=false;
  if(checkpoint){
    bool seen_chunk=false,seen_wall=false,seen_stop=false;
    for(int i=4;i<argc;++i){
      const std::string arg(argv[i]);
      const bool chunk=arg.rfind("--chunk=",0)==0;
      const bool wall=arg.rfind("--wall-ms=",0)==0;
      const bool stop=arg.rfind("--stop-el0=",0)==0;
      require(chunk||wall||stop,
        "Checkpoint accepts only --chunk=N, --wall-ms=N and --stop-el0=1");
      bool& seen=chunk?seen_chunk:wall?seen_wall:seen_stop;
      require(!seen,"Duplicate checkpoint host control");seen=true;
      const std::string numeric=arg.substr(chunk?8:wall?10:11);
      require(!numeric.empty() &&
        numeric.find_first_not_of("0123456789")==std::string::npos,
        "Checkpoint host control must be a positive decimal integer");
      const auto value=std::stoull(numeric);
      if(chunk){
        require(value>0&&value<=1000000,
          "Transaction chunk must be 1..1000000 original ARM64 instructions");
        run_chunk=value;
      }else if(wall){
        require(value>0&&value<=600000,
          "Host wall budget must be 1..600000 milliseconds");
        max_wall_ms=value;
      }else{
        require(value==1,"EL0 observation control must be --stop-el0=1");stop_el0=true;
      }
    }
  }else{
    require(argc<=6,"EL0 observation is a checkpoint-only host control");
    require(argc!=6||std::string(argv[5])=="android-cuttlefish"||std::string(argv[5])=="android-cuttlefish-normal"||std::string(argv[5])=="android-cuttlefish-normal-no-scx",
      "Only named Android Cuttlefish bootargs profiles are supported");
  }
  MinimalGuest guest(128*1024*1024,1048576);Bytes initrd;if(argc>=5&&!checkpoint){std::ifstream rd(argv[4],std::ios::binary);require(bool(rd),"Cannot open initrd");initrd.assign(std::istreambuf_iterator<char>(rd),{});}
  if(checkpoint){guest=MinimalGuest::from_snapshot(image);require(guest.machine().enabled&&guest.machine().image_kind==2,"Probe checkpoint must contain a Linux Image");}
  else guest.load_linux_image(image,initrd,
    argc==6?(std::string(argv[5])=="android-cuttlefish-normal-no-scx"?
      guest_android_cuttlefish_normal_no_scx_bootargs():std::string(argv[5])=="android-cuttlefish-normal"?
      guest_android_cuttlefish_normal_bootargs():guest_android_cuttlefish_bootargs()):std::string{});
  auto last_heartbeat=std::chrono::steady_clock::now();
  const auto emit_heartbeat=[&](bool force=false){
    const auto now=std::chrono::steady_clock::now();
    if(!force&&now-last_heartbeat<std::chrono::seconds(2))return;
    last_heartbeat=now;
    const auto elapsed_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(now-begin).count();
    // stderr is intentionally separate from the stable machine-readable
    // stdout report; UI can render it without waiting for an entire boot.
    std::cerr<<"REVERSE_ANDROID_GUEST_HEARTBEAT "
             <<"{\"guest_instructions\":"<<guest.registers().instructions
             <<",\"pc\":"<<quote(hex(guest.registers().pc))
             <<",\"max_wall_ms\":"<<max_wall_ms<<",\"host_elapsed_ns\":"<<elapsed_ns
             <<",\"guest_el\":"<<(guest.machine().enabled?guest.machine().el:0)
             <<",\"android_init_observed\":false}\n";
  };

  // Preserve the old input RVG unchanged on disk; explicitly promote only the
  // guest running this development probe to the paged unbounded authority.
  guest.enable_adaptive_memory();
  const auto initial_instructions=guest.registers().instructions;std::string console,reason="instruction_budget";
  for(std::uint64_t step=0;step<budget && !guest.registers().halted;step+=run_chunk){
    const auto count=std::min<std::uint64_t>(run_chunk,budget-step);
    try{
      auto result=guest.run(count,stop_el0);console+=result.output;
      emit_heartbeat();
      if(stop_el0&&!guest.machine().el){reason="userspace_entry";emit_heartbeat(true);break;}
      if(result.waiting){reason="waiting_for_device_or_interrupt";break;}
      if(max_wall_ms &&
         std::chrono::steady_clock::now()-begin>=std::chrono::milliseconds(max_wall_ms) &&
         !guest.registers().halted){reason="host_wall_budget";emit_heartbeat(true);break;}
    }
    catch(const std::exception&){
      // Locate the first rejected transition without replaying thousands of
      // one-instruction COW transactions over a large already-written state.
      const auto beginning=guest;auto good=guest;std::string prefix;
      std::uint64_t low=0,high=count;
      while(low+1<high){const auto middle=low+(high-low)/2;auto trial=beginning;
        try{const auto run=trial.run(middle);low=middle;good=std::move(trial);prefix=run.output;}
        catch(const std::exception&){high=middle;}
      }
      guest=std::move(good);console+=prefix;
      try{guest.run(1);reason="probe_localization_error";}catch(const std::exception& error){reason=error.what();}
      break;
    }
  }
  // Read-only diagnostic: inspect the exact XI/Ether-backed next instruction.
  std::string next_instruction="null";
  try {
    const auto virtual_pc=guest.registers().pc;
    const auto physical_pc=guest.physical_address(virtual_pc,false,true);
    const auto opcode=guest.read(physical_pc,4);
    std::ostringstream details;
    details<<"{\"virtual_pc\":"<<quote(hex(virtual_pc))
           <<",\"physical_pc\":"<<quote(hex(physical_pc))
           <<",\"opcode\":"<<quote(hex(opcode))
           <<",\"uart_ifls\":"<<guest.machine().uart_ifls
           <<",\"registers\":{";
    for(unsigned i=0;i<guest.registers().x.size();++i){
      if(i)details<<',';
      details<<quote("x"+std::to_string(i))<<':'<<quote(hex(guest.registers().x[i]));
    }
    details<<"}";
    if(reason=="userspace_entry"){
      try{
        constexpr char digits[]="0123456789abcdef";std::string window;
        for(unsigned i=0;i<64;++i){
          const auto address=guest.physical_address(virtual_pc+i,false,true);
          const auto byte=guest.read8(address);
          window.push_back(digits[byte>>4]);window.push_back(digits[byte&15]);
        }
        details<<",\"window64_hex\":"<<quote(window);
      }catch(...){details<<",\"window64_error\":\"entry code window unavailable\"";}
    }
    details<<"}";next_instruction=details.str();
  }catch(const std::exception& error){
    next_instruction="{\"inspection_error\":"+quote(error.what())+"}";
  }catch(...){next_instruction="{\"inspection_error\":\"architectural translation fault\"}";}
  const auto state=guest.snapshot();if(argc>=4){std::ofstream out(argv[3],std::ios::binary);out.write(reinterpret_cast<const char*>(state.data()),state.size());require(bool(out),"Cannot save probe checkpoint");}
  const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
  std::cout<<"{\"expected_upstream_linux\":\"6.1/830b3c68c1fb1e9176028d02ef86f3cf76aa2476\",\"initial_instructions\":"<<initial_instructions<<",\"guest\":"<<guest.statistics()
    <<",\"run_chunk_instructions\":"<<run_chunk<<",\"max_wall_ms\":"<<max_wall_ms<<",\"host_elapsed_ns\":"<<elapsed<<",\"checkpoint_bytes\":"<<state.size()<<",\"checkpoint_fnv\":"<<quote(hex(digest(state,state.size())))<<",\"adaptive_classical_storage\":"<<(guest.runtime().memory().limits().max_overrides==0?"true":"false")
    <<",\"stop_reason\":"<<quote(reason)<<",\"next_instruction\":"<<next_instruction<<",\"console\":"<<quote(console)
    <<",\"linux_userspace_init_observed\":"<<(console.find("REVERSE_LINUX_INIT_OK")!=std::string::npos?"true":"false")
    <<",\"linux_boot_complete\":false,\"android_boot\":false}\n";
  return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
