#include "rxe.hpp"
#include "../reverse-os-android/app/src/main/cpp/session.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

namespace rxe=reverse_pc::rxe;
static unsigned checks=0;
void check(bool ok){if(!ok)throw std::runtime_error("RXE check "+std::to_string(checks));checks++;}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}
void same(const std::string& source,const std::string& title="Prueba REVERSE"){
 auto image=rxe::compile(source,title);auto parsed=rxe::decode(image);check(parsed.title==title);
 check(rxe::compile(source,title)==image);check(!rxe::has_magic(source));
 mundo_x::QuantumRuntime text,binary;const auto a=text.execute(source),b=rxe::execute(binary,image);
 check(a==b);check(reverse_os::save(text)==reverse_os::save(binary));
 auto clone=reverse_os::restore(reverse_os::save(binary));check(reverse_os::save(clone)==reverse_os::save(binary));
}
int main(int argc,char** argv){
 try{
  const std::string header="#!reverse-pc-machine-v1\n";
  const std::string hello=header+"set r0 6\nmul r0 7\nprint r0\nhalt\n";
  same(hello,"Hola REVERSE");same(header+"set r0 -9223372036854775808\nprint r0\nhalt\n","Prueba \xf0\x9f\x92\xbb");
  same(header+"# comentario\n\nset R0 +0002\nset r1 -3\nadd r0 R1\nprint r0\nlabel loop\nadd r0 1\ncmp r0 5\njlt LOOP\nstore8 42 r0\nmem_read 42\nhalt\n");
  same(header+"set r0 0\nlabel loop\nadd r0 1\ncmp r0 1000\njlt loop\nprint r0\n");
  same(header+"seed 42\nset r0 200\nh $r0\nlabel grow\nmov r1 r0\nadd r1 1\ncx $r0 $r1\nmov r0 r1\ncmp r0 327\njlt grow\nt 327\nexpect Z:200 Z:$r0\nmeasure 200\nqstats\nhalt\n");
  same(header+"background zero\nbegin\nh 0\ncx 0 1\nend\nassert expect 1 Z:0 Z:1\nt 1\ntdg 1\nprobz 0\nmem_store 700 91\nmem_materialize 700 1\nmem_stats\nstats\n");
  if(argc>1){std::ifstream in(std::string(argv[1])+"/examples/machine-ghz-128.xi");std::string s((std::istreambuf_iterator<char>(in)),{});same(s);}
  std::mt19937 rng(42);
  for(unsigned i=0;i<100;i++){
   int start=int(rng()%101)-50,delta=int(rng()%11)-5;
   same(header+"set r0 "+std::to_string(start)+"\nadd r0 "+std::to_string(delta)+"\nmul r0 -3\nmod r0 19\nprint r0\nhalt\n");
  }
  for(const auto& source:{header+"set r16 1\n",header+"jmp missing\n",header+"label x\nlabel X\n",header+"unknown r0\n",header+"set r0 9223372036854775808\n",std::string("set r0 1\n")})reject([&]{rxe::compile(source,"bad");});
  const auto image=rxe::compile(hello,"A");
  for(std::size_t n=0;n<image.size();n++){auto cut=image;cut.resize(n);reject([&]{rxe::decode(cut);});}
  for(std::size_t n=0;n<image.size();n++){auto bad=image;bad[n]^=1;reject([&]{rxe::decode(bad);});}
  auto forged=[&](rxe::Bytes bad,std::size_t at,std::uint64_t value,unsigned width){rxe::patch(bad,at,value,width);rxe::patch(bad,40,rxe::checksum(bad),8);reject([&]{rxe::decode(bad);});};
  forged(image,57+4,65535,2); // unknown opcode, valid checksum
  forged(image,57+9,16,1);   // invalid register, valid checksum
  forged(image,57+8,255,1);  // unknown operand tag
  forged(image,32,1,4);      // unsupported entry point
  forged(image,24,UINT32_MAX,4); // malicious constant table count
  auto jump=rxe::compile(header+"jmp loop\nlabel loop\nhalt\n","A");
  forged(jump,56+1+4+4+8+1,0,4); // jump targets the jump, not its label
  auto constant=rxe::compile(header+"h 0\nhalt\n","A");forged(constant,56+1+4,10,1); // newline injection
  auto appended=image;appended.push_back(0);reject([&]{rxe::decode(appended);});
  for(unsigned i=0;i<500;i++){auto fuzz=image;fuzz[rng()%fuzz.size()]^=std::uint8_t(1u<<(rng()%8));rxe::patch(fuzz,40,rxe::checksum(fuzz),8);try{auto parsed=rxe::decode(fuzz);(void)rxe::disassemble(parsed);}catch(const std::exception&){}checks++;}
  mundo_x::QuantumRuntime runtime;runtime.execute(header+"store8 42 21\n");auto before=reverse_os::save(runtime);
  auto late=rxe::compile(header+"store8 42 99\nh 0\nseed 999\ndiv r0 0\n","fallo");reject([&]{rxe::execute(runtime,late);});check(reverse_os::save(runtime)==before);
  auto infinite=rxe::compile(header+"label loop\njmp loop\n","bucle");reject([&]{rxe::execute(runtime,infinite);});check(reverse_os::save(runtime)==before);
  reject([&]{rxe::execute(runtime,rxe::Bytes(hello.begin(),hello.end()));});check(reverse_os::save(runtime)==before);
  std::cout<<"RXE_PASS checks="<<checks<<" text_binary_equivalence=true structured_fuzz=500 rollback=true quantum=128 no_renamed_text=true\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"RXE_TEST_FAIL "<<e.what()<<"\n";return 1;}
}
