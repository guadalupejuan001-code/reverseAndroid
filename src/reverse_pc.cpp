#include "mundo_x/quantum_runtime.hpp"
#include "rxe.hpp"
#include "rxe_app.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs=std::filesystem;
using Runtime=mundo_x::QuantumRuntime;

namespace {
constexpr const char* UPSTREAM="e2778d962af22f6f1a0794058dd4685fc2573966";
constexpr const char* FAR="9999999999999999999999999999999999999999999999999999999999999999";
constexpr std::uint64_t MAX_BYTES=32ULL*1024*1024;
constexpr std::uint64_t FNV_BASE=UINT64_C(14695981039346656037);
constexpr std::uint64_t FNV_MULT=UINT64_C(1099511628211);
#ifndef REVERSE_PC_SOURCE_DIR
#define REVERSE_PC_SOURCE_DIR "."
#endif

void check(bool success,const std::string& reason) {
  if(!success)throw std::runtime_error(reason);
}

std::string read_file(const fs::path& file) {
  const auto size=fs::file_size(file);
  check(size<=16*1024*1024,"program larger than 16 MiB: "+file.string());
  std::ifstream in(file,std::ios::binary);
  check(bool(in),"cannot open: "+file.string());
  std::string data(static_cast<std::size_t>(size),'\0');
  in.read(data.data(),static_cast<std::streamsize>(data.size()));
  check(static_cast<std::size_t>(in.gcount())==data.size(),"truncated file: "+file.string());
  return data;
}

std::uint64_t fnv(const std::string& a,const std::string& b={}) {
  std::uint64_t result=FNV_BASE;
  for(unsigned char c:a){result^=c;result*=FNV_MULT;}
  for(unsigned char c:b){result^=c;result*=FNV_MULT;}
  return result;
}
std::string hex(std::uint64_t value) {
  std::ostringstream o;
  o<<std::hex<<std::setfill('0')<<std::setw(16)<<value;
  return o.str();
}
void write64(std::ostream& out,std::uint64_t value) {
  for(unsigned i=0;i<8;++i)out.put(static_cast<char>((value>>(8*i))&255u));
}
std::uint64_t read64(std::istream& in) {
  std::uint64_t value=0;
  for(unsigned i=0;i<8;++i){
    const int byte=in.get();
    check(byte!=EOF,"state header is incomplete");
    value|=std::uint64_t(static_cast<unsigned>(byte))<<(i*8);
  }
  return value;
}

// Single-file persistent snapshot: core + inverse memory must be restored
// together. The FNV-1a checksum detects accidental corruption, not attacks.
void save_state(const fs::path& filename,const Runtime& state) {
  const auto core=state.core_snapshot(),memory=state.memory_persistent_snapshot();
  check(core.size()<=MAX_BYTES && memory.size()<=MAX_BYTES &&
        core.size()<=MAX_BYTES-memory.size(),"state exceeds 32 MiB");
  if(!filename.parent_path().empty())fs::create_directories(filename.parent_path());
  const fs::path pending=filename.string()+".pending";
  std::ofstream out(pending,std::ios::binary|std::ios::trunc);
  check(bool(out),"cannot write state");
  out.write("RVPC2",5);
  write64(out,core.size());
  write64(out,memory.size());
  write64(out,fnv(core,memory));
  out.write(core.data(),static_cast<std::streamsize>(core.size()));
  out.write(memory.data(),static_cast<std::streamsize>(memory.size()));
  out.close();
  check(bool(out),"state write failed");
  fs::rename(pending,filename);
}
Runtime load_state(const fs::path& filename) {
  std::ifstream in(filename,std::ios::binary);
  check(bool(in),"cannot open state "+filename.string());
  char magic[5]={};
  in.read(magic,5);
  check(in.gcount()==5 && (std::string(magic,5)=="RVPC1" || std::string(magic,5)=="RVPC2"),"invalid REVERSE-PC state header");
  const auto n_core=read64(in),n_mem=read64(in),digest=read64(in);
  check(n_core<=MAX_BYTES && n_mem<=MAX_BYTES &&
        n_core<=MAX_BYTES-n_mem,"state violates size limit");
  std::string core(static_cast<std::size_t>(n_core),'\0');
  std::string memory(static_cast<std::size_t>(n_mem),'\0');
  in.read(core.data(),static_cast<std::streamsize>(core.size()));
  check(static_cast<std::uint64_t>(in.gcount())==n_core,"truncated core snapshot");
  in.read(memory.data(),static_cast<std::streamsize>(memory.size()));
  check(static_cast<std::uint64_t>(in.gcount())==n_mem,"truncated memory snapshot");
  check(in.peek()==EOF && fnv(core,memory)==digest,"invalid state payload/checksum");
  Runtime result;
  result.restore(core,memory);
  check(result.core_snapshot()==core && (std::string(magic,5)=="RVPC2" ? result.memory_persistent_snapshot() : result.memory_snapshot())==memory,
        "upstream snapshot changed during restoration");
  return result;
}
std::string quoted(const std::string& text) {
  std::ostringstream out;
  out<<'"';
  for(unsigned char c:text){
    switch(c){
      case '\\':out<<"\\\\";break;
      case '"':out<<"\\\"";break;
      case '\n':out<<"\\n";break;
      case '\r':out<<"\\r";break;
      case '\t':out<<"\\t";break;
      default:
        if(c<32)out<<"\\u00"<<std::hex<<std::setfill('0')<<std::setw(2)
                        <<unsigned(c)<<std::dec;
        else out<<c;
    }
  }
  out<<'"';
  return out.str();
}
std::string records(const std::string& raw) {
  std::istringstream stream(raw);
  std::ostringstream out;
  out<<"[";
  bool first=true;
  for(std::string line;std::getline(stream,line);){
    if(line.empty())continue;
    if(!first)out<<",";
    first=false;
    out<<line; // Canonical XI output is JSON Lines.
  }
  return out.str()+"]";
}
std::string section(const char* name,const Runtime& state,
                    const std::string& output,const fs::path& file) {
  const auto memory=state.memory().stats();
  const auto core=state.core().stats();
  std::ostringstream out;
  out<<"{\"name\":"<<quoted(name)
     <<",\"state_file\":"<<quoted(file.string())
     <<",\"core_fnv64\":"<<quoted(hex(fnv(state.core_snapshot())))
     <<",\"memory_fnv64\":"<<quoted(hex(fnv(state.memory_snapshot())))
     <<",\"active_qubits\":"<<core.active_qubits
     <<",\"global_ether_rules\":"<<core.ether_rules
     <<",\"overrides\":"<<memory.overrides
     <<",\"quantum_bindings\":"<<memory.quantum_bindings
     <<",\"observations\":"<<records(output)<<"}";
  return out.str();
}

void experiment(const fs::path& output_dir) {
  const fs::path examples=fs::path(REVERSE_PC_SOURCE_DIR)/"examples";
  const auto boot=read_file(examples/"boot.xi");
  const auto program_a=read_file(examples/"program-a.xi");
  const auto program_b=read_file(examples/"program-b.xi");
  Runtime initial;
  const auto output_initial=initial.execute(boot);
  const auto initial_core=initial.core_snapshot();
  const auto initial_memory=initial.memory_snapshot();
  auto read=[](const Runtime& r,const std::string& address){
    return r.memory().read(address,r.core());
  };
  check(read(initial,"7").deterministic&&read(initial,"7").value==5,
        "first input data must be 5");
  check(read(initial,"8").deterministic&&read(initial,"8").value==10,
        "second input data must be 10");
  check(read(initial,"9").deterministic&&read(initial,"9").value==5,
        "explicitly bound quantum byte must start at 5");
  const auto active=initial.core().stats().active_qubits;
  check(read(initial,FAR).value==0 && initial.core().probability_one(FAR)==0.0 &&
        initial.core().stats().active_qubits==active,
        "untouched huge address was materialized");

  Runtime a,b;
  a.restore(initial_core,initial_memory);
  b.restore(initial_core,initial_memory);
  const auto output_a=a.execute(program_a);
  const auto output_b=b.execute(program_b);
  check(read(a,"9").deterministic&&read(a,"9").value==7,
        "program A failed: CNOT should change bound 5 to 7");
  check(!read(b,"9").deterministic&&read(b,"9").handle=="qbyte:9@100",
        "program B must expose an unresolved quantum handle");
  check(a.core().probability_one(FAR)==0.0 &&
        b.core().probability_one(FAR)==0.5,
        "global and local programs must have distinct distant observables");
  check(a.core().stats().ether_rules==0 &&
        b.core().stats().ether_rules==1,
        "program B did not modify an actual global Ether rule");
  check(b.core().stats().active_qubits==active,
        "program B materialized the distant domain");
  check(a.memory_snapshot()==b.memory_snapshot() &&
        a.memory_snapshot()==initial_memory,
        "quantum transformations altered classical memory metadata");
  check(initial_core==initial.core_snapshot() &&
        initial_memory==initial.memory_snapshot() &&
        a.core_snapshot()!=b.core_snapshot(),
        "two runs did not branch from identical immutable initial state");
  check(read(a,"7").value==5 && read(b,"8").value==10,
        "unchanged input data were corrupted");

  for(int i=0;i<3;++i) {
    Runtime again_a,again_b;
    again_a.restore(initial_core,initial_memory);
    again_b.restore(initial_core,initial_memory);
    check(again_a.execute(program_a)==output_a &&
          again_a.core_snapshot()==a.core_snapshot() &&
          again_a.memory_snapshot()==a.memory_snapshot(),"A is not reproducible");
    check(again_b.execute(program_b)==output_b &&
          again_b.core_snapshot()==b.core_snapshot() &&
          again_b.memory_snapshot()==b.memory_snapshot(),"B is not reproducible");
  }

  for(Runtime* current:{&a,&b}){
    const auto before_core=current->core_snapshot();
    const auto before_memory=current->memory_snapshot();
    bool rejected=false;
    try{current->execute("mem_store 7 44\nthis_command_does_not_exist\n");}
    catch(const std::exception&){rejected=true;}
    check(rejected && before_core==current->core_snapshot() &&
          before_memory==current->memory_snapshot(),"invalid program committed changes");
  }

  fs::create_directories(output_dir);
  const auto initial_path=output_dir/"initial.rvp";
  const auto a_path=output_dir/"program-a.rvp";
  const auto b_path=output_dir/"program-b.rvp";
  save_state(initial_path,initial);
  save_state(a_path,a);
  save_state(b_path,b);
  for(const auto& item:std::vector<std::pair<fs::path,const Runtime*>>{
      {initial_path,&initial},{a_path,&a},{b_path,&b}}){
    const auto restored=load_state(item.first);
    check(restored.core_snapshot()==item.second->core_snapshot() &&
          restored.memory_snapshot()==item.second->memory_snapshot(),
          "saved state failed exact restoration");
  }

  // Explicit reversibility, performed after reloading from disk.
  auto undo_a=load_state(a_path),undo_b=load_state(b_path);
  undo_a.execute("cx 100 101\n");
  undo_b.execute("h_all\n");
  check(read(undo_a,"9").deterministic&&read(undo_a,"9").value==5,
        "program A could not be reversed");
  check(read(undo_b,"9").deterministic&&read(undo_b,"9").value==5 &&
        undo_b.core().probability_one(FAR)==0.0,
        "program B could not be reversed");

  const fs::path report_path=output_dir/"report.json";
  std::ofstream out(report_path,std::ios::binary|std::ios::trunc);
  out<<"{\n\"experiment\":\"one-space-two-programs\",\n"
     <<"\"backend\":\"Mundo X QuantumRuntime / InfiniteCore / InverseMemory\",\n"
     <<"\"upstream_commit\":"<<quoted(UPSTREAM)<<",\n"
     <<"\"far_address\":"<<quoted(FAR)<<",\n"
     <<"\"identical_initial_states\":true,\n"
     <<"\"transactional_rollback\":true,\n"
     <<"\"repeat_runs_verified\":3,\n"
     <<"\"saved_and_restored\":true,\n"
     <<"\"reversed_both_programs\":true,\n"
     <<"\"no_dense_domain_allocation\":true,\n"
     <<"\"physical_quantum_hardware\":false,\n"
     <<"\"universal_programming_demonstrated\":false,\n"
     <<"\"initial\":"<<section("initial",initial,output_initial,initial_path)<<",\n"
     <<"\"program_a\":"<<section("A:local-controlled-gate",a,output_a,a_path)<<",\n"
     <<"\"program_b\":"<<section("B:global-ether-rule",b,output_b,b_path)<<"\n}\n";
  out.close();
  check(bool(out),"failed writing report");
  std::cout<<"REVERSE_PC_DEMO_PASS A_byte=7 B_handle=qbyte:9@100"
           <<" far_Z_A=0 far_Z_B=0.5"
           <<" repeat=3 persistence=pass rollback=pass\n"
           <<"Report: "<<report_path.string()<<"\n";
}

void run(const fs::path& program,const fs::path& state_path,
         const fs::path& destination){
  auto runtime=load_state(state_path);
  const auto data=read_file(program);
  check(!reverse_pc::rxe_app::has_magic({data.begin(),data.end()}),"RXE2 applications use app-turn with a message and an output language");
  auto extension=program.extension().string();for(char& c:extension)if(c>='A'&&c<='Z')c=char(c+('a'-'A'));
  const bool binary=extension==".rxe"||reverse_pc::rxe::has_magic(data);
  const auto output=binary?reverse_pc::rxe::execute(runtime,reverse_pc::rxe::Bytes(data.begin(),data.end())):runtime.execute(data);
  save_state(destination,runtime);
  std::cout<<output;
  std::cerr<<"REVERSE_PC_RUN_PASS saved="<<destination.string()<<"\n";
}
void write_program(const fs::path& path,const std::string& data){
  if(!path.parent_path().empty())fs::create_directories(path.parent_path());
  const fs::path temporary=path.string()+".pending";std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
  check(bool(out),"cannot write program output");out.write(data.data(),std::streamsize(data.size()));out.close();check(bool(out),"program output failed");fs::rename(temporary,path);
}
void help(){
  std::cout<<"REVERSE-PC / integrated Ether computer 0.2\n"
           <<"reverse_pc init STATE_OUTPUT.rvp\n"
           <<"reverse_pc demo [directory]\n"
           <<"reverse_pc run PROGRAM.xi|APP.rxe STATE_INPUT.rvp STATE_OUTPUT.rvp\n"
           <<"reverse_pc compile PROGRAM.xi APP.rxe [TITLE]\n"
           <<"reverse_pc inspect-rxe APP.rxe\n"
           <<"reverse_pc disassemble APP.rxe OUTPUT.xi\n"
           <<"reverse_pc pack-app CODE.rxe SEED.rvp APP.rxe ID TITLE VERSION SOURCE_SHA\n"
           <<"reverse_pc app-init APP.rxe NEW_STATE.rvp\n"
           <<"reverse_pc app-turn APP.rxe STATE_INPUT.rvp STATE_OUTPUT.rvp es|en MESSAGE\n"
           <<"reverse_pc --help\n";
}
}

