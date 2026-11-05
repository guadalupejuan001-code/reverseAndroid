#include "../tools/guest_console_input.hpp"
#include <filesystem>
#include <functional>
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;require(ok,why);}
void fails(const std::function<void()>& action){bool caught=false;try{action();}catch(const std::exception&){caught=true;}check(caught,"Invalid input accepted");}
MinimalGuest fixture(){
  Bytes kernel(64);
  const auto patch=[&](unsigned at,std::uint64_t value,unsigned size){for(unsigned i=0;i<size;++i)kernel[at+i]=std::uint8_t(value>>(8*i));};
  patch(0,0x14000010,4);patch(8,0x80000,8);patch(16,576,8);patch(24,2,8);patch(56,0x644d5241,4);
  for(unsigned i=0;i<128;++i)put(kernel,0xd503201f,4);
  MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(kernel);
  std::array<std::uint8_t,32> seed{};for(unsigned i=0;i<seed.size();++i)seed[i]=std::uint8_t(i*7+3);
  guest.enable_counter_latency(seed);guest.configure_counter_rate(64);guest.run(67);return guest;
}
}
int main(int argc,char** argv){try{
  require(argc==2,"Expected temporary test directory");const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
  const auto write=[&](const char* name,const Bytes& bytes){const auto p=(root/name).string();std::ofstream file(p,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));require(bool(file),"Fixture write failed");return p;};
  check(guest_console_input_file(nullptr).empty(),"Absent input changed behavior");fails([&]{guest_console_input_file("");});fails([&]{guest_console_input_file((root/"missing").c_str());});
  const auto empty=write("empty",{});check(guest_console_input_file(empty.c_str()).empty(),"Empty input rejected");
  Bytes binary;for(unsigned n=0;n<256;++n)binary.push_back(std::uint8_t(n));const auto path=write("binary",binary);check(guest_console_input_file(path.c_str())==binary,"Binary input altered");
  const auto maximum=write("maximum",Bytes(4096,0xa5));check(guest_console_input_file(maximum.c_str()).size()==4096,"Maximum input rejected");
  const auto oversize=write("oversize",Bytes(4097,0));fails([&]{guest_console_input_file(oversize.c_str());});
  auto guest=fixture();const auto before=guest.snapshot();const auto memory=guest.runtime().memory_persistent_snapshot();const auto core=guest.runtime().core_snapshot();const auto cpu=guest.registers();const auto fp=guest.floating();const auto machine=guest.machine();
  check(guest_console_input_event(guest,{}).empty()&&guest.snapshot()==before,"Empty input changed original bytes");
  const auto receipt=guest_console_input_event(guest,binary);check(receipt.find("\"instructions\":67")!=std::string::npos&&receipt.find("\"counter_subtick\":3")!=std::string::npos&&receipt.find("\"bytes\":256")!=std::string::npos,"Input boundary receipt is wrong");
  check(receipt.find(GuestSha256::hash(binary))!=std::string::npos,"Receipt did not authenticate exact input bytes");
  check(guest.machine().receive==std::string(binary.begin(),binary.end()),"Modeled UART lost input bytes");
  auto normalized=guest.machine();normalized.receive=machine.receive;check(normalized.save()==machine.save(),"Input changed another modeled device, timer, seed or interrupt field");
  check(guest.registers().x==cpu.x&&guest.registers().sp==cpu.sp&&guest.registers().pc==cpu.pc&&guest.registers().instructions==cpu.instructions&&guest.registers().nzcv==cpu.nzcv,"Input executed CPU instructions");
  check(guest.floating().save()==fp.save(),"Input changed floating registers");
  check(guest.runtime().memory_persistent_snapshot()==memory&&guest.runtime().core_snapshot()==core,"Input changed the XI/Ether memory authority");
  const auto queued=guest.snapshot();auto restored=MinimalGuest::from_snapshot(queued);check(restored.snapshot()==queued,"Original decoder did not preserve input bytes");
  fails([&]{guest_console_input_event(guest,Bytes(4096,1));});check(guest.snapshot()==queued,"Rejected overflow changed state");
  fails([&]{guest_console_input_event(guest,Bytes(4097,1));});check(guest.snapshot()==queued,"Rejected oversized event changed state");
  auto control=MinimalGuest::from_snapshot(before);const auto accepted=guest_console_input_event(control,Bytes(4096,0));check(control.machine().receive.size()==4096&&!accepted.empty(),"Full UART capacity rejected");
  check(MinimalGuest::from_snapshot(control.snapshot()).snapshot()==control.snapshot(),"Maximum queue changed original decoding");
  std::cout<<"{\"checks\":"<<checks<<",\"original_compact_decoder\":true,\"modeled_uart_only\":true,\"cpu_instructions_executed_by_input\":0,\"android_boot\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
