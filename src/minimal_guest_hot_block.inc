// C39: decoded ARM64 hot instructions. Slots are grouped by 64-byte physical
// basic-code regions; guest permission checks, faults, timer and IRQ dispatch
// are still performed by the original machine on EVERY architectural step.
// Do not promote loads/stores, barriers, exception returns or system ops:
// those continue through the reference instruction interpreter.
inline bool MinimalGuest::try_hot_step(unsigned kind,std::uint32_t instruction,std::uint64_t pc) {
  auto& cpu=registers_;
  const unsigned bits=(instruction>>31)?64:32,rd=instruction&31,rn=(instruction>>5)&31,rm=(instruction>>16)&31;
  std::uint64_t next=pc+4;
  switch(kind){
    case 1: break; // accepted architectural NOP/BTI/CSDB hints only
    case 2: { // MOVN/MOVZ/MOVK
      const unsigned opcode=(instruction>>29)&3,position=((instruction>>21)&3)*16;
      const auto value=std::uint64_t((instruction>>5)&0xffff)<<position;
      set(rd,opcode==0?~value:opcode==2?value:(reg(rd,bits)&~(UINT64_C(0xffff)<<position))|value,bits);
      break;
    }
    case 3: { // ADD/SUB immediate; stack register semantics preserved
      const bool subtract=instruction&0x40000000,update=instruction&0x20000000;
      const auto a=reg(rn,bits,true),b=std::uint64_t((instruction>>10)&4095)<<((instruction&0x00400000)?12:0);
      const auto value=(subtract?a-b:a+b)&mask(bits);
      if(update)flags(a,b,value,bits,subtract);
      set(rd,value,bits,!update);break;
    }
    case 4: { // ADD/SUB shifted register
      const unsigned amount=(instruction>>10)&63,shift_kind=(instruction>>22)&3;
      const bool subtract=instruction&0x40000000,update=instruction&0x20000000;
      const auto a=reg(rn,bits),b=shift(reg(rm,bits),amount,shift_kind,bits),value=(subtract?a-b:a+b)&mask(bits);
      if(update)flags(a,b,value,bits,subtract);
      set(rd,value,bits);break;
    }
    case 5: { // logical shifted register, including BIC/ORN/EON variants
      const unsigned opcode=(instruction>>29)&3,amount=(instruction>>10)&63;
      const auto a=reg(rn,bits);auto b=shift(reg(rm,bits),amount,(instruction>>22)&3,bits);
      if(instruction&0x00200000)b=(~b)&mask(bits);
      const auto value=(opcode==0||opcode==3)?a&b:opcode==1?a|b:a^b;
      set(rd,value,bits);
      if(opcode==3)cpu.nzcv=(value&(UINT64_C(1)<<(bits-1))?8:0)|(value==0?4:0);
      break;
    }
    case 6: { // ADR/ADRP
      const auto immediate=signed_field((((instruction>>5)&0x7ffff)<<2)|((instruction>>29)&3),21);
      const bool page=instruction&0x80000000;
      set(rd,(page?(pc&~UINT64_C(4095)):pc)+std::uint64_t(immediate)*(page?4096:1));break;
    }
    case 7: { // B/BL
      if(instruction&0x80000000)set(30,next);
      next=pc+std::uint64_t(signed_field(instruction&0x3ffffff,26))*4;break;
    }
    case 8: { // B.cond
      if(condition(instruction&15))next=pc+std::uint64_t(signed_field((instruction>>5)&0x7ffff,19))*4;break;
    }
    case 9: { // CBZ/CBNZ
      if((reg(rd,bits)!=0)==bool(instruction&0x01000000))next=pc+std::uint64_t(signed_field((instruction>>5)&0x7ffff,19))*4;break;
    }
    case 10: { // TBZ/TBNZ
      const unsigned bit=((instruction>>31)<<5)|((instruction>>19)&31);
      if(bool((reg(rd)>>bit)&1)==bool(instruction&0x01000000))next=pc+std::uint64_t(signed_field((instruction>>5)&0x3fff,14))*4;break;
    }
    case 11: { // MADD/MSUB, including MUL alias
      const auto product=reg(rn,bits)*reg(rm,bits),accumulator=reg((instruction>>10)&31,bits);
      set(rd,(instruction&0x8000)?accumulator-product:accumulator+product,bits);break;
    }
    case 12: { // CSEL/CSINC/CSINV/CSNEG
      auto value=reg(rn,bits);
      if(!condition((instruction>>12)&15)){value=reg(rm,bits);if(instruction&0x40000000)value=~value;if(instruction&0x400)++value;}
      set(rd,value,bits);break;
    }
    case 13: { // ADC/ADCS/SBC/SBCS
      const bool subtract=instruction&0x40000000,update=instruction&0x20000000;
      const auto a=reg(rn,bits),b=subtract?(~reg(rm,bits)&mask(bits)):reg(rm,bits),carry=std::uint64_t((cpu.nzcv>>1)&1);
      const auto partial=(a+b)&mask(bits),value=(partial+carry)&mask(bits);
      if(update){const auto sign=UINT64_C(1)<<(bits-1);cpu.nzcv=(value&sign?8:0)|(value==0?4:0)|
        ((partial<a||value<partial)?2:0)|((~(a^b)&(a^value)&sign)?1:0);}
      set(rd,value,bits);break;
    }
    default: return false; // never emulate an unclassified or privileged opcode
  }
  require(!(next&3),"Guest branch target is unaligned");
  if(!machine_.enabled)range(next,cpu.halted?0:4);
  cpu.pc=next;advance();return true;
}
