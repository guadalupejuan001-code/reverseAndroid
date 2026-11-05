#pragma once
// Finite architectural FP/SIMD state and ARMv8 IEEE arithmetic. SoftFloat is a
// pinned arithmetic library, not a CPU emulator or another quantum authority.
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#define THREAD_LOCAL thread_local
extern "C" {
#include "../third_party/softfloat/source/include/softfloat.h"
}
#undef THREAD_LOCAL

namespace reverse_pc::guest {
struct Vector128 {
  std::uint64_t low=0,high=0;
  std::uint64_t lane(unsigned index,unsigned bits) const {
    const auto word=index*bits<64?low:high;const auto shift=(index*bits)%64;
    return bits==64?word:(word>>shift)&((UINT64_C(1)<<bits)-1);
  }
  void lane(unsigned index,unsigned bits,std::uint64_t value){
    auto& word=index*bits<64?low:high;const auto shift=(index*bits)%64;
    const auto mask=bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1;
    word=(word&~(mask<<shift))|((value&mask)<<shift);
  }
};
struct FloatingState {
  std::array<Vector128,32> v{};
  std::uint32_t fpcr=0,fpsr=0;
  unsigned nonzero() const {unsigned count=0;for(const auto& value:v)if(value.low||value.high)++count;return count;}
  std::vector<std::uint8_t> save() const {
    std::uint32_t mask=0;for(unsigned i=0;i<32;++i)if(v[i].low||v[i].high)mask|=1u<<i;
    if(!mask&&!fpcr&&!fpsr)return {}; // Unchanged register background is zero.
    std::vector<std::uint8_t> bytes{'R','F','P','1',13,10,26,10};
    auto put=[&](std::uint64_t value,unsigned size){for(unsigned i=0;i<size;++i)bytes.push_back(std::uint8_t(value>>(8*i)));};
    put(fpcr,4);put(fpsr,4);put(mask,4);
    for(unsigned i=0;i<32;++i)if(mask&(1u<<i)){put(v[i].low,8);put(v[i].high,8);}return bytes;
  }
  static FloatingState restore(const std::vector<std::uint8_t>& bytes){
    FloatingState result;if(bytes.empty())return result;
    const std::vector<std::uint8_t> magic{'R','F','P','1',13,10,26,10};
    if(bytes.size()<20||bytes.size()>532||!std::equal(magic.begin(),magic.end(),bytes.begin()))throw std::runtime_error("Invalid FP/SIMD state");
    std::size_t at=8;auto take=[&](unsigned size){if(size>bytes.size()-at)throw std::runtime_error("Truncated FP/SIMD state");std::uint64_t value=0;for(unsigned i=0;i<size;++i)value|=std::uint64_t(bytes[at++])<<(8*i);return value;};
    result.fpcr=std::uint32_t(take(4));result.fpsr=std::uint32_t(take(4));const auto mask=take(4);
    if((result.fpcr&~0x03c00000u)||(result.fpsr&~0x0800009fu))throw std::runtime_error("Unsupported FPCR/FPSR state");
    for(unsigned i=0;i<32;++i)if(mask&(1u<<i)){result.v[i].low=take(8);result.v[i].high=take(8);}
    if(at!=bytes.size()||result.save()!=bytes)throw std::runtime_error("Noncanonical FP/SIMD state");return result;
  }
};

class ArmFloat {
 public:
  explicit ArmFloat(FloatingState& state):state_(state){}
  static std::uint64_t sign(unsigned bits){return UINT64_C(1)<<(bits-1);}
  static std::uint64_t fraction(unsigned bits){return bits==32?0x7fffff:UINT64_C(0xfffffffffffff);}
  static std::uint64_t exponent(unsigned bits){return bits==32?0x7f800000:UINT64_C(0x7ff0000000000000);}
  static std::uint64_t quiet(unsigned bits){return bits==32?0x400000:UINT64_C(0x8000000000000);}
  static std::uint64_t default_nan(unsigned bits){return exponent(bits)|quiet(bits);}
  static bool nan(std::uint64_t value,unsigned bits){return (value&exponent(bits))==exponent(bits)&&(value&fraction(bits));}
  static bool signaling(std::uint64_t value,unsigned bits){return nan(value,bits)&&!(value&quiet(bits));}
  static std::int64_t signed_value(std::uint64_t value){return value>INT64_MAX?-1-std::int64_t(UINT64_MAX-value):std::int64_t(value);}
  unsigned rounding() const {const unsigned modes[]{softfloat_round_near_even,softfloat_round_max,softfloat_round_min,softfloat_round_minMag};return modes[(state_.fpcr>>22)&3];}
  void begin(){softfloat_roundingMode=std::uint_fast8_t(rounding());softfloat_detectTininess=softfloat_tininess_beforeRounding;softfloat_exceptionFlags=0;}
  std::uint64_t input(std::uint64_t value,unsigned bits){
    if((state_.fpcr&(1u<<24))&&!(value&exponent(bits))&&(value&fraction(bits))){state_.fpsr|=128;return value&sign(bits);}return value;
  }
  void flags(unsigned flags){state_.fpsr|=(flags&softfloat_flag_invalid?1u:0u)|(flags&softfloat_flag_infinite?2u:0u)|
    (flags&softfloat_flag_overflow?4u:0u)|(flags&softfloat_flag_underflow?8u:0u)|(flags&softfloat_flag_inexact?16u:0u);}
  std::uint64_t finish(std::uint64_t value,unsigned bits){
    auto status=unsigned(softfloat_exceptionFlags);
    if((state_.fpcr&(1u<<24))&&((!(value&exponent(bits))&&(value&fraction(bits)))||(status&softfloat_flag_underflow))){
      value&=sign(bits);status&=~unsigned(softfloat_flag_underflow|softfloat_flag_inexact);state_.fpsr|=8;
    }
    flags(status);if((state_.fpcr&(1u<<25))&&nan(value,bits))value=default_nan(bits);return value;
  }
  static bool less(std::uint64_t a,std::uint64_t b,unsigned bits){
    if(!(a&~sign(bits))&&!(b&~sign(bits)))return false;
    if(bool(a&sign(bits))!=bool(b&sign(bits)))return bool(a&sign(bits));return a&sign(bits)?a>b:a<b;
  }
  std::uint64_t propagate(std::uint64_t a,std::uint64_t b,unsigned bits){
    if(signaling(a,bits)||signaling(b,bits))softfloat_exceptionFlags|=softfloat_flag_invalid;
    const auto value=signaling(a,bits)?a:signaling(b,bits)?b:nan(a,bits)?a:b;return value|quiet(bits);
  }
  // ARM two-source opcode: MUL, DIV, ADD, SUB, MAX, MIN, MAXNM, MINNM, NMUL.
  std::uint64_t binary(unsigned operation,unsigned bits,std::uint64_t a,std::uint64_t b){
    begin();a=input(a,bits);b=input(b,bits);std::uint64_t value=0;
    if(operation>=4&&operation<=7){
      if(nan(a,bits)||nan(b,bits)){
        if(operation>=6&&!signaling(a,bits)&&!signaling(b,bits)&&nan(a,bits)!=nan(b,bits))value=nan(a,bits)?b:a;
        else value=propagate(a,b,bits);
      }else if(!(a&~sign(bits))&&!(b&~sign(bits)))value=operation&1?(a|b)&sign(bits):(a&b)&sign(bits);
      else value=less(a,b,bits)==bool(operation&1)?a:b;
    }else if(bits==32){
      const float32_t x{std::uint32_t(a)},y{std::uint32_t(b)};
      switch(operation){case 0:case 8:value=f32_mul(x,y).v;break;case 1:value=f32_div(x,y).v;break;case 2:value=f32_add(x,y).v;break;case 3:value=f32_sub(x,y).v;break;default:throw std::runtime_error("Unsupported FP binary operation");}
    }else{
      const float64_t x{a},y{b};
      switch(operation){case 0:case 8:value=f64_mul(x,y).v;break;case 1:value=f64_div(x,y).v;break;case 2:value=f64_add(x,y).v;break;case 3:value=f64_sub(x,y).v;break;default:throw std::runtime_error("Unsupported FP binary operation");}
    }
    if(operation==8)value^=sign(bits);return finish(value,bits);
  }
  std::uint64_t sqrt(unsigned bits,std::uint64_t value){begin();value=input(value,bits);return finish(bits==32?std::uint64_t(f32_sqrt(float32_t{std::uint32_t(value)}).v):f64_sqrt(float64_t{value}).v,bits);}
  std::uint64_t fma(unsigned bits,std::uint64_t a,std::uint64_t b,std::uint64_t c,bool negate_product,bool negate_addend){
    begin();a=input(a,bits);b=input(b,bits);c=input(c,bits);if(negate_product)a^=sign(bits);if(negate_addend)c^=sign(bits);
    if(nan(a,bits)||nan(b,bits)||nan(c,bits)){
      // FPMulAdd processes the addend first: SNaN before QNaN, then C/A/B.
      const auto magnitude=~sign(bits);const bool invalid_product=((a&magnitude)==exponent(bits)&&!(b&magnitude))||((b&magnitude)==exponent(bits)&&!(a&magnitude));
      std::uint64_t value=0;
      if(signaling(c,bits)||signaling(a,bits)||signaling(b,bits)){
        softfloat_exceptionFlags|=softfloat_flag_invalid;value=signaling(c,bits)?c:signaling(a,bits)?a:b;value|=quiet(bits);
      }else if(invalid_product){softfloat_exceptionFlags|=softfloat_flag_invalid;value=default_nan(bits);}
      else value=nan(c,bits)?c:nan(a,bits)?a:b;
      return finish(value,bits);
    }
    return finish(bits==32?std::uint64_t(f32_mulAdd(float32_t{std::uint32_t(a)},float32_t{std::uint32_t(b)},float32_t{std::uint32_t(c)}).v):f64_mulAdd(float64_t{a},float64_t{b},float64_t{c}).v,bits);
  }
  unsigned compare(unsigned bits,std::uint64_t a,std::uint64_t b,bool signal_all_nan){
    begin();a=input(a,bits);b=input(b,bits);
    if(nan(a,bits)||nan(b,bits)){if(signal_all_nan||signaling(a,bits)||signaling(b,bits))state_.fpsr|=1;return 3;}
    if(a==b||(!(a&~sign(bits))&&!(b&~sign(bits))))return 6;return less(a,b,bits)?8:2;
  }
  std::uint64_t round_integral(unsigned bits,std::uint64_t value,unsigned mode,bool exact){begin();value=input(value,bits);return finish(bits==32?std::uint64_t(f32_roundToInt(float32_t{std::uint32_t(value)},std::uint_fast8_t(mode),exact).v):f64_roundToInt(float64_t{value},std::uint_fast8_t(mode),exact).v,bits);}
  std::uint64_t from_int(unsigned bits,std::uint64_t value,bool is_signed,unsigned integer_bits){
    begin();if(integer_bits==32){value&=0xffffffff;if(is_signed&&(value&0x80000000))value|=UINT64_C(0xffffffff00000000);}
    const auto result=bits==32?(is_signed?std::uint64_t(i64_to_f32(signed_value(value)).v):ui64_to_f32(value).v):(is_signed?i64_to_f64(signed_value(value)).v:ui64_to_f64(value).v);
    return finish(result,bits);
  }
  std::uint64_t to_int(unsigned bits,std::uint64_t value,bool is_signed,unsigned integer_bits,unsigned mode){
    begin();value=input(value,bits);std::uint64_t result=0;
    if(bits==32){const float32_t x{std::uint32_t(value)};result=integer_bits==32?(is_signed?std::uint64_t(f32_to_i32(x,std::uint_fast8_t(mode),true)):f32_to_ui32(x,std::uint_fast8_t(mode),true)):
      (is_signed?std::uint64_t(f32_to_i64(x,std::uint_fast8_t(mode),true)):f32_to_ui64(x,std::uint_fast8_t(mode),true));}
    else{const float64_t x{value};result=integer_bits==32?(is_signed?std::uint64_t(f64_to_i32(x,std::uint_fast8_t(mode),true)):f64_to_ui32(x,std::uint_fast8_t(mode),true)):
      (is_signed?std::uint64_t(f64_to_i64(x,std::uint_fast8_t(mode),true)):f64_to_ui64(x,std::uint_fast8_t(mode),true));}
    flags(unsigned(softfloat_exceptionFlags));return integer_bits==32?result&0xffffffff:result;
  }
  std::uint64_t to_fixed(unsigned bits,std::uint64_t value,bool is_signed,unsigned fracbits,unsigned integer_bits=0){
    // FPToFixed scales the exact real value, without an intermediate floating
    // multiply or its overflow/inexact flags. Apply FZ to the original input.
    value=input(value,bits);const unsigned shift=bits==32?23u:52u;
    const auto signbit=value&sign(bits),mask=fraction(bits),maximum=exponent(bits)>>shift;
    auto exp=(value>>shift)&maximum,mantissa=value&mask;
    if(exp!=maximum&&(exp||mantissa)){
      while(!exp&&fracbits){mantissa<<=1;--fracbits;if(mantissa&(UINT64_C(1)<<shift)){exp=1;mantissa&=mask;}}
      exp+=fracbits;
      value=signbit|(exp>=maximum?exponent(bits):(exp<<shift)|mantissa);
    }
    return to_int(bits,value,is_signed,integer_bits?integer_bits:bits,softfloat_round_minMag);
  }
  std::uint64_t from_fixed(unsigned bits,std::uint64_t value,bool is_signed,unsigned integer_bits,unsigned fracbits){
    const auto converted=from_int(bits,value,is_signed,integer_bits);
    if(!(converted&~sign(bits)))return converted;
    // An integer divided by at most 2^64 stays normal in FP32/FP64.
    // Scaling is exact, so the existing integer conversion rounds only once.
    const unsigned shift=bits==32?23u:52u;const auto mask=exponent(bits);
    return (converted&~mask)|((((converted&mask)>>shift)-fracbits)<<shift);
  }
  std::uint64_t convert(unsigned source_bits,unsigned destination_bits,std::uint64_t value){
    begin();if(source_bits!=16)value=input(value,source_bits);std::uint64_t result=0;
    if(source_bits==32&&destination_bits==64)result=f32_to_f64(float32_t{std::uint32_t(value)}).v;
    else if(source_bits==64&&destination_bits==32)result=f64_to_f32(float64_t{value}).v;
    else if(destination_bits==16){result=source_bits==32?f32_to_f16(float32_t{std::uint32_t(value)}).v:f64_to_f16(float64_t{value}).v;flags(unsigned(softfloat_exceptionFlags));
      if((state_.fpcr&(1u<<25))&&(result&0x7c00)==0x7c00&&(result&0x3ff))result=0x7e00;return result;}
    else if(source_bits==16){result=destination_bits==32?std::uint64_t(f16_to_f32(float16_t{std::uint16_t(value)}).v):f16_to_f64(float16_t{std::uint16_t(value)}).v;}
    else throw std::runtime_error("Unsupported FP format conversion");
    return finish(result,destination_bits);
  }
 private:FloatingState& state_;
};
} // namespace reverse_pc::guest
