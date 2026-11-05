// Included inside reverse_pc::guest after MinimalGuest's declaration.
inline bool MinimalGuest::fp_instruction(std::uint32_t i,std::uint64_t pc,RunResult& result){
  const unsigned rd=i&31,rn=(i>>5)&31,rm=(i>>16)&31,type=(i>>22)&3;
  const unsigned bits=type==0?32:type==1?64:type==3?16:0;
  auto scalar=[&](unsigned index){return floating_.v[index].lane(0,bits);};
  auto valid_float=[&](){return bits==32||bits==64;};
  auto word_size=[&](){const auto opcode=(i>>22)&3,size=i>>30;return opcode&2?(size==0?16u:0u):(1u<<size);};
  if((i&0xffe0fc00u)==0x5e000400u){ // DUP element to scalar B/H/S/D, including MOV Dd,Vn.D[index].
    const unsigned immediate=(i>>16)&31;if(!immediate)return false;
    unsigned size=0;while(!(immediate&(1u<<size)))++size;if(size>3)return false;
    const unsigned lane_bits=8u<<size,index=immediate>>(size+1);
    fp_access();const auto value=floating_.v[rn].lane(index,lane_bits);fp_set(rd,value,lane_bits);return true;
  }
  if((i&0xfffffc00u)==0x5ef1b800u){ // ADDP Dd,Vn.2D: integer pairwise scalar reduction.
    fp_access();const auto source=floating_.v[rn];
    // Unsigned addition wraps modulo 2^64; copy both lanes before aliasing Rd.
    // Scalar writes clear the upper 64 bits and leave NZCV/FPCR/FPSR unchanged.
    fp_set(rd,source.low+source.high,64);return true;
  }
  const auto scalar_compare=i&0xdf20fc00u;
  if(scalar_compare==0x5e203400u||scalar_compare==0x5e203c00u||scalar_compare==0x5e208c00u){ // CMGT/CMHI, CMGE/CMHS, CMTST/CMEQ Dd,Dn,Dm.
    if(type!=3)return false;fp_access();const auto a=floating_.v[rn].low,b=floating_.v[rm].low;
    const bool unsigned_form=i&0x20000000u;bool match;
    if(scalar_compare==0x5e208c00u)match=unsigned_form?a==b:bool(a&b);
    else{
      const auto bias=unsigned_form?UINT64_C(0):UINT64_C(0x8000000000000000),x=a^bias,y=b^bias;
      match=scalar_compare==0x5e203400u?x>y:x>=y;
    }
    fp_set(rd,match?UINT64_MAX:0,64);return true;
  }
  const auto scalar_compare_zero=i&0xff3ffc00u;
  if(scalar_compare_zero==0x5e208800u||scalar_compare_zero==0x7e208800u||scalar_compare_zero==0x5e209800u||scalar_compare_zero==0x7e209800u||scalar_compare_zero==0x5e20a800u){ // Scalar CMGT/CMGE/CMEQ/CMLE/CMLT #0.
    if(type!=3)return false;fp_access();const auto x=floating_.v[rn].low;const bool negative=x&UINT64_C(0x8000000000000000);
    const bool match=scalar_compare_zero==0x5e208800u?!negative&&x:scalar_compare_zero==0x7e208800u?!negative:scalar_compare_zero==0x5e209800u?!x:scalar_compare_zero==0x7e209800u?negative||!x:negative;
    fp_set(rd,match?UINT64_MAX:0,64);return true;
  }
  if((i&0xbf40d000u)==0x0d40c000u){ // LD1R/LD2R/LD3R/LD4R, replicate one element per vector.
    const bool post=i&0x00800000u,wide=i&0x40000000u;
    if(!post&&rm)return false;
    const unsigned count=1+((i>>21)&1)+2*((i>>13)&1),bytes=1u<<((i>>10)&3),lanes=(wide?16:8)/bytes;
    fp_access();const auto base=reg(rn,64,true),increment=post?(rm==31?std::uint64_t(count*bytes):reg(rm)):0;
    std::array<Vector128,4> values{};
    for(unsigned n=0;n<count;n++)check_access(base+std::uint64_t(n)*bytes,bytes,false);
    for(unsigned n=0;n<count;n++){
      const auto x=cpu_read(base+std::uint64_t(n)*bytes,bytes,result);
      for(unsigned lane=0;lane<lanes;lane++)values[n].lane(lane,bytes*8,x);
    }
    for(unsigned n=0;n<count;n++)floating_.v[(rd+n)&31]=values[n];
    if(post)set(rn,base+increment,64,true);return true;
  }
  if((i&0xbf000000u)==0x0d000000u){ // LD1/2/3/4 and ST1/2/3/4, one lane per register.
    const unsigned opcode=(i>>13)&7,scale=opcode>>1,s=(i>>12)&1,size=(i>>10)&3;
    const bool post=i&0x00800000u,load=i&0x00400000u;
    const unsigned q=(i>>30)&1,count=1+((i>>21)&1)+2*(opcode&1);
    if(scale==3||(!post&&rm))return false; // Replicates are handled separately above.
    unsigned lane_bits=8,lane=(q<<3)|(s<<2)|size;
    if(scale==1){if(size&1)return false;lane_bits=16;lane=(q<<2)|(s<<1)|(size>>1);}
    if(scale==2){
      if((size&2)||((size&1)&&s))return false;
      lane_bits=size&1?64:32;lane=size&1?q:(q<<1)|s;
    }
    fp_access();const unsigned bytes=lane_bits/8;const auto base=reg(rn,64,true);
    const auto increment=post?(rm==31?std::uint64_t(count*bytes):reg(rm)):0;
    std::array<Vector128,4> values{};
    for(unsigned n=0;n<count;n++)check_access(base+std::uint64_t(n)*bytes,bytes,!load);
    for(unsigned n=0;n<count;n++)values[n]=floating_.v[(rd+n)&31];
    for(unsigned n=0;n<count;n++){
      const auto address=base+std::uint64_t(n)*bytes;
      if(load)values[n].lane(lane,lane_bits,cpu_read(address,bytes,result));
      else cpu_write(address,values[n].lane(lane,lane_bits),bytes,result);
    }
    if(load)for(unsigned n=0;n<count;n++)floating_.v[(rd+n)&31]=values[n];
    if(post)set(rn,base+increment,64,true);return true;
  }
  // LD1/ST1 sequential registers and LD2/3/4-ST2/3/4 interleaved structures.
  // Traverse memory in architectural element order; Q selects 64/128-bit vectors.
  if((i&0xbf200000u)==0x0c000000u){
    const bool post=i&0x00800000u,load=i&0x00400000u,wide=i&0x40000000u;
    const unsigned operation=(i>>12)&15,element_bytes=1u<<((i>>10)&3),vector_bytes=wide?16:8;
    const bool interleaved=operation==0||operation==4||operation==8;
    const unsigned count=operation==0?4:operation==4?3:operation==8?2:
                         operation==7?1:operation==10?2:operation==6?3:operation==2?4:0;
    if(!count||(!post&&rm)||(interleaved&&!wide&&element_bytes==8))return false;
    fp_access();const auto base=reg(rn,64,true),increment=post?(rm==31?std::uint64_t(count*vector_bytes):reg(rm)):0;
    const unsigned lanes=vector_bytes/element_bytes,total=count*lanes;
    std::array<Vector128,4> values{};
    // Preflight every transfer before any destination register or memory write.
    for(unsigned k=0;k<total;k++)check_access(base+std::uint64_t(k)*element_bytes,element_bytes,!load);
    if(!load)for(unsigned n=0;n<count;n++)values[n]=floating_.v[(rd+n)&31];
    for(unsigned k=0;k<total;k++){
      const unsigned n=interleaved?k%count:k/lanes,lane=interleaved?k/count:k%lanes;
      const auto address=base+std::uint64_t(k)*element_bytes;
      if(load)values[n].lane(lane,element_bytes*8,cpu_read(address,element_bytes,result));
      else cpu_write(address,values[n].lane(lane,element_bytes*8),element_bytes,result);
    }
    if(load)for(unsigned n=0;n<count;n++)floating_.v[(rd+n)&31]=values[n];
    if(post)set(rn,base+increment,64,true);return true;
  }
  // Scalar and Q memory operations use the same physical XI/Ether bytes.
  if((i&0x3b000000)==0x39000000 && (i&0x04000000)){
    const auto bytes=word_size();if(!bytes)return false;fp_access();
    const auto address=reg(rn,64,true)+std::uint64_t((i>>10)&4095)*bytes;
    if(i&0x00400000)floating_.v[rd]=fp_load(address,bytes,result);else fp_store(address,floating_.v[rd],bytes,result);return true;
  }
  if((i&0x3b000000)==0x18000000 && (i&0x04000000)){
    const auto opcode=i>>30;if(opcode==3)return false;fp_access();
    floating_.v[rd]=fp_load(pc+std::uint64_t(signed_field((i>>5)&0x7ffff,19))*4,4u<<opcode,result);return true;
  }
  if((i&0x3b200000)==0x38000000 && (i&0x04000000)){
    const auto bytes=word_size(),mode=(i>>10)&3;if(!bytes||mode==2)return false;fp_access();
    const auto base=reg(rn,64,true),updated=base+std::uint64_t(signed_field((i>>12)&511,9)),address=mode==1?base:updated;
    if(i&0x00400000)floating_.v[rd]=fp_load(address,bytes,result);else fp_store(address,floating_.v[rd],bytes,result);
    if(mode)set(rn,updated,64,true);return true;
  }
  if((i&0x3b200c00)==0x38200800 && (i&0x04000000)){
    const auto bytes=word_size(),option=(i>>13)&7;if(!bytes||!(option&2))return false;fp_access();
    auto offset=reg(rm,option&1?64:32);if(option==6&&(offset&0x80000000))offset|=UINT64_C(0xffffffff00000000);
    if(i&0x1000){unsigned scale=0;while((1u<<scale)<bytes)++scale;offset<<=scale;}
    const auto address=reg(rn,64,true)+offset;
    if(i&0x00400000)floating_.v[rd]=fp_load(address,bytes,result);else fp_store(address,floating_.v[rd],bytes,result);return true;
  }
  if((i&0x3a000000)==0x28000000 && (i&0x04000000)){
    const auto opcode=i>>30,second=(i>>10)&31,mode=(i>>23)&3;if(opcode==3)return false;
    const bool load=i&0x00400000;if(load&&rd==second)return false;fp_access();const unsigned bytes=4u<<opcode;
    const auto base=reg(rn,64,true),updated=base+std::uint64_t(signed_field((i>>15)&127,7))*bytes,address=mode==1?base:updated;
    check_access(address,bytes,!load);check_access(address+bytes,bytes,!load);
    if(load){const auto first=fp_load(address,bytes,result),last=fp_load(address+bytes,bytes,result);floating_.v[rd]=first;floating_.v[second]=last;}
    else{fp_store(address,floating_.v[rd],bytes,result);fp_store(address+bytes,floating_.v[second],bytes,result);}
    if(mode==1||mode==3)set(rn,updated,64,true);return true;
  }
  const auto indexed_integer=i&0xbf00f400u;
  if(indexed_integer==0x0f008000u||indexed_integer==0x2f000000u||indexed_integer==0x2f004000u){ // MUL/MLA/MLS, vector by element, H/S.
    if(type!=1&&type!=2)return false;
    const unsigned lane_bits=8u<<type,vector_bits=(i&0x40000000u)?128u:64u;
    const unsigned h=(i>>11)&1u,l=(i>>21)&1u,m=(i>>20)&1u;
    const unsigned index=type==1?(h<<2)|(l<<1)|m:(h<<1)|l,source_register=type==1?rm&15u:rm;
    fp_access();const auto source=floating_.v[rn],accumulator=floating_.v[rd];
    const auto factor=floating_.v[source_register].lane(index,lane_bits);Vector128 output{};
    for(unsigned lane=0;lane<vector_bits/lane_bits;++lane){
      const auto product=source.lane(lane,lane_bits)*factor,old=accumulator.lane(lane,lane_bits);
      output.lane(lane,lane_bits,indexed_integer==0x0f008000u?product:indexed_integer==0x2f000000u?old+product:old-product);
    }
    floating_.v[rd]=output;return true; // Snapshot all aliases before writes; Q=0 clears the upper half.
  }
  const auto indexed_widening=indexed_integer&~0x20000000u;
  if(indexed_widening==0x0f00a000u||indexed_widening==0x0f002000u||indexed_widening==0x0f006000u){ // S/U MULL, MLAL, MLSL by element, including upper-half variants.
    if(type!=1&&type!=2)return false;
    const unsigned lane_bits=8u<<type,destination_bits=2*lane_bits,start=(i&0x40000000u)?64u/lane_bits:0u;
    const unsigned h=(i>>11)&1u,l=(i>>21)&1u,m=(i>>20)&1u;
    const unsigned index=type==1?(h<<2)|(l<<1)|m:(h<<1)|l,source_register=type==1?rm&15u:rm;
    const bool unsigned_form=i&0x20000000u;
    const auto extended=[&](std::uint64_t value){return !unsigned_form&&(value&(UINT64_C(1)<<(lane_bits-1)))?value|(UINT64_MAX<<lane_bits):value;};
    fp_access();const auto source=floating_.v[rn],accumulator=floating_.v[rd];
    const auto factor=extended(floating_.v[source_register].lane(index,lane_bits));Vector128 output{};
    for(unsigned lane=0;lane<64u/lane_bits;++lane){
      const auto product=extended(source.lane(start+lane,lane_bits))*factor,old=accumulator.lane(lane,destination_bits);
      output.lane(lane,destination_bits,indexed_widening==0x0f00a000u?product:indexed_widening==0x0f002000u?old+product:old-product);
    }
    floating_.v[rd]=output;return true; // Unsigned two's-complement arithmetic gives exact modular signed results.
  }
  const bool rounding_vector=(i&0x9f80ec00u)==0x0f002400u;
  const bool rounding_scalar=(i&0xdf80ec00u)==0x5f002400u;
  if((i&0x00780000u)&&(rounding_vector||rounding_scalar)){ // S/URSHR and S/URSRA, scalar D or vector B/H/S/D.
    const unsigned encoded=(i>>16)&127u;unsigned lane_bits=8;
    while(lane_bits*2<=encoded)lane_bits*=2;
    const bool wide=i&0x40000000u,unsigned_form=i&0x20000000u,accumulate=i&0x1000u;
    if((rounding_scalar&&lane_bits!=64)||(!rounding_scalar&&!wide&&lane_bits==64))return false;
    const unsigned amount=2*lane_bits-encoded,lanes=rounding_scalar?1u:(wide?128u:64u)/lane_bits;
    const auto mask=lane_bits==64?UINT64_MAX:(UINT64_C(1)<<lane_bits)-1;
    fp_access();const auto source=floating_.v[rn],old=floating_.v[rd];Vector128 output{};
    for(unsigned lane=0;lane<lanes;++lane){
      const auto x=source.lane(lane,lane_bits);auto shifted=amount==lane_bits?UINT64_C(0):x>>amount;
      if(!unsigned_form&&(x&(UINT64_C(1)<<(lane_bits-1))))
        shifted|=amount==lane_bits?mask:mask^(mask>>amount);
      // Quotient plus the discarded rounding bit avoids a 65-bit sum and
      // never shifts a C++ word by 64. Lane writes reduce modulo 2^esize.
      const auto rounded=shifted+((x>>(amount-1))&1u);
      output.lane(lane,lane_bits,rounded+(accumulate?old.lane(lane,lane_bits):0));
    }
    floating_.v[rd]=output;return true; // Capture aliases first; clear unwritten scalar/Q=0 bits.
  }
  if((i&0x9f20dc00u)==0x0e204000u){ // ADDHN/RADDHN, SUBHN/RSUBHN and upper-half variants.
    if(type==3)return false;
    const unsigned narrow_bits=8u<<type,source_bits=2*narrow_bits;
    const bool upper=i&0x40000000u,rounded=i&0x20000000u,subtract=i&0x2000u;
    const auto mask=source_bits==64?UINT64_MAX:(UINT64_C(1)<<source_bits)-1;
    const auto bias=rounded?UINT64_C(1)<<(narrow_bits-1):UINT64_C(0);
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm],old=floating_.v[rd];
    Vector128 output=upper?old:Vector128{};const unsigned first=upper?64u/narrow_bits:0;
    for(unsigned lane=0;lane<128u/source_bits;++lane){
      const auto x=a.lane(lane,source_bits),y=b.lane(lane,source_bits);
      const auto temporary=((subtract?x-y:x+y)+bias)&mask;
      output.lane(first+lane,narrow_bits,temporary>>narrow_bits);
    }
    floating_.v[rd]=output;return true; // Sources precede aliased writes; *2 preserves the lower half.
  }
  ArmFloat fp(floating_);
  const auto scalar_pairwise_fp=i&0xff3ffc00u;
  if(scalar_pairwise_fp==0x7e30d800u||scalar_pairwise_fp==0x7e30f800u||scalar_pairwise_fp==0x7e30c800u){ // FADDP/FMAXP/FMINP/FMAXNMP/FMINNMP, scalar S/D.
    const bool minimum=i&0x00800000u;
    if(scalar_pairwise_fp==0x7e30d800u&&minimum)return false;
    const unsigned lane_bits=(i&0x00400000u)?64u:32u;
    fp_access();const auto source=floating_.v[rn];
    const unsigned operation=scalar_pairwise_fp==0x7e30d800u?2u:(scalar_pairwise_fp==0x7e30c800u?6u:4u)+unsigned(minimum);
    fp_set(rd,fp.binary(operation,lane_bits,source.lane(0,lane_bits),source.lane(1,lane_bits)),lane_bits);
    return true; // Capture both source lanes before an aliased scalar write clears the rest.
  }
  if((i&0x9f80fc00u)==0x0f00fc00u||(i&0xdf80fc00u)==0x5f00fc00u){ // FCVTZS/FCVTZU SIMD scalar/vector, fixed-point.
    const bool scalar=(i&0xdf80fc00u)==0x5f00fc00u,wide=i&0x40000000u;
    const unsigned encoded=(i>>16)&127u,lane_bits=encoded>=64?64u:32u;
    if(encoded<32||(!scalar&&!wide&&lane_bits==64))return false;
    const unsigned fracbits=2*lane_bits-encoded;
    fp_access();const auto source=floating_.v[rn];Vector128 output;
    const unsigned lanes=scalar?1u:(wide?128u:64u)/lane_bits;
    for(unsigned lane=0;lane<lanes;++lane)
      output.lane(lane,lane_bits,fp.to_fixed(lane_bits,source.lane(lane,lane_bits),!(i&0x20000000u),fracbits));
    floating_.v[rd]=output;return true;
  }
  if((i&0x9fbffc00u)==0x0ea1b800u){ // FCVTZS/FCVTZU Vd.2S/4S/2D,Vn.2S/4S/2D, round toward zero.
    const bool wide=i&0x40000000u;const unsigned lane_bits=i&0x00400000u?64u:32u;
    if(!wide&&lane_bits==64)return false;
    fp_access();const auto source=floating_.v[rn];Vector128 output;
    for(unsigned lane=0;lane<(wide?128u:64u)/lane_bits;++lane)
      output.lane(lane,lane_bits,fp.to_int(lane_bits,source.lane(lane,lane_bits),!(i&0x20000000u),lane_bits,softfloat_round_minMag));
    floating_.v[rd]=output;return true;
  }
  if((i&0x9fbffc00u)==0x0e21d800u){ // SCVTF/UCVTF Vd.2S/4S/2D,Vn.2S/4S/2D.
    const bool wide=i&0x40000000u;const unsigned lane_bits=i&0x00400000u?64u:32u;
    if(!wide&&lane_bits==64)return false; // The 1D vector form is unallocated.
    fp_access();const auto source=floating_.v[rn];Vector128 output;
    for(unsigned lane=0;lane<(wide?128u:64u)/lane_bits;++lane)
      output.lane(lane,lane_bits,fp.from_int(lane_bits,source.lane(lane,lane_bits),!(i&0x20000000u),lane_bits));
    floating_.v[rd]=output;return true;
  }
  if((i&0xdfbffc00u)==0x5ea1b800u){ // FCVTZS/FCVTZU Sd,Sn or Dd,Dn (AdvSIMD scalar).
    const unsigned scalar_bits=(i&0x00400000u)?64u:32u;
    fp_access();const auto value=floating_.v[rn].lane(0,scalar_bits);
    fp_set(rd,fp.to_int(scalar_bits,value,!(i&0x20000000u),scalar_bits,softfloat_round_minMag),scalar_bits);
    return true;
  }
  if((i&0xdfbffc00u)==0x5e21d800u){ // SCVTF/UCVTF Sd,Sn or Dd,Dn (AdvSIMD scalar).
    const unsigned scalar_bits=(i&0x00400000u)?64u:32u;
    fp_access();const auto value=floating_.v[rn].lane(0,scalar_bits);
    fp_set(rd,fp.from_int(scalar_bits,value,!(i&0x20000000u),scalar_bits),scalar_bits);
    return true;
  }
  if((i&0xff201fe0)==0x1e201000){ // FMOV immediate
    if(!valid_float())return false;fp_access();const unsigned imm=(i>>13)&255,e=bits==32?8:11,f=bits-e-1;
    const unsigned repeat=(imm>>6)&1;
    const auto exponent=(std::uint64_t(!repeat)<<(e-1))|(repeat?((UINT64_C(1)<<(e-3))-1)<<2:0)|((imm>>4)&3);
    fp_set(rd,(std::uint64_t(imm>>7)<<(bits-1))|(exponent<<f)|(std::uint64_t(imm&15)<<(f-4)),bits);return true;
  }
  if((i&0xbf3ffc00u)==0x2e213800u){ // SHLL/SHLL2 8B->8H, 4H->4S, 2S->2D, by source element size.
    const unsigned size=(i>>22)&3u;if(size==3)return false;
    const unsigned lane_bits=8u<<size,lanes=64u/lane_bits,first=(i&0x40000000u)?lanes:0;
    fp_access();const auto source=floating_.v[rn];Vector128 output;
    // Capture the entire source before writing an aliased destination. Unsigned
    // shifts preserve the architectural low 2*esize bits without signed UB.
    for(unsigned lane=0;lane<lanes;++lane)output.lane(lane,2*lane_bits,source.lane(first+lane,lane_bits)<<lane_bits);
    floating_.v[rd]=output;return true;
  }
  const auto frint_vector=i&0xbfbffc00u;
  if(frint_vector==0x0e218800u||frint_vector==0x0ea18800u||frint_vector==0x0e219800u||frint_vector==0x0ea19800u||frint_vector==0x2e218800u||frint_vector==0x2ea19800u||frint_vector==0x2e219800u){ // FRINTN/P/M/Z/A/I/X, vector 2S/4S/2D.
    const unsigned lane_bits=(i&0x00400000u)?64u:32u,width=(i&0x40000000u)?128u:64u;
    if(lane_bits==64&&width==64)return false;
    const unsigned mode=frint_vector==0x0e218800u?softfloat_round_near_even:frint_vector==0x0ea18800u?softfloat_round_max:frint_vector==0x0e219800u?softfloat_round_min:frint_vector==0x0ea19800u?softfloat_round_minMag:frint_vector==0x2e218800u?softfloat_round_near_maxMag:fp.rounding();
    fp_access();const auto source=floating_.v[rn];Vector128 output;
    for(unsigned lane=0;lane<width/lane_bits;++lane)output.lane(lane,lane_bits,fp.round_integral(lane_bits,source.lane(lane,lane_bits),mode,frint_vector==0x2e219800u));
    floating_.v[rd]=output;return true;
  }
  const bool indexed_scalar_mul=(i&0xff80f400u)==0x5f809000u;
  if(indexed_scalar_mul||(i&0xbf80f400u)==0x0f809000u){ // FMUL S/D and 2S/4S/2D, by indexed S/D element.
    const bool scalar_form=indexed_scalar_mul,q=i&0x40000000u;const unsigned size=(i>>22)&3u;
    if((!scalar_form&&size==3&&!q)||(size==3&&(i&0x00200000u)))return false;
    const unsigned lane_bits=size==2?32u:64u,index=size==2?(((i>>21)&1u)|((i>>10)&2u)):((i>>11)&1u);
    fp_access();const auto source=floating_.v[rn];const auto element=floating_.v[rm].lane(index,lane_bits);Vector128 output;
    const unsigned lanes=scalar_form?1u:(q?128u:64u)/lane_bits;
    for(unsigned lane=0;lane<lanes;++lane)output.lane(lane,lane_bits,fp.binary(0,lane_bits,source.lane(lane,lane_bits),element));
    floating_.v[rd]=output;return true;
  }
  const auto fp_zero=i&0x8fbffc00u;
  if(fp_zero==0x0ea0c800u||fp_zero==0x0ea0d800u||fp_zero==0x0ea0e800u){ // FCMEQ/FCMGE/FCMGT/FCMLE/FCMLT #0, S/D and 2S/4S/2D.
    const bool scalar_form=i&0x10000000u,q=i&0x40000000u,u=i&0x20000000u;
    const unsigned lane_bits=(i&0x00400000u)?64u:32u;
    if((scalar_form&&!q)||(!scalar_form&&lane_bits==64&&!q)||(fp_zero==0x0ea0e800u&&u))return false;
    fp_access();const auto source=floating_.v[rn];Vector128 output;
    const unsigned lanes=scalar_form?1u:(q?128u:64u)/lane_bits;
    const bool equal=fp_zero==0x0ea0d800u&&!u;
    for(unsigned lane=0;lane<lanes;++lane){
      const auto flags=fp.compare(lane_bits,source.lane(lane,lane_bits),0,!equal);
      const bool match=fp_zero==0x0ea0c800u?(u?(flags==2||flags==6):flags==2):fp_zero==0x0ea0d800u?(u?(flags==8||flags==6):flags==6):flags==8;
      output.lane(lane,lane_bits,match?UINT64_MAX:0);
    }
    floating_.v[rd]=output;return true;
  }
  const bool fabd_scalar=(i&0xffa0fc00u)==0x7ea0d400u;
  const bool fabd_vector=(i&0xbfa0fc00u)==0x2ea0d400u;
  if(fabd_scalar||fabd_vector){ // FABD S/D, 2S/4S/2D: rounded subtraction, then absolute value.
    const unsigned lane_bits=(i&0x00400000u)?64u:32u,width=(i&0x40000000u)?128u:64u;
    if(fabd_vector&&lane_bits==64&&width==64)return false;
    fp_access();const auto left=floating_.v[rn],right=floating_.v[rm];Vector128 output;
    const unsigned lanes=fabd_scalar?1u:width/lane_bits;
    for(unsigned lane=0;lane<lanes;++lane)output.lane(lane,lane_bits,fp.binary(3,lane_bits,left.lane(lane,lane_bits),right.lane(lane,lane_bits))&~ArmFloat::sign(lane_bits));
    floating_.v[rd]=output;return true;
  }
  const auto fixed_gpr=i&0x7f3f0000u;
  if(fixed_gpr==0x1e180000u||fixed_gpr==0x1e190000u||fixed_gpr==0x1e020000u||fixed_gpr==0x1e030000u){ // Fixed-point FCVTZS/FCVTZU and SCVTF/UCVTF, GPR <-> S/D.
    const unsigned integer_bits=(i&0x80000000u)?64u:32u,scale=(i>>10)&63u;
    if(!valid_float()||(integer_bits==32&&scale<32))return false;
    fp_access();const unsigned fracbits=64-scale;const bool signed_value=!(i&0x10000u);
    if(fixed_gpr==0x1e180000u||fixed_gpr==0x1e190000u)set(rd,fp.to_fixed(bits,scalar(rn),signed_value,fracbits,integer_bits),integer_bits);
    else fp_set(rd,fp.from_fixed(bits,reg(rn,integer_bits),signed_value,integer_bits,fracbits),bits);
    return true;
  }
  if((i&0x7f20fc00)==0x1e200000){ // Integer/FP transfers and conversions
    const auto op=(i>>16)&7,mode=(i>>19)&3,integer_bits=(i>>31)?64u:32u;fp_access();
    if(op==6||op==7){
      if(mode==1&&type==2&&integer_bits==64){if(op==6)set(rd,floating_.v[rn].high);else floating_.v[rd].high=reg(rn);return true;}
      if(mode||!valid_float()||integer_bits!=bits)return false;
      if(op==6)set(rd,scalar(rn),integer_bits);else fp_set(rd,reg(rn,integer_bits),bits);return true;
    }
    if(!valid_float())return false;
    if(op==2||op==3){if(mode)return false;fp_set(rd,fp.from_int(bits,reg(rn,integer_bits),op==2,integer_bits),bits);return true;}
    if(op==0||op==1||op==4||op==5){
      if(op>=4&&mode)return false;const unsigned rounds[]{softfloat_round_near_even,softfloat_round_max,softfloat_round_min,softfloat_round_minMag};
      set(rd,fp.to_int(bits,scalar(rn),!(op&1),integer_bits,op>=4?unsigned(softfloat_round_near_maxMag):rounds[mode]),integer_bits);return true;
    }
    return false;
  }
  if((i&0xff200c00)==0x1e200800){ // FP two-source
    const auto operation=(i>>12)&15;if(!valid_float()||operation>8)return false;fp_access();fp_set(rd,fp.binary(operation,bits,scalar(rn),scalar(rm)),bits);return true;
  }
  if((i&0xff207c00)==0x1e204000){ // FP one-source, format conversion, rounding
    const auto operation=(i>>15)&63;fp_access();
    if(operation==4||operation==5||operation==7){const unsigned destination=operation==4?32:operation==5?64:16;
      if(!bits||bits==destination)return false;const auto value=fp.convert(bits,destination,scalar(rn));fp_set(rd,value,destination);return true;}
    if(!valid_float())return false;auto value=scalar(rn);
    if(operation==0)fp_set(rd,value,bits);
    else if(operation==1)fp_set(rd,value&~ArmFloat::sign(bits),bits);
    else if(operation==2)fp_set(rd,value^ArmFloat::sign(bits),bits);
    else if(operation==3)fp_set(rd,fp.sqrt(bits,value),bits);
    else if(operation>=8&&operation<=15&&operation!=13){
      const unsigned rounds[]{softfloat_round_near_even,softfloat_round_max,softfloat_round_min,softfloat_round_minMag,softfloat_round_near_maxMag};
      fp_set(rd,fp.round_integral(bits,value,operation<=12?rounds[operation-8]:fp.rounding(),operation==14),bits);
    }else return false;return true;
  }
  if((i&0xff20fc07)==0x1e202000){ // FCMP/FCMPE, register or zero
    if(!valid_float()||((i&8)&&rm))return false;fp_access();registers_.nzcv=fp.compare(bits,scalar(rn),i&8?0:scalar(rm),bool(i&16));return true;
  }
  if((i&0xff200c00)==0x1e200400){ // FCCMP/FCCMPE
    if(!valid_float())return false;fp_access();registers_.nzcv=condition((i>>12)&15)?fp.compare(bits,scalar(rn),scalar(rm),bool(i&16)):i&15;return true;
  }
  if((i&0xff200c00)==0x1e200c00){ // FCSEL preserves raw source bits, raises no FP arithmetic flags.
    if(!valid_float())return false;fp_access();fp_set(rd,scalar(condition((i>>12)&15)?rn:rm),bits);return true;
  }
  if((i&0xff000000)==0x1f000000){ // FMADD/FMSUB/FNMADD/FNMSUB
    if(!valid_float())return false;fp_access();const bool negative_addend=i&0x00200000,negative_product=negative_addend!=bool(i&0x8000);
    fp_set(rd,fp.fma(bits,scalar(rn),scalar(rm),scalar((i>>10)&31),negative_product,negative_addend),bits);return true;
  }
  const bool q=i&0x40000000,u=i&0x20000000;
  const unsigned width=q?128:64,size=(i>>22)&3,element=8u<<size;
  auto commit=[&](Vector128 value){if(!q)value.high=0;floating_.v[rd]=value;};
  const bool indexed_scalar=(i&0xff80b400u)==0x5f801000u;
  if(indexed_scalar||(i&0xbf80b400u)==0x0f801000u){ // FMLA/FMLS, single/double by indexed element.
    const unsigned lane_bits=(i&0x00400000u)?64u:32u;
    // L is reserved for D elements; the vector 1D form is unallocated.
    if(lane_bits==64&&((i&0x00200000u)||(!indexed_scalar&&!q)))return false;
    const unsigned index=lane_bits==64?((i>>11)&1u):(((i>>11)&1u)<<1)|((i>>21)&1u);
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm],old=floating_.v[rd];
    const auto selected=b.lane(index,lane_bits);Vector128 value;
    // Copy every operand first, including the indexed source when Rd == Rm.
    for(unsigned n=0;n<(indexed_scalar?1u:width/lane_bits);++n)
      value.lane(n,lane_bits,fp.fma(lane_bits,a.lane(n,lane_bits),selected,
          old.lane(n,lane_bits),bool(i&0x00004000u),false));
    floating_.v[rd]=value;return true; // Scalar/64-bit forms clear the unwritten upper bits.
  }
  const auto right_shift=[&](std::uint64_t x,unsigned amount,unsigned lane_bits,bool signed_value){
    const bool negative=signed_value&&bool(x&(UINT64_C(1)<<(lane_bits-1)));
    if(amount>=lane_bits)return negative?low_mask(lane_bits):UINT64_C(0);
    if(!amount)return x;
    return (x>>amount)|(negative?(low_mask(lane_bits)^low_mask(lane_bits-amount)):UINT64_C(0));
  };
  // immh=0000 selects modified immediates, not a reserved shift. Let the
  // MOVI/MVNI/ORR/BIC decoder below handle those shared opcode patterns.
  const bool shift_immediate=(i&0x00780000u)!=0;
  const auto immediate_shift=i&0x9f80fc00u;
  if(shift_immediate&&(immediate_shift==0x0f000400u||immediate_shift==0x0f005400u||(immediate_shift==0x0f004400u&&u))){ // S/USHR, SHL, SLI or SRI.
    const unsigned encoded=(i>>16)&127;if(encoded<8)return false;
    unsigned lane_bits=8;while(lane_bits*2<=encoded)lane_bits*=2;
    if(lane_bits==64&&!q)return false;
    const bool left=immediate_shift==0x0f005400u;
    const bool insert=(left&&u)||immediate_shift==0x0f004400u;
    const unsigned amount=left?encoded-lane_bits:2*lane_bits-encoded;
    fp_access();const auto source=floating_.v[rn],old=floating_.v[rd];Vector128 value;
    for(unsigned n=0;n<width/lane_bits;n++){
      const auto x=source.lane(n,lane_bits);
      auto result=left?x<<amount:right_shift(x,amount,lane_bits,!u);
      if(insert){const auto preserve=left?low_mask(amount):low_mask(lane_bits)^low_mask(lane_bits-amount);result|=old.lane(n,lane_bits)&preserve;}
      value.lane(n,lane_bits,result);
    }commit(value);return true;
  }
  if(shift_immediate&&immediate_shift==0x0f001400u){ // SSRA/USRA: arithmetic or logical shift right, accumulate modulo each lane.
    const unsigned encoded=(i>>16)&127;if(encoded<8)return false;
    unsigned lane_bits=8;while(lane_bits*2<=encoded)lane_bits*=2;
    if(lane_bits==64&&!q)return false; // Unallocated 1D form.
    const unsigned amount=2*lane_bits-encoded; // 1..element width; never shift C++ by 64.
    fp_access();const auto source=floating_.v[rn],old=floating_.v[rd];Vector128 value;
    for(unsigned n=0;n<width/lane_bits;n++){
      const auto shifted=right_shift(source.lane(n,lane_bits),amount,lane_bits,!u);
      value.lane(n,lane_bits,old.lane(n,lane_bits)+shifted);
    }
    commit(value);return true; // Correct when Rd aliases Rn; unrelated state unchanged.
  }
  if(shift_immediate&&immediate_shift==0x0f00a400u){ // SSHLL/USHLL and upper-half variants (SXTL/UXTL at shift zero).
    const unsigned encoded=(i>>16)&127;if(encoded>=64)return false; // immh[3] is reserved.
    unsigned narrow=8;while(narrow*2<=encoded)narrow*=2;
    const unsigned amount=encoded-narrow,lanes=64/narrow,first=q?lanes:0;
    fp_access();const auto source=floating_.v[rn];Vector128 value;
    for(unsigned n=0;n<lanes;n++){
      auto x=source.lane(first+n,narrow);
      if(!u&&(x&(UINT64_C(1)<<(narrow-1))))x|=~low_mask(narrow);
      value.lane(n,2*narrow,x<<amount); // Unsigned arithmetic also handles negative signed inputs without UB.
    }
    // Q selects the source half; the widened destination is always 128 bits.
    floating_.v[rd]=value;return true;
  }
  if((i&0x9f20fc00u)==0x0e204400u){ // SSHL/USHL: the signed low byte determines each lane's shift.
    if(size==3&&!q)return false;fp_access();const auto source=floating_.v[rn],shifts=floating_.v[rm];Vector128 value;
    for(unsigned n=0;n<width/element;n++){
      const auto x=source.lane(n,element);const unsigned byte=unsigned(shifts.lane(n,element)&255);
      const int amount=byte<128?int(byte):int(byte)-256;
      value.lane(n,element,amount<0?right_shift(x,unsigned(-amount),element,!u):unsigned(amount)>=element?0:x<<unsigned(amount));
    }commit(value);return true;
  }
  const bool saturating_narrow_vector=(i&0x9f3ffc00u)==0x0e214800u||(i&0xbf3ffc00u)==0x2e212800u;
  const bool saturating_narrow_scalar=(i&0xdf3ffc00u)==0x5e214800u||(i&0xff3ffc00u)==0x7e212800u;
  if(saturating_narrow_vector||saturating_narrow_scalar){ // SQXTN/UQXTN/SQXTUN, scalar and lower/upper vector halves.
    if(size==3)return false;fp_access();const auto source=floating_.v[rn];Vector128 value;bool saturated=false;
    const bool signed_source=!u||(i&0x6000u)==0x2000u;
    for(unsigned n=0;n<(saturating_narrow_scalar?1u:64/element);n++){
      const auto x=source.lane(n,2*element);std::uint64_t result;
      const bool negative=signed_source&&bool(x&(UINT64_C(1)<<(2*element-1)));
      if(negative&&u){result=0;saturated=true;}
      else if(negative){
        const auto magnitude=(UINT64_C(0)-x)&low_mask(2*element),limit=UINT64_C(1)<<(element-1);
        if(magnitude>limit){result=UINT64_C(0)-limit;saturated=true;}else result=x;
      }else{
        const auto limit=low_mask(u?element:element-1);
        if(x>limit){result=limit;saturated=true;}else result=x;
      }
      value.lane(n,element,result);
    }
    if(saturated)floating_.fpsr|=UINT32_C(0x08000000);
    if(saturating_narrow_scalar)floating_.v[rd]=value;
    else if(q)floating_.v[rd].high=value.low;else floating_.v[rd]={value.low,0};return true;
  }
  if((i&0xbf3ffc00u)==0x0e212800u){ // XTN/XTN2: truncate each lane to half its width, without saturation.
    if(size==3)return false; // size=11 is unallocated; reject before touching any state.
    fp_access();const auto source=floating_.v[rn];Vector128 value;
    for(unsigned n=0;n<64/element;n++)value.lane(n,element,source.lane(n,2*element));
    // Capture all source lanes first, including when Rd==Rn. Q selects the
    // destination half, not the source half; XTN2 preserves the old low 64 bits.
    if(q)floating_.v[rd].high=value.low;else floating_.v[rd]={value.low,0};return true;
  }
  const auto pairwise_long=i&0x9f3ffc00u;
  if(pairwise_long==0x0e202800u||pairwise_long==0x0e206800u){ // S/UADDLP and S/UADALP, widening pairwise addition, optionally accumulated.
    if(size==3)return false;fp_access();const auto source=floating_.v[rn],accumulator=floating_.v[rd];Vector128 value;
    for(unsigned n=0;n<width/(2*element);n++){
      auto x=source.lane(2*n,element),y=source.lane(2*n+1,element);
      if(!u){const auto sign=UINT64_C(1)<<(element-1);if(x&sign)x|=~low_mask(element);if(y&sign)y|=~low_mask(element);}
      value.lane(n,element*2,x+y+(pairwise_long==0x0e206800u?accumulator.lane(n,element*2):0));
    }commit(value);return true;
  }
  if(shift_immediate&&(i&0xbf80f400u)==0x0f008400u){ // SHRN/RSHRN and upper-half variants.
    const unsigned encoded=(i>>16)&127;if(encoded<8||encoded>=64)return false;
    unsigned narrow=8;while(narrow*2<=encoded)narrow*=2;
    const unsigned shift=2*narrow-encoded;const bool rounded=i&0x800;
    fp_access();const auto source=floating_.v[rn];Vector128 result;
    for(unsigned n=0;n<64/narrow;n++){
      const auto x=source.lane(n,2*narrow);
      const auto carry=rounded&&((x&((UINT64_C(1)<<shift)-1))>=(UINT64_C(1)<<(shift-1)));
      result.lane(n,narrow,(x>>shift)+unsigned(carry));
    }
    if(q)floating_.v[rd].high=result.low;else floating_.v[rd]={result.low,0};return true;
  }
  if((i&0xbf3ffc00u)==0x2e20b800u){ // NEG Vd.<T>, Vn.<T>: modular vector integer negate.
    if(element==64&&!q)return false; // 1D is unallocated; reject before FP access.
    fp_access();const auto source=floating_.v[rn];Vector128 value;
    for(unsigned n=0;n<width/element;n++)value.lane(n,element,UINT64_C(0)-source.lane(n,element));
    commit(value);return true; // 64-bit vectors zero their high half; Rd==Rn supported.
  }
  const auto compare_zero=i&0xbf3ffc00u;
  if(compare_zero==0x0e208800u||compare_zero==0x2e208800u||compare_zero==0x0e209800u||compare_zero==0x2e209800u||compare_zero==0x0e20a800u){ // CMGT/CMGE/CMEQ/CMLE/CMLT, signed comparisons with zero.
    if(size==3&&!q)return false;fp_access();const auto source=floating_.v[rn];Vector128 value;
    for(unsigned n=0;n<width/element;n++){
      const auto x=source.lane(n,element);const bool negative=x&(UINT64_C(1)<<(element-1));
      const bool yes=compare_zero==0x0e208800u?!negative&&x:compare_zero==0x2e208800u?!negative:compare_zero==0x0e209800u?!x:compare_zero==0x2e209800u?negative||!x:negative;
      value.lane(n,element,yes?UINT64_MAX:0);
    }commit(value);return true;
  }
  if((i&0x9f3ffc00u)==0x0e303800u){ // SADDLV/UADDLV: widen and add all lanes into one scalar.
    if(size==3||(size==2&&!q))return false;
    fp_access();const auto source=floating_.v[rn];std::uint64_t sum=0;
    for(unsigned n=0;n<width/element;n++){
      auto x=source.lane(n,element);
      if(!u&&(x&(UINT64_C(1)<<(element-1))))x|=~low_mask(element);
      sum+=x;
    }
    fp_set(rd,sum,2*element);return true;
  }
  const auto minmax_across=i&0x9f3ffc00u;
  if(minmax_across==0x0e30a800u||minmax_across==0x0e31a800u){ // S/UMAXV and S/UMINV across lanes.
    if(size==3||(size==2&&!q))return false; // Neither 64-bit lanes nor the 2S arrangement is allocated.
    fp_access();const auto source=floating_.v[rn];
    const bool minimum=minmax_across==0x0e31a800u;
    const auto sign=u?UINT64_C(0):(UINT64_C(1)<<(element-1));
    auto best=source.lane(0,element);
    for(unsigned n=1;n<width/element;n++){
      const auto x=source.lane(n,element);
      // Flipping the sign bit gives the signed ordering without implementation-defined casts.
      if(minimum?((x^sign)<(best^sign)):((x^sign)>(best^sign)))best=x;
    }
    fp_set(rd,best,element);return true; // Scalar result clears all remaining vector bits, including Rd==Rn.
  }
  const auto pairwise=i&0x9f20fc00u;
  if(pairwise==0x0e20bc00u||pairwise==0x0e20a400u||pairwise==0x0e20ac00u){ // ADDP, S/UMAXP, S/UMINP.
    const bool add=pairwise==0x0e20bc00u,minimum=pairwise==0x0e20ac00u;
    if((add&&u)||(size==3&&(!q||!add)))return false;
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm];Vector128 value;const unsigned lanes=width/element;
    for(unsigned n=0;n<lanes;n++){
      const auto& source=n<lanes/2?a:b;const auto at=(n%(lanes/2))*2;
      const auto x=source.lane(at,element),y=source.lane(at+1,element);
      const auto sign=UINT64_C(1)<<(element-1),xx=u?x:x^sign,yy=u?y:y^sign;
      value.lane(n,element,add?x+y:minimum?(xx<yy?x:y):(xx>yy?x:y));
    }commit(value);return true;
  }
  if((i&0x9f20fc00)==0x0e201c00){ // AND/BIC/ORR/ORN/EOR/BSL/BIT/BIF
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm],d=floating_.v[rd];Vector128 value;
    auto logic=[&](std::uint64_t x,std::uint64_t y,std::uint64_t old){
      if(!u){switch(size){case 0:return x&y;case 1:return x&~y;case 2:return x|y;default:return x|~y;}}
      switch(size){case 0:return x^y;case 1:return (old&x)|(~old&y);case 2:return (old&~y)|(x&y);default:return (old&y)|(x&~y);}
    };value.low=logic(a.low,b.low,d.low);value.high=logic(a.high,b.high,d.high);commit(value);return true;
  }
  if((i&0xbf20fc00u)==0x0e20e000u){ // PMULL/PMULL2: carryless polynomial multiplication, P8 or P64.
    if(size!=0&&size!=3)return false;
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm];Vector128 value;
    if(size==0){
      for(unsigned n=0;n<8;n++){
        const auto x=a.lane(n+(q?8:0),8),y=b.lane(n+(q?8:0),8);std::uint64_t product=0;
        for(unsigned bit=0;bit<8;bit++)if(y&(UINT64_C(1)<<bit))product^=x<<bit;
        value.lane(n,16,product);
      }
    }else{
      const auto x=q?a.high:a.low,y=q?b.high:b.low;
      for(unsigned bit=0;bit<64;bit++)if(y&(UINT64_C(1)<<bit)){value.low^=x<<bit;if(bit)value.high^=x>>(64-bit);}
    }
    floating_.v[rd]=value;return true;
  }
  if((i&0x9f20cc00u)==0x0e200000u){ // S/UADDL, S/UADDW, S/USUBL, S/USUBW, including the upper-half forms.
    if(size==3)return false; // No 64-to-128-bit element form is allocated.
    const unsigned operation=(i>>12)&3,lanes=64/element,first=q?lanes:0;
    const bool wide=operation&1,subtract=operation&2;
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm];Vector128 value;
    for(unsigned n=0;n<lanes;n++){
      auto x=a.lane(wide?n:first+n,wide?2*element:element),y=b.lane(first+n,element);
      if(!u){
        const auto sign=UINT64_C(1)<<(element-1);
        if(!wide&&(x&sign))x|=~low_mask(element);
        if(y&sign)y|=~low_mask(element);
      }
      // Unsigned arithmetic expresses modular lane results without signed overflow.
      value.lane(n,2*element,subtract?x-y:x+y);
    }
    // Q chooses only narrow source halves. The destination and a wide Rn are 128 bits.
    floating_.v[rd]=value;return true;
  }
  const auto multiply_long=i&0x9f20fc00u;
  if(multiply_long==0x0e20c000u||multiply_long==0x0e208000u||multiply_long==0x0e20a000u){ // S/UMULL, S/UMLAL, S/UMLSL, lower or upper source half.
    if(size==3)return false;
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm],old=floating_.v[rd];Vector128 value;
    const unsigned lanes=64/element,first=q?lanes:0;
    for(unsigned n=0;n<lanes;n++){
      auto x=a.lane(first+n,element),y=b.lane(first+n,element);
      if(!u){const auto sign=UINT64_C(1)<<(element-1);if(x&sign)x|=~low_mask(element);if(y&sign)y|=~low_mask(element);}
      const auto product=x*y,accumulator=old.lane(n,2*element);
      value.lane(n,2*element,multiply_long==0x0e20c000u?product:multiply_long==0x0e208000u?accumulator+product:accumulator-product);
    }
    // Q selects the narrow source half. Every destination is a full 128-bit vector.
    floating_.v[rd]=value;return true;
  }
  const auto same=i&0x9f20fc00;
  if(same==0x0e208400||same==0x0e209c00||same==0x0e209400||same==0x0e208c00||same==0x0e203400||same==0x0e203c00||same==0x0e206400||same==0x0e206c00){
    if((element==64&&!q)||(same==0x0e209c00&&(size==3||(u&&size!=0)))||(same==0x0e209400&&size==3)||((same==0x0e206400||same==0x0e206c00)&&size==3))return false;
    fp_access();const auto a=floating_.v[rn],b=floating_.v[rm],old=floating_.v[rd];Vector128 value;
    for(unsigned lane=0;lane<width/element;++lane){const auto x=a.lane(lane,element),y=b.lane(lane,element);std::uint64_t out=0;
      if(same==0x0e208400)out=u?x-y:x+y;
      else if(same==0x0e209c00){if(u){for(unsigned bit=0;bit<8;bit++)if(y&(UINT64_C(1)<<bit))out^=x<<bit;}else out=x*y;}
      else if(same==0x0e209400)out=u?old.lane(lane,element)-x*y:old.lane(lane,element)+x*y;
      else if(same==0x0e206400||same==0x0e206c00){const auto sign=UINT64_C(1)<<(element-1),xx=u?x:x^sign,yy=u?y:y^sign;out=same==0x0e206400?(xx>yy?x:y):(xx<yy?x:y);}
      else if(same==0x0e208c00)out=(u?x==y:bool(x&y))?UINT64_MAX:0; // CMEQ or CMTST, per integer lane.
      else{const auto sign=UINT64_C(1)<<(element-1);const auto ordered_x=u?x:x^sign,ordered_y=u?y:y^sign;
        out=(same==0x0e203400?ordered_x>ordered_y:ordered_x>=ordered_y)?UINT64_MAX:0;}
      value.lane(lane,element,out);
    }commit(value);return true;
  }
  if((i&0x9fe08400)==0x0e000400){ // DUP/INS/UMOV/SMOV
    const auto imm5=(i>>16)&31,imm4=(i>>11)&15;if(!imm5)return false;
    unsigned shift=0;while(!(imm5&(1u<<shift)))++shift;const auto lane_bits=8u<<shift,index=imm5>>(shift+1);
    if(shift>3)return false;fp_access();const auto source=floating_.v[rn];auto value=floating_.v[rd];
    if(u){ // INS from a vector lane; source index has the same element size.
      if(!q||((i>>10)&1)==0||(imm4&((1u<<shift)-1)))return false;
      value.lane(index,lane_bits,source.lane(imm4>>shift,lane_bits));floating_.v[rd]=value;return true;
    }
    if(imm4==0||imm4==1){ // DUP from vector lane or general register.
      if(lane_bits==64&&!q)return false;if(imm4==1&&imm5!=(1u<<shift))return false;
      const auto repeated=imm4==1?reg(rn,lane_bits==64?64:32):source.lane(index,lane_bits);value={};
      for(unsigned n=0;n<width/lane_bits;++n)value.lane(n,lane_bits,repeated);commit(value);return true;
    }
    if(imm4==3){if(!q)return false;value.lane(index,lane_bits,reg(rn,lane_bits==64?64:32));floating_.v[rd]=value;return true;}
    if(imm4==5||imm4==7){ // Signed/unsigned extraction into W/X.
      if((imm4==7&&(lane_bits==64)!=q)||(imm4==5&&(lane_bits==64||(!q&&lane_bits==32))))return false;
      auto extracted=source.lane(index,lane_bits);
      if(imm4==5&&extracted&(UINT64_C(1)<<(lane_bits-1)))extracted|=~low_mask(lane_bits);
      set(rd,extracted,q?64:32);return true;
    }return false;
  }
  if((i&0x9ff80400)==0x0f000400){ // MOVI/MVNI/ORR/BIC vector immediate; FMOV vector immediate.
    const auto cmode=(i>>12)&15,imm=(((i>>16)&7)<<5)|((i>>5)&31);std::uint64_t expanded=0;
    if(cmode<8){const auto word=std::uint64_t(imm)<<(8*(cmode>>1));expanded=word|(word<<32);}
    else if(cmode<12){const auto half=std::uint64_t(imm)<<(8*((cmode>>1)&1));expanded=half|(half<<16)|(half<<32)|(half<<48);}
    else if(cmode==12||cmode==13){const unsigned shift=8*(cmode-11);const auto word=(std::uint64_t(imm)<<shift)|((UINT64_C(1)<<shift)-1);expanded=word|(word<<32);}
    else if(cmode==14){if(u){for(unsigned n=0;n<8;++n)if(imm&(1u<<n))expanded|=UINT64_C(255)<<(8*n);}
      else expanded=std::uint64_t(imm)*UINT64_C(0x0101010101010101);}
    else{if(u&&!q)return false;const unsigned e=u?11:8,f=u?52:23,repeat=(imm>>6)&1;
      const auto exp=(std::uint64_t(!repeat)<<(e-1))|(repeat?((UINT64_C(1)<<(e-3))-1)<<2:0)|((imm>>4)&3);
      expanded=(std::uint64_t(imm>>7)<<(u?63:31))|(exp<<f)|(std::uint64_t(imm&15)<<(f-4));if(!u)expanded|=expanded<<32;}
    fp_access();auto value=floating_.v[rd];
    if(cmode<12&&(cmode&1)){if(u){value.low&=~expanded;value.high&=~expanded;}else{value.low|=expanded;value.high|=expanded;}}
    else{if(u&&cmode<14)expanded=~expanded;value={expanded,expanded};}
    commit(value);return true;
  }
  if((i&0xbfe08400)==0x2e000000){ // EXT concatenates at byte offset; sources may alias the destination.
    const auto offset=(i>>11)&15,bytes=width/8;if(offset>=bytes)return false;fp_access();
    const auto a=floating_.v[rn],b=floating_.v[rm];Vector128 value;
    for(unsigned n=0;n<bytes;++n)value.lane(n,8,n+offset<bytes?a.lane(n+offset,8):b.lane(n+offset-bytes,8));commit(value);return true;
  }
  if((i&0xbf208c00)==0x0e000800){ // UZP/TRN/ZIP, either half.
    const auto operation=(i>>12)&7;if((operation&3)==0||(element==64&&!q))return false;fp_access();
    const auto a=floating_.v[rn],b=floating_.v[rm];const unsigned lanes=width/element,part=operation>>2;Vector128 value;
    for(unsigned n=0;n<lanes;++n){std::uint64_t out=0;
      if((operation&3)==1)out=n<lanes/2?a.lane(2*n+part,element):b.lane(2*(n-lanes/2)+part,element);
      else if((operation&3)==2)out=(n&1?b:a).lane((n/2)*2+part,element);
      else out=(n&1?b:a).lane(n/2+part*(lanes/2),element);
      value.lane(n,element,out);
    }commit(value);return true;
  }
  if((i&0xbfe08c00)==0x0e000000){ // TBL/TBX, one through four 128-bit table registers.
    const auto length=((i>>13)&3)+1;const bool extension=i&0x1000;fp_access();
    const auto indices=floating_.v[rm],old=floating_.v[rd];Vector128 value;
    for(unsigned n=0;n<width/8;++n){const auto index=unsigned(indices.lane(n,8));
      value.lane(n,8,index<16*length?floating_.v[(rn+index/16)&31].lane(index%16,8):extension?old.lane(n,8):0);}
    commit(value);return true;
  }
  if((i&0x9ffffc00)==0x0e205800){ // CNT or NOT, byte vectors.
    fp_access();const auto a=floating_.v[rn];Vector128 value;
    if(u)value={~a.low,~a.high};
    else for(unsigned n=0;n<width/8;n++){
      auto byte=a.lane(n,8);unsigned count=0;for(unsigned bit=0;bit<8;bit++)count+=unsigned((byte>>bit)&1);
      value.lane(n,8,count);
    }
    commit(value);return true;
  }
  const auto reverse=i&0xbf3ffc00u;
  if(reverse==0x0e200800u||reverse==0x2e200800u||reverse==0x0e201800u){ // REV64, REV32, REV16.
    const unsigned block_bits=reverse==0x0e200800u?64:reverse==0x2e200800u?32:16;
    if(element>=block_bits)return false;fp_access();const auto a=floating_.v[rn];Vector128 value;const auto per_word=block_bits/element;
    for(unsigned n=0;n<width/element;++n)value.lane(n,element,a.lane((n/per_word)*per_word+per_word-1-n%per_word,element));commit(value);return true;
  }
  if((i&0xbf3ffc00)==0x0e31b800){ // ADDV
    if(size==3||(size==2&&!q))return false;fp_access();const auto a=floating_.v[rn];std::uint64_t total=0;
    for(unsigned n=0;n<width/element;++n)total+=a.lane(n,element);fp_set(rd,total,element);return true;
  }
  const auto vector_fp=i&0xbf20fc00;const unsigned fp_bits=(i&0x00400000)?64:32;
  if(vector_fp==0x0e20d400||vector_fp==0x2e20dc00||vector_fp==0x2e20fc00||vector_fp==0x0e20cc00||
     vector_fp==0x0e20f400||vector_fp==0x0e20c400||vector_fp==0x0e20e400||vector_fp==0x2e20e400||vector_fp==0x2e20ec00){
    if(fp_bits==64&&!q)return false;fp_access();const auto a=floating_.v[rn],b=floating_.v[rm],old=floating_.v[rd];Vector128 value;
    const bool variant=i&0x00800000;
    for(unsigned n=0;n<width/fp_bits;++n){const auto x=a.lane(n,fp_bits),y=b.lane(n,fp_bits);std::uint64_t out;
      if(vector_fp==0x0e20d400)out=fp.binary(variant?3:2,fp_bits,x,y);
      else if(vector_fp==0x2e20dc00){if(variant)return false;out=fp.binary(0,fp_bits,x,y);}
      else if(vector_fp==0x2e20fc00){if(variant)return false;out=fp.binary(1,fp_bits,x,y);}
      else if(vector_fp==0x0e20cc00)out=fp.fma(fp_bits,x,y,old.lane(n,fp_bits),variant,false);
      else if(vector_fp==0x0e20f400||vector_fp==0x0e20c400)out=fp.binary((vector_fp==0x0e20c400?6u:4u)+unsigned(variant),fp_bits,x,y);
      else{ // FCMEQ/FCMGE/FCMGT and absolute comparisons. Unordered always yields false.
        const bool absolute=vector_fp==0x2e20ec00;const auto xx=absolute?x&~ArmFloat::sign(fp_bits):x,yy=absolute?y&~ArmFloat::sign(fp_bits):y;
        const auto flags=fp.compare(fp_bits,xx,yy,vector_fp!=0x0e20e400);
        bool yes=vector_fp==0x0e20e400?flags==6:variant?flags==2:flags==2||flags==6;out=yes?UINT64_MAX:0;
      }value.lane(n,fp_bits,out);
    }commit(value);return true;
  }
  const auto vector_unary=i&0xbfbffc00;
  if(vector_unary==0x2ea1f800||vector_unary==0x0ea0f800||vector_unary==0x2ea0f800){ // FSQRT/FABS/FNEG
    if(fp_bits==64&&!q)return false;fp_access();const auto a=floating_.v[rn];Vector128 value;
    for(unsigned n=0;n<width/fp_bits;++n){const auto x=a.lane(n,fp_bits);
      value.lane(n,fp_bits,vector_unary==0x2ea1f800?fp.sqrt(fp_bits,x):vector_unary==0x0ea0f800?x&~ArmFloat::sign(fp_bits):x^ArmFloat::sign(fp_bits));}
    commit(value);return true;
  }
  return false;
}
