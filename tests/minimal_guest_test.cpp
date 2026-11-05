#include "minimal_guest_demo.hpp"
#include <iostream>
#include <functional>

using namespace reverse_pc::guest;
namespace {
unsigned checks=0;
void check(bool value,const char* text) { ++checks; if(!value)throw std::runtime_error(text); }
void fails(const std::function<void()>& action,const char* text) {
  bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,text);
}
void patch(Bytes& image,std::size_t offset,std::uint64_t value,unsigned count) {
  for(unsigned i=0;i<count;++i)image.at(offset+i)=std::uint8_t(value>>(i*8));
}
Bytes instructions(const std::vector<std::uint32_t>& opcodes) {
  auto image=demo_elf();image.resize(256+opcodes.size()*4);
  patch(image,64+32,opcodes.size()*4,8);
  for(std::size_t i=0;i<opcodes.size();++i)patch(image,256+4*i,opcodes[i],4);
  return image;
}
void memory() {
  MinimalGuest guest(UINT64_C(1)<<40);guest.load_elf(demo_elf(UINT64_C(1)<<40));
  check(guest.runtime().memory().stats().overrides==0,"Loading ELF materialized RAM");
  check(guest.read8((UINT64_C(1)<<40)-1)==0,"BSS/zero rule lost");
  check(guest.image_bytes()==332,"Unexpected immutable fixture size");
  const auto original=guest.read8(0x1000);
  check(original!=0,"Fixture instruction must have nonzero first byte");
  guest.write(0x1000,0,1);check(guest.read8(0x1000)==0,"Zero over nonzero image was lost");
  check(guest.runtime().memory().stats().overrides==1,"Image edit did not use one XOR delta");
  guest.write(0x1000,original,1);check(guest.runtime().memory().stats().overrides==0,"Restoring image retained a redundant delta");
  guest.write(90000,0,8);check(guest.runtime().memory().stats().overrides==0,"Writing zero to zero RAM allocated state");
  std::vector<std::uint8_t> oracle(8192);
  for(std::size_t i=0;i<oracle.size();++i)oracle[i]=guest.read8(0x1000+i);
  std::uint64_t rng=0x563412;
  for(unsigned i=0;i<500;++i) {
    rng=rng*UINT64_C(6364136223846793005)+1;
    const auto offset=std::size_t(rng%(oracle.size()-8)),value=rng^(rng>>11);
    const unsigned size=1u<<(i%4);guest.write(0x1000+offset,value,size);
    for(unsigned j=0;j<size;++j)oracle[offset+j]=std::uint8_t(value>>(j*8));
  }
  for(std::size_t i=0;i<oracle.size();++i)check(guest.read8(0x1000+i)==oracle[i],"Sparse/image memory differs from dense oracle");
  const auto before=guest.snapshot();
  fails([&]{guest.write(guest.logical_ram()-4,42,8);},"Out-of-range word store accepted");
  check(guest.snapshot()==before,"Out-of-range store changed state");
  MinimalGuest bounded(1<<20,1);bounded.load_elf(demo_elf(1<<20));const auto clean=bounded.snapshot();
  fails([&]{bounded.write(0x8000,0x0101,2);},"Override exhaustion accepted");
  check(bounded.snapshot()==clean,"Failed multi-byte store leaked a partial change");
}
void execution() {
  MinimalGuest direct;direct.load_elf(demo_elf());const auto all=direct.run(100);
  check(all.halted&&all.exit_code==0&&all.executed==13,"ARM64 demo did not exit normally");
  check(all.output=="REVERSE ARM64: 6*7 = 42\n","Guest write output mismatch");
  check(direct.read(0x8000,8)==42,"ARM64 arithmetic/store result mismatch");
  check(direct.runtime().memory().stats().overrides==1,"Guest wrote redundant zero deltas");
  MinimalGuest sliced;sliced.load_elf(demo_elf());const auto part=sliced.run(4);
  check(!part.halted&&part.executed==4&&sliced.registers().x[2]==42,"Instruction slice failed");
  auto restored=MinimalGuest::from_snapshot(sliced.snapshot());const auto rest=restored.run(100);
  check(rest.output==all.output&&rest.executed==9,"Checkpoint repeated or skipped guest effects");
  check(restored.snapshot()==direct.snapshot(),"Sliced/restored guest differs from uninterrupted execution");
  MinimalGuest bad;bad.load_elf(instructions({0xd2800aa0,0xd2900001,0xf9000020,0xffffffff}));
  const auto pristine=bad.snapshot();
  fails([&]{bad.run(4);},"Unsupported ARM64 instruction did not trap");
  check(bad.snapshot()==pristine,"Faulting run committed CPU or memory changes");
  MinimalGuest loop;loop.load_elf(instructions({0x14000000}));const auto yield=loop.run(73);
  check(!yield.halted&&yield.executed==73&&loop.registers().pc==0x1000,"Finite budget did not yield a running guest");
  auto resumed=MinimalGuest::from_snapshot(loop.snapshot());resumed.run(1);
  check(resumed.registers().instructions==74,"Guest instruction counter did not persist");
}
void arithmetic() {
  // Independently specified architectural outcomes: 64-bit wrap/carry, W-register
  // zero extension, arithmetic shift and signed division overflow.
  MinimalGuest alu;
  alu.load_elf(instructions({0x92800000,0xd2800021,0xab010002,0x52800023}));
  alu.run(4);
  check(alu.registers().x[0]==UINT64_MAX,"MOVN did not produce all ones");
  check(alu.registers().x[2]==0&&alu.registers().nzcv==6,"A64 ADD wrap/Z/C flags mismatch");
  check(alu.registers().x[3]==1,"W register did not zero extend");
  alu.load_elf(instructions({0x92800000,0x9341fc01}));alu.run(2);
  check(alu.registers().x[1]==UINT64_MAX,"ASR did not sign extend");
  alu.load_elf(instructions({0xd2f00000,0x92800001,0x9ac10c02,0x9adf0803}));alu.run(4);
  check(alu.registers().x[2]==(UINT64_C(1)<<63),"SDIV min/-1 did not wrap architecturally");
  check(alu.registers().x[3]==0,"UDIV by zero did not return zero");
}
void languages_and_quantum() {
  MinimalGuest guest;guest.load_elf(demo_elf());
  const std::string xi="#!reverse-pc-machine-v1\nmem_store 90000 42\nh 10\nhalt\n";
  const auto rxe=reverse_pc::rxe::compile(xi,"Guest delta");guest.apply_rxe_delta(rxe);
  check(guest.read8(90000)==42,"RXE did not update the actual guest delta authority");
  guest.apply_xi_delta("mem_bind 90001 10\n");const auto before=guest.snapshot();
  fails([&]{guest.read8(90001);},"Guest implicitly measured a nondeterministic byte");
  fails([&]{guest.write(90001,0,1);},"Guest silently overwrote a quantum handle");
  check(guest.snapshot()==before,"Rejected quantum byte changed core/memory/RNG");
  auto restored=MinimalGuest::from_snapshot(before);
  check(restored.snapshot()==before,"Guest lost shared quantum state during restore");
  guest.apply_xi_delta("mem_unbind 90001\nmem_reset 255\n");
  check(guest.read8(90000)==255,"XI global delta rule was not applied lazily");
  check(guest.runtime().memory().stats().overrides==0,"Global delta rule was materialized");
}
void malformed() {
  MinimalGuest guest;guest.load_elf(demo_elf());const auto initial=guest.snapshot();
  auto wrong=demo_elf();wrong[18]=62;fails([&]{guest.load_elf(wrong);},"Foreign ELF architecture accepted");
  check(guest.snapshot()==initial,"Invalid ELF replaced live guest");
  auto dynamic=demo_elf();patch(dynamic,16,3,2);fails([&]{guest.load_elf(dynamic);},"Dynamic ELF accepted without a loader");
  auto truncated=initial;truncated.pop_back();fails([&]{MinimalGuest::from_snapshot(truncated);},"Truncated guest snapshot accepted");
  auto corrupt=initial;corrupt.back()^=1;fails([&]{MinimalGuest::from_snapshot(corrupt);},"Corrupt guest snapshot accepted");
  auto extreme=demo_elf();patch(extreme,64+40,UINT64_MAX,8);fails([&]{guest.load_elf(extreme);},"Overflowing ELF segment accepted");
}
} // namespace
int main() {
  try {memory();execution();arithmetic();languages_and_quantum();malformed();
    std::cout<<"{\"checks\":"<<checks<<",\"memory_oracle\":\"dense-independent\",\"execution\":\"own-aarch64\",\"languages\":[\"XI\",\"RXE\"],\"android_boot\":\"not_implemented\"}\n";return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