int main(int argc,char** argv) {
  try{
    if(argc==1||std::string(argv[1])=="--help"){help();return 0;}
    const std::string cmd=argv[1];
    if(cmd=="init"&&argc==3) { Runtime initial; save_state(argv[2],initial); }
    else if(cmd=="demo"&&argc<=3)
      experiment(argc==3?fs::path(argv[2]):fs::path("reverse-pc-results"));
    else if(cmd=="run"&&argc==5)
      run(argv[2],argv[3],argv[4]);
    else if(cmd=="compile"&&(argc==4||argc==5)){
      check(!fs::exists(argv[3])||!fs::equivalent(argv[2],argv[3]),"source and executable must be different files");
      const auto image=reverse_pc::rxe::compile(read_file(argv[2]),argc==5?argv[4]:fs::path(argv[2]).stem().string());
      write_program(argv[3],std::string(image.begin(),image.end()));std::cout<<reverse_pc::rxe::describe(image)<<"\n";
    }
    else if(cmd=="inspect-rxe"&&argc==3){const auto data=read_file(argv[2]);const reverse_pc::rxe::Bytes bytes(data.begin(),data.end());std::cout<<(reverse_pc::rxe_app::has_magic(bytes)?reverse_pc::rxe_app::describe(bytes):reverse_pc::rxe::describe(bytes))<<"\n";}
    else if(cmd=="pack-app"&&argc==9){
      check(!fs::exists(argv[4])||(!fs::equivalent(argv[2],argv[4])&&!fs::equivalent(argv[3],argv[4])),"application output must not replace its inputs");
      const auto code=read_file(argv[2]),seed=read_file(argv[3]);
      reverse_pc::rxe_app::App app{argv[5],argv[6],argv[7],argv[8],{code.begin(),code.end()},{seed.begin(),seed.end()}};
      const auto bytes=reverse_pc::rxe_app::pack(app);write_program(argv[4],std::string(bytes.begin(),bytes.end()));std::cout<<reverse_pc::rxe_app::describe(bytes)<<"\n";
    }
    else if(cmd=="app-init"&&argc==4){
      check(!fs::exists(argv[3]),"state already exists; use another output to preserve its memory");
      const auto data=read_file(argv[2]);const auto app=reverse_pc::rxe_app::decode({data.begin(),data.end()});write_program(argv[3],std::string(app.seed.begin(),app.seed.end()));
    }
    else if(cmd=="app-turn"&&argc==7){
      check(std::string(argv[5])=="es"||std::string(argv[5])=="en","language must be es or en");
      check(!fs::exists(argv[4])||!fs::equivalent(argv[2],argv[4]),"state must not replace the executable");
      const auto data=read_file(argv[2]);const auto result=reverse_pc::rxe_app::turn({data.begin(),data.end()},reverse_pc::rvpc2::save(load_state(argv[3])),argv[6],std::string(argv[5])=="es"?1:2);
      write_program(argv[4],std::string(result.state.begin(),result.state.end()));std::cout<<result.text<<"\n";
    }
    else if(cmd=="disassemble"&&argc==4){
      check(!fs::exists(argv[3])||!fs::equivalent(argv[2],argv[3]),"input and output must be different files");const auto data=read_file(argv[2]);write_program(argv[3],reverse_pc::rxe::disassemble(reverse_pc::rxe::decode(reverse_pc::rxe::Bytes(data.begin(),data.end()))));
    }
    else throw std::invalid_argument("incorrect command/arguments, run --help");
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"REVERSE_PC_FAIL "<<e.what()<<"\n";
    return 1;
  }
}
