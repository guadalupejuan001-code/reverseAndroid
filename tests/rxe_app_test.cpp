#include "rxe_app.hpp"
#include <iostream>
#include <random>

namespace app=reverse_pc::rxe_app;
namespace rxe=reverse_pc::rxe;
unsigned checks=0;
void check(bool value){if(!value)throw std::runtime_error("app check "+std::to_string(checks));checks++;}
template<class F>void reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught);}
int main(){try{
    mundo_x::QuantumRuntime empty;auto seed=reverse_pc::rvpc2::save(empty);
    auto code=rxe::compile("#!reverse-pc-machine-v1\nload8 r14 7400000000000000128\nprint r14\nstore8 42 21\nhalt\n","Byte transport");
    app::App data{"org.reverse.test","Prueba RXE","1.0",std::string(40,'a'),code,seed};auto image=app::pack(data);
    auto parsed=app::decode(image);check(parsed.seed==seed&&parsed.code==code);check(app::pack(parsed)==image);check(app::has_magic(image));
    auto turn=app::turn(image,seed,"Hola",1);check(turn.text=="H");auto runtime=reverse_pc::rvpc2::restore(turn.state);check(runtime.execute("#!reverse-pc-machine-v1\nload8 r0 42\nprint r0\n").find("21")!=std::string::npos);
    check(reverse_pc::rvpc2::save(empty)==seed);check(app::turn(image,{},"English",2).text=="E");
    for(std::size_t n=0;n<image.size();n++){auto cut=image;cut.resize(n);reject([&]{app::decode(cut);});}
    for(std::size_t n=0;n<image.size();n++){auto broken=image;broken[n]^=1;reject([&]{app::decode(broken);});}
    for(auto at:{8u,10u,12u,16u,20u,24u,28u,32u,36u,48u,52u,56u,60u}){auto broken=image;rxe::patch(broken,at,UINT32_MAX,4);rxe::patch(broken,40,rxe::checksum(broken),8);reject([&]{app::decode(broken);});}
    auto bad=data;bad.id="../outside";reject([&]{app::pack(bad);});bad=data;bad.source="forged";reject([&]{app::pack(bad);});bad=data;bad.seed.back()^=1;reject([&]{app::pack(bad);});bad=data;bad.code={'t','e','x','t'};reject([&]{app::pack(bad);});
    reject([&]{app::turn(image,seed,"",1);});reject([&]{app::turn(image,seed,std::string(256,'x'),1);});reject([&]{app::turn(image,seed,"hola",3);});reject([&]{app::turn(image,seed,std::string(1,char(255)),1);});
    bad=data;bad.code=rxe::compile("#!reverse-pc-machine-v1\nstore8 42 99\nh 0\nseed 999\ndiv r0 0\n","Fallido");auto late=app::pack(bad);reject([&]{app::turn(late,seed,"hola",1);});check(seed==reverse_pc::rvpc2::save(empty));
    bad=data;bad.code=rxe::compile("#!reverse-pc-machine-v1\nset r14 300\nprint r14\n","Bad speech");reject([&]{app::turn(app::pack(bad),seed,"hola",1);});
    bad=data;bad.code=rxe::compile("#!reverse-pc-machine-v1\nset r0 65\nprint r0\n","Wrong output register");reject([&]{app::turn(app::pack(bad),seed,"hola",1);});
    std::mt19937 random(42);for(unsigned i=0;i<500;i++){auto fuzz=image;fuzz[random()%fuzz.size()]^=std::uint8_t(1u<<(random()%8));rxe::patch(fuzz,40,rxe::checksum(fuzz),8);try{auto valid=app::decode(fuzz);check(!valid.code.empty());}catch(const std::exception&){}checks++;}
    std::cout<<"RXE_APP_PASS checks="<<checks<<" package_integrity=true seed_preserved=true rollback=true mailbox=true\n";return 0;
}catch(const std::exception& error){std::cerr<<"RXE_APP_FAIL "<<error.what()<<'\n';return 1;}}
