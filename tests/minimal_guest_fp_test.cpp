#include "minimal_guest.hpp"
#include <future>
#include <iostream>
#include <bitset>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0;
void check(bool okay,const char* message){++checks;if(!okay)throw std::runtime_error(message);}
template<class F>void fails(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
Bytes elf(std::initializer_list<std::uint32_t> words,bool system=false){
  Bytes code;for(auto word:words)put(code,word,4);const auto entry=system?0x40080000:0x1000;
  Bytes bytes{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(bytes,2,2);put(bytes,183,2);put(bytes,1,4);put(bytes,entry);put(bytes,64);put(bytes,0);
  put(bytes,0,4);put(bytes,64,2);put(bytes,56,2);put(bytes,1,2);put(bytes,0,2);put(bytes,0,2);put(bytes,0,2);
  put(bytes,1,4);put(bytes,7,4);put(bytes,0x100);put(bytes,entry);put(bytes,entry);put(bytes,code.size());put(bytes,code.size());put(bytes,0x100);
  bytes.resize(0x100);bytes.insert(bytes.end(),code.begin(),code.end());return bytes;
}
void shift_insert(){
  for(bool wide:{false,true})for(unsigned bits:{8u,16u,32u,64u}){
    if(bits==64&&!wide)continue;const unsigned bytes=wide?16:8,lanes=bytes*8/bits;
    for(bool left:{false,true})for(unsigned n=0;n<bits;n++)for(bool alias:{false,true}){
      const unsigned amount=left?n:n+1,encoded=left?bits+amount:2*bits-amount,rd=alias?1:2;
      const auto opcode=(left?0x2f005400u:0x2f004400u)|(wide?0x40000000u:0u)|(encoded<<16)|(1<<5)|rd;
      MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x0c40a041u|(wide?0x40000000u:0u),opcode}));
      for(unsigned j=0;j<bytes*2;j++)g.write(0x2000+j,(j*43+19)&255,1);
      g.run(2);const auto source=g.floating().v[1],old=g.floating().v[rd];auto restored=MinimalGuest::from_snapshot(g.snapshot());g.run(1);restored.run(1);
      check(g.snapshot()==restored.snapshot(),"Shift-insert alias or restore changed complete state");
      for(unsigned lane=0;lane<lanes;lane++){
        const std::bitset<64> from(source.lane(lane,bits)),before(old.lane(lane,bits));std::bitset<64> expected;
        for(unsigned bit=0;bit<bits;bit++)expected[bit]=left?(bit<amount?before[bit]:from[bit-amount]):(bit+amount>=bits?before[bit]:from[bit+amount]);
        check(g.floating().v[rd].lane(lane,bits)==expected.to_ullong(),"Shift-insert changed preserved destination bits or incoming source bits");
      }
      if(!wide)check(!g.floating().v[rd].high,"64-bit shift-insert retained upper bits");
    }
  }
}
void scalar_element_copy(){
  for(unsigned size=0;size<4;size++)for(unsigned index=0;index<16/(1u<<size);index++)for(bool alias:{false,true}){
    const unsigned bytes=1u<<size,rd=alias?1:3,encoded=(index<<(size+1))|(1u<<size);
    const auto opcode=0x5e000400u|(encoded<<16)|(1<<5)|rd;
    MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x4c407041u,opcode}));
    for(unsigned n=0;n<16;n++)g.write(0x2000+n,n*11+5,1);
    auto split=MinimalGuest::from_snapshot(g.snapshot());g.run(3);for(unsigned n=0;n<3;n++)split.run(1);
    std::uint64_t expected=0;for(unsigned n=0;n<bytes;n++)expected|=std::uint64_t((index*bytes+n)*11+5)<<(8*n);
    check(g.floating().v[rd].low==expected&&!g.floating().v[rd].high,"Scalar element copy lost its selected lane or failed to clear other bits");
    check(g.snapshot()==split.snapshot(),"Scalar element copy alias or chunk changed complete state");
  }
}
void vector_reverse(){
  for(bool wide:{false,true})for(unsigned block_bits:{16u,32u,64u})for(unsigned size=0;(8u<<size)<block_bits;size++){
    const unsigned bytes=wide?16:8,element_bytes=1u<<size;
    const auto opcode=(block_bits==64?0x0e200823u:block_bits==32?0x2e200823u:0x0e201823u)|(wide?0x40000000u:0u)|(size<<22);
    MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x0c407041u|(wide?0x40000000u:0u),opcode}));
    for(unsigned n=0;n<bytes;n++)g.write(0x2000+n,n*7+3,1);auto split=MinimalGuest::from_snapshot(g.snapshot());g.run(3);for(unsigned n=0;n<3;n++)split.run(1);
    check(g.snapshot()==split.snapshot(),"Vector reverse changed across chunks");
    for(unsigned n=0;n<bytes;n++){
      const unsigned group=block_bits/8,within=n%group,from=(n/group)*group+(group/element_bytes-1-within/element_bytes)*element_bytes+within%element_bytes;
      check(g.floating().v[3].lane(n,8)==from*7+3,"Vector reverse changed element byte order or group extent");
    }
    if(!wide)check(!g.floating().v[3].high,"64-bit vector reverse retained upper bits");
  }
}
void vector_shifts(){
  const auto expected_shift=[](std::uint64_t x,int amount,unsigned bits,bool unsigned_value){
    std::bitset<64> source(x),result;
    for(unsigned bit=0;bit<bits;bit++){
      const int from=int(bit)-amount;
      result[bit]=from>=0&&from<int(bits)?source[unsigned(from)]:from>=int(bits)&&amount<0&&!unsigned_value?source[bits-1]:false;
    }return result.to_ullong();
  };
  for(bool wide:{false,true})for(unsigned size=0;size<4;size++)for(bool unsigned_value:{false,true}){
    const unsigned bits=8u<<size,bytes=wide?16:8,lanes=bytes*8/bits;if(!lanes||bits==64&&!wide)continue;
    const auto mask=bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1,sign=UINT64_C(1)<<(bits-1);
    for(int amount:{-128,-int(bits),-int(bits)+1,-7,-1,0,1,7,int(bits)-1,int(bits),127})for(bool immediate:{false,true}){
      if(immediate&&(amount>=int(bits)||amount<-int(bits)))continue;
      const auto opcode=immediate?
        ((amount>=0?0x0f005423u:0x0f000423u|(unsigned_value?0x20000000u:0u))|(wide?0x40000000u:0u)|(unsigned(amount>=0?int(bits)+amount:2*int(bits)+amount)<<16)):
        (0x0e224423u|(wide?0x40000000u:0u)|(unsigned_value?0x20000000u:0u)|(size<<22));
      MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x0c40a041u|(wide?0x40000000u:0u),opcode}));
      const std::uint64_t values[]{mask,sign,1,0,sign-1,37};
      for(unsigned n=0;n<lanes;n++){
        g.write(0x2000+std::uint64_t(n)*(bits/8),values[n%6],bits/8);
        g.write(0x2000+bytes+std::uint64_t(n)*(bits/8),(UINT64_C(0x5a6c179b34d2ee00)|std::uint8_t(amount))&mask,bits/8);
      }
      auto split=MinimalGuest::from_snapshot(g.snapshot());g.run(3);for(unsigned n=0;n<3;n++)split.run(1);
      check(g.snapshot()==split.snapshot(),"Vector shift state differs between chunks");
      for(unsigned n=0;n<lanes;n++)check(g.floating().v[3].lane(n,bits)==expected_shift(values[n%6],amount,bits,unsigned_value),"Vector shift disagrees with independent bit-position oracle");
      if(!wide)check(!g.floating().v[3].high,"64-bit vector shift retained upper bits");
    }
  }
}
void widening_pairs(){
  for(bool wide:{false,true})for(unsigned size=0;size<3;size++)for(bool unsigned_values:{false,true}){
    const unsigned bits=8u<<size,bytes=wide?16:8,lanes=bytes*8/bits;
    const auto sign=UINT64_C(1)<<(bits-1),unit=UINT64_C(1)<<bits;
    const auto instruction=0x0e202823u|(wide?0x40000000u:0u)|(unsigned_values?0x20000000u:0u)|(size<<22);
    MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x0c407041u|(wide?0x40000000u:0u),instruction}));
    std::vector<std::uint64_t> numbers;for(unsigned n=0;n<lanes;n++){numbers.push_back((n%3==0?unit-1:n%3==1?sign:37)&(unit-1));g.write(0x2000+std::uint64_t(n)*(bits/8),numbers.back(),bits/8);}
    auto split=MinimalGuest::from_snapshot(g.snapshot());g.run(3);for(unsigned n=0;n<3;n++)split.run(1);
    check(g.snapshot()==split.snapshot(),"Widening pairwise addition changed state across chunks");
    for(unsigned n=0;n<lanes/2;n++){
      const auto value=[&](unsigned at){const auto x=numbers[at];return unsigned_values||x<sign?std::int64_t(x):std::int64_t(x)-std::int64_t(unit);};
      const auto expected=std::uint64_t(value(2*n)+value(2*n+1));const auto mask=bits==32?UINT64_MAX:(UINT64_C(1)<<(2*bits))-1;
      check(g.floating().v[3].lane(n,2*bits)==(expected&mask),"Widening pair sum lost sign extension or carry");
    }
    if(!wide)check(!g.floating().v[3].high,"64-bit widening pair sum retained high bits");
  }
}
void byte_population(){
  for(bool wide:{false,true}){
    const unsigned bytes=wide?16:8;
    for(unsigned first=0;first<256;first+=bytes){
      MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x0c407042u|(wide?0x40000000u:0u),0x0e205842u|(wide?0x40000000u:0u)}));
      for(unsigned j=0;j<bytes;j++)g.write(0x2000+j,first+j,1);
      auto split=MinimalGuest::from_snapshot(g.snapshot());g.run(3);for(unsigned j=0;j<3;j++)split.run(1);
      check(g.snapshot()==split.snapshot(),"CNT alias or transaction boundary changed state");
      for(unsigned j=0;j<bytes;j++)check(g.floating().v[2].lane(j,8)==std::bitset<8>(first+j).count(),"CNT disagrees with exhaustive independent byte population count");
      if(!wide)check(!g.floating().v[2].high,"64-bit CNT retained upper bits");
    }
  }
}
void shift_narrow(){
  for(unsigned bits:{8u,16u,32u})for(unsigned shift=1;shift<=bits;shift++)for(bool upper:{false,true})for(bool rounded:{false,true})for(bool alias:{false,true}){
    const unsigned dest=alias?1:2;
    const auto opcode=0x0f008400u|(upper?0x40000000u:0u)|(rounded?0x800u:0u)|((2*bits-shift)<<16)|(1<<5)|dest;
    MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x4c40a041u,opcode}));
    for(unsigned j=0;j<32;j++)g.write(0x2000+j,((j*71+23)^(j%3?0:255))&255,1);
    g.run(2);const auto source=g.floating().v[1],old=g.floating().v[dest];auto restored=MinimalGuest::from_snapshot(g.snapshot());g.run(1);restored.run(1);
    check(g.snapshot()==restored.snapshot(),"Shift-narrow source alias or restore changed complete state");
    const auto& result=g.floating().v[dest];
    for(unsigned n=0;n<64/bits;n++){
      const auto x=source.lane(n,2*bits),unit=UINT64_C(1)<<shift;
      const auto expected=(x/unit+(rounded&&x%unit>=unit/2))&((UINT64_C(1)<<bits)-1);
      check(result.lane(n+(upper?64/bits:0),bits)==expected,"Shift-narrow width, rounding or truncation differs");
    }
    check(upper?result.low==old.low:result.high==0,"Shift-narrow changed its preserved or cleared half");
  }
}
void integer_reductions(){
  for(bool wide:{false,true})for(unsigned size=0;size<4;size++){
    if(size==3&&!wide)continue;
    const unsigned bytes=wide?16:8,bits=8u<<size,lanes=bytes*8/bits;
    const auto mask=bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1,sign=UINT64_C(1)<<(bits-1);
    std::vector<std::uint64_t> numbers;
    const std::uint64_t pattern[]{0,1,mask,sign,sign-1,sign+1,17,42};
    for(unsigned n=0;n<2*lanes;n++)numbers.push_back(pattern[n%8]&mask);
    for(unsigned operation=0;operation<6;operation++){
      if(size==3&&operation>=2)continue;
      const std::uint32_t bases[]{0x0e209800u,0x0e22bc00u,0x2e22ac00u,0x2e22a400u,0x0e22ac00u,0x0e22a400u};
      const auto opcode=bases[operation]|(wide?0x40000000u:0u)|(size<<22)|(1<<5)|3;
      const auto load=0x0c40a041u|(wide?0x40000000u:0u);
      MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,load,opcode}));
      for(unsigned n=0;n<numbers.size();n++)g.write(0x2000+std::uint64_t(n)*(bits/8),numbers[n],bits/8);
      auto sliced=MinimalGuest::from_snapshot(g.snapshot());g.run(3);for(unsigned n=0;n<3;n++)sliced.run(1);
      check(g.snapshot()==sliced.snapshot(),"Vector reduction changed across transaction boundaries");
      for(unsigned n=0;n<lanes;n++){
        std::uint64_t expected;
        if(operation==0)expected=numbers[n]==0?mask:0;
        else{
          const auto x=numbers[n*2],y=numbers[n*2+1];
          const auto xx=operation<4?x:x^sign,yy=operation<4?y:y^sign;
          expected=operation==1?(x+y)&mask:(operation==2||operation==4)?(xx<yy?x:y):(xx>yy?x:y);
        }
        check(g.floating().v[3].lane(n,bits)==expected,"Vector zero comparison or pairwise reduction differs from independent lane values");
      }
      if(!wide)check(g.floating().v[3].high==0,"64-bit vector reduction retained upper bits");
    }
  }
}
void structure_memory(){
  // Genuine first rejected Android memcpy opcode: LD1 {v1.16b,v2.16b},[x2],#32.
  // Validate every encoded arrangement against independent sequential bytes.
  for(unsigned count=1;count<=4;count++)for(bool wide:{false,true})for(unsigned size=0;size<4;size++)for(bool post:{false,true}){
    const unsigned operation=count==1?7:count==2?10:count==3?6:2,bytes=wide?16:8;
    const auto instruction=0x0c400000u|(wide?0x40000000u:0u)|(post?0x009f0000u:0u)|(operation<<12)|(size<<10)|(2<<5)|31;
    MinimalGuest g(65536);g.load_elf(elf({0xd2840022u,instruction})); // MOV X2,#0x2001 (unaligned byte base)
    const auto base=UINT64_C(0x2001);
    for(unsigned j=0;j<count*bytes;j++)g.write(base+j,(j*37+11)&255,1);
    g.run(1);const auto input=g.snapshot();auto restored=MinimalGuest::from_snapshot(input);g.run(1);restored.run(1);
    check(g.snapshot()==restored.snapshot(),"Structure load changed across a full-state restore");
    check(g.registers().x[2]==base+(post?count*bytes:0),"LD1 post-index count or no-offset base differs");
    for(unsigned n=0;n<count;n++){
      const auto& v=g.floating().v[(31+n)&31];
      for(unsigned j=0;j<bytes;j++)check(v.lane(j,8)==((n*bytes+j)*37+11)%256,"LD1 lost an element, order or register wraparound");
      if(!wide)check(v.high==0,"64-bit LD1 preserved stale upper vector bits");
    }
  }
  // The actual two-register load and store with immediate post-index.
  MinimalGuest copy(65536);copy.load_elf(elf({0xd2840002u,0xd2860003u,0x4cdfa041u,0x4c9fa061u}));
  for(unsigned j=0;j<32;j++)copy.write(0x2000+j,0xf0-j,1);
  copy.run(4);for(unsigned j=0;j<32;j++)check(copy.read(0x3000+j,1)==0xf0-j,"ST1 changed the actual loaded byte sequence");
  check(copy.registers().x[2]==0x2020&&copy.registers().x[3]==0x3020,"Structure copy post-index differs");
  // A later element outside RAM must roll back vectors and base together.
  MinimalGuest bad(65536);bad.load_elf(elf({0xd29ffe02u,0x4cdfa041u}));bad.run(1);const auto before=bad.snapshot();
  fails([&]{bad.run(1);},"Structure load beyond RAM was accepted");check(bad.snapshot()==before,"Rejected LD1 committed a vector or post-index");
}
void arithmetic(){
  FloatingState state;ArmFloat fp(state);
  check(fp.binary(2,32,0x3f800000,0x40000000)==0x40400000,"FP32 addition");
  check(fp.sqrt(64,UINT64_C(0x4010000000000000))==UINT64_C(0x4000000000000000),"FP64 square root");
  state.fpsr=0;check(fp.binary(1,32,0x3f800000,0)==0x7f800000&&state.fpsr==2,"Divide by zero result/flag");
  state.fpsr=0;check(fp.binary(1,32,0,0)==0x7fc00000&&state.fpsr==1,"Invalid result/flag");
  state.fpsr=0;check(fp.binary(4,32,0,0x80000000)==0&&fp.binary(5,32,0,0x80000000)==0x80000000,"Min/max signed zero");
  check(fp.binary(6,32,0x7fc12345,0x3f800000)==0x3f800000,"Maximum number and quiet NaN");
  check(fp.binary(6,32,0x7f812345,0x3f800000)==0x7fc12345&&state.fpsr==1,"Signaling NaN propagation");
  state.fpsr=0;state.fpcr=1u<<22;
  check(fp.binary(2,32,0x3f800000,0x33800000)==0x3f800001&&state.fpsr==16,"Round toward positive infinity");
  state.fpcr=2u<<22;check(fp.binary(2,32,0x3f800000,0x33800000)==0x3f800000,"Round toward negative infinity");
  state.fpcr=1u<<24;state.fpsr=0;
  check(fp.binary(2,32,1,0)==0&&state.fpsr==128,"FZ input denormal and IDC");
  state.fpsr=0;check(fp.binary(0,32,0x00800000,0x3f000000)==0&&state.fpsr==8,"FZ output underflow");
  state.fpcr=1u<<25;state.fpsr=0;
  check(fp.binary(2,32,0x7fc12345,0)==0x7fc00000,"Default NaN mode");
  state.fpcr=0;state.fpsr=0;
  check(fp.fma(32,0x7fc12345,0x40000000,0x7fc54321,false,false)==0x7fc54321,"FMA addend NaN priority");
  check(fp.fma(32,0x7f812345,0x7f854321,0x7f811111,false,false)==0x7fc11111&&state.fpsr==1,"FMA signaling addend priority/flag");
  state.fpsr=0;check(fp.fma(32,0x7f800000,0,0x7fc12345,false,false)==0x7fc00000&&state.fpsr==1,"FMA 0*infinity+quiet-NaN");
  state.fpcr=1u<<25;check(fp.convert(32,16,0x7fc12345)==0x7e00,"Half conversion default NaN");state.fpcr=0;state.fpsr=0;
  check(fp.to_int(64,UINT64_C(0xc004000000000000),true,64,softfloat_round_minMag)==UINT64_MAX-1&&state.fpsr==16,"Signed conversion truncation and IXC");
  check(fp.to_int(32,0x7fc12345,true,32,softfloat_round_minMag)==0&&(state.fpsr&1),"NaN conversion saturation");
  auto worker=[](unsigned mode,std::uint64_t expected){FloatingState s;s.fpcr=mode<<22;ArmFloat f(s);for(unsigned n=0;n<20000;++n)if(f.binary(2,32,0x3f800000,0x33800000)!=expected)return false;return true;};
  auto up=std::async(std::launch::async,worker,1,0x3f800001),down=std::async(std::launch::async,worker,2,0x3f800000);
  check(up.get()&&down.get(),"Concurrent guest rounding modes contaminated one another");
}
void sparse_state(){
  FloatingState state;check(state.save().empty(),"Zero FP state should occupy no snapshot payload");
  state.v[31]={1,UINT64_MAX};check(state.save().size()==36,"One nonzero register should need one mask bit and 16 bytes");
  check(FloatingState::restore(state.save()).save()==state.save(),"Sparse FP state is not exact");
  for(auto& v:state.v)v={UINT64_MAX,UINT64_MAX};state.fpcr=0x03c00000;state.fpsr=0x0800009f;
  check(state.save().size()==532&&FloatingState::restore(state.save()).save()==state.save(),"Full FP register state exceeds finite bound or loses bits");
  auto malformed=state.save();malformed.pop_back();fails([&]{FloatingState::restore(malformed);},"Truncated FP state accepted");
  malformed=state.save();malformed[8]|=1;fails([&]{FloatingState::restore(malformed);},"Unsupported FPCR state accepted");
  malformed={'R','F','P','1',13,10,26,10};malformed.resize(20);fails([&]{FloatingState::restore(malformed);},"Redundant all-zero FP payload accepted");
}
void architectural_state(){
  // Opcodes independently assembled and covered again by the full QEMU witness.
  const auto image=elf({0x1e2e1000,0x1e201001,0x1e212802,0x1e260040,0x4f02e443,0x4e238463});
  MinimalGuest whole(65536);whole.load_elf(image);
  check(whole.snapshot()[3]=='1',"Legacy integer checkpoint changed before any FP state exists");
  whole.apply_xi_delta("h 4095\n");const auto core=whole.runtime().core_snapshot();
  auto sliced=MinimalGuest::from_snapshot(whole.snapshot());whole.run(6);
  for(unsigned n=0;n<6;++n){sliced.run(1);sliced=MinimalGuest::from_snapshot(sliced.snapshot());}
  check(whole.registers().x[0]==0x40400000,"FMOV/FADD scalar instruction decoder");
  check(whole.floating().v[3].low==UINT64_C(0x8484848484848484)&&whole.floating().v[3].high==whole.floating().v[3].low,"MOVI/ADD vector decoder");
  check(whole.snapshot()[3]=='3'&&whole.snapshot()==sliced.snapshot(),"RVG3 sliced execution is not bit-exact");
  check(whole.runtime().core_snapshot()==core,"FP/SIMD execution changed original quantum core");
  check(whole.runtime().memory().stats().overrides==0,"FP registers allocated guest RAM differences");
  const auto before=whole.snapshot();fails([&]{whole.run(1);},"End of executable accepted");check(before==whole.snapshot(),"Rejected instruction changed FP/CPU/core state");
  whole.load_elf(elf({0x1e2703e0}));whole.run(1);check(whole.snapshot()[3]=='1'&&!whole.floating().nonzero(),"Writing a zero register materialized redundant state");
  const auto trap_image=elf({0x1e2e1000},true);MinimalGuest trap(128*1024*1024);trap.load_elf(trap_image,true);trap.run(1);
  check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"CPACR FPEN did not trap before changing a register");
  // MSR CPACR_EL1,x0, with x0 built from a MOVZ of the FPEN field.
  MinimalGuest allowed(128*1024*1024);allowed.load_elf(elf({0xd2a00600,0xd5181040,0x1e2e1000},true),true);allowed.run(3);
  check(allowed.machine().exceptions==0&&allowed.floating().v[0].low==0x3f800000,"Enabled FPEN rejected EL1 FP");
  check(MinimalGuest::from_snapshot(allowed.snapshot()).snapshot()==allowed.snapshot(),"System FP checkpoint lost architectural state");
}
}
int main(){try{arithmetic();sparse_state();architectural_state();structure_memory();integer_reductions();shift_narrow();byte_population();widening_pairs();vector_shifts();vector_reverse();scalar_element_copy();shift_insert();std::cout<<"{\"checks\":"<<checks<<",\"fp\":\"ARMv8 bounded FP32/FP64 + SIMD\",\"state\":\"sparse RVG3\",\"quantum_engine\":\"unchanged\"}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
