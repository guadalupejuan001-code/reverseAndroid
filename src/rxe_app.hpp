#pragma once
// RXE2 application capsule. The BBY Q profile transports bytes to the owner's
// compiled XI program; all language, reasoning and learning stay inside XI.
#include "rxe.hpp"
#include "rvpc2.hpp"
#include <charconv>

namespace reverse_pc::rxe_app {
using rxe::Bytes;
using rxe::require;
inline constexpr unsigned char magic[8]={'R','X','E','2',13,10,26,10};
inline bool has_magic(const Bytes& b){return b.size()>=8&&std::equal(std::begin(magic),std::end(magic),b.begin());}
struct App {
    std::string id,title,version,source;
    Bytes code,seed;
};
inline bool identifier(const std::string& s){
    return !s.empty()&&s.size()<=96&&s.front()>='a'&&s.front()<='z'&&
        std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-';});
}
inline void validate(const App& app){
    require(identifier(app.id),"Identidad de aplicación RXE no válida");
    require(!app.title.empty()&&app.title.size()<=160&&rxe::utf8(app.title),"Nombre de aplicación RXE no válido");
    require(!app.version.empty()&&app.version.size()<=64&&rxe::utf8(app.version),"Versión de aplicación RXE no válida");
    require(app.source.size()==40&&std::all_of(app.source.begin(),app.source.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"Referencia de código RXE no válida");
    require(!app.seed.empty()&&app.seed.size()<=rxe::max_image,"Memoria inicial RXE no válida");
    (void)rxe::disassemble(rxe::decode(app.code));
    (void)rvpc2::restore(app.seed);
}
inline Bytes pack(const App& app){
    validate(app);
    const auto total=64+app.id.size()+app.title.size()+app.version.size()+app.source.size()+app.code.size()+app.seed.size();
    require(total<=rxe::max_image,"La aplicación supera 8 MiB");
    Bytes b(magic,magic+8);
    for(auto v:{std::uint64_t(2),std::uint64_t(2)})rxe::put(b,v,2);
    for(auto v:{std::uint64_t(64),std::uint64_t(total),std::uint64_t(app.id.size()),std::uint64_t(app.title.size()),std::uint64_t(app.version.size()),std::uint64_t(app.code.size()),std::uint64_t(app.seed.size())})rxe::put(b,v,4);
    rxe::put(b,0,8);
    for(unsigned v:{255u,1u,0u,40u})rxe::put(b,v,4);
    for(const auto* s:{&app.id,&app.title,&app.version,&app.source})b.insert(b.end(),s->begin(),s->end());
    b.insert(b.end(),app.code.begin(),app.code.end());b.insert(b.end(),app.seed.begin(),app.seed.end());
    rxe::patch(b,40,rxe::checksum(b),8);return b;
}
inline App decode(const Bytes& b){
    require(b.size()>=64&&b.size()<=rxe::max_image&&has_magic(b),"No es una aplicación RXE2");
    rxe::Reader in{b,8};
    require(in.get(2)==2&&in.get(2)==2&&in.get(4)==64,"Versión o ABI de aplicación RXE no compatible");
    require(in.get(4)==b.size(),"Longitud de aplicación RXE incorrecta");
    auto ni=in.get(4),nt=in.get(4),nv=in.get(4),nc=in.get(4),ns=in.get(4),hash=in.get(8);
    require(hash==rxe::checksum(b),"Integridad de aplicación RXE incorrecta");
    require(in.get(4)==255&&in.get(4)==1&&in.get(4)==0&&in.get(4)==40,"Perfil de aplicación RXE no compatible");
    require(ni>=1&&ni<=96&&nt>=1&&nt<=160&&nv>=1&&nv<=64&&nc>=56&&ns>=29&&ni+nt+nv+40+nc+ns==b.size()-64,"Secciones de aplicación RXE no válidas");
    App a;a.id=in.text(std::size_t(ni));a.title=in.text(std::size_t(nt));a.version=in.text(std::size_t(nv));a.source=in.text(40);
    const auto code=in.text(std::size_t(nc)),seed=in.text(std::size_t(ns));
    a.code.assign(code.begin(),code.end());a.seed.assign(seed.begin(),seed.end());
    require(in.pos==b.size(),"Datos sobrantes en aplicación RXE");validate(a);return a;
}
inline std::string describe(const Bytes& b){
    const auto app=decode(b);const auto image=rxe::decode(app.code);
    return "{\"format\":\"RXE\",\"version\":2,\"abi\":\"BBYQ-conversation-v1\",\"ui\":\"chat\",\"app_id\":"+rxe::quote(app.id)+",\"title\":"+rxe::quote(app.title)+",\"app_version\":"+rxe::quote(app.version)+",\"source_sha\":"+rxe::quote(app.source)+",\"instructions\":"+std::to_string(image.code.size())+",\"seed_bytes\":"+std::to_string(app.seed.size())+",\"input_bytes\":255,\"bytes\":"+std::to_string(b.size())+"}";
}
// Identical mailbox to BBY-Q ai/native/session.cpp. No question parsing here.
inline std::string input(const std::string& message,unsigned language){
    require(!message.empty()&&message.size()<=255,"Introduce entre 1 y 255 bytes de texto");
    require(language==1||language==2,"Idioma de salida no válido");
    require(rxe::utf8(message),"Texto UTF-8 no válido");
    std::string code=std::string(rxe::xi::machine_header)+"\nstore8 7400000000000000016 1\nstore8 7400000000000000017 "+std::to_string(message.size())+"\nstore8 7400000000000000003 "+std::to_string(language)+"\n";
    for(std::size_t n=0;n<message.size();n++)code+="store8 "+std::to_string(UINT64_C(7400000000000000128)+n)+" "+std::to_string(static_cast<unsigned char>(message[n]))+"\n";
    return code;
}
inline std::string speech(const std::string& jsonl){
    std::string result,line;std::istringstream rows(jsonl);
    while(std::getline(rows,line)){
        if(line.find("\"register\":\"r14\"")==std::string::npos&&line.find("\"register\":\"r9\"")==std::string::npos)continue;
        auto at=line.find("\"value\":");require(at!=std::string::npos,"Salida RXE incompleta");at+=8;
        unsigned value=256;const auto parsed=std::from_chars(line.data()+at,line.data()+line.size(),value);
        require(parsed.ec==std::errc{}&&parsed.ptr<line.data()+line.size()&&*parsed.ptr=='}'&&value<=255,"Byte de salida RXE no válido");
        require(result.size()<4096,"La respuesta supera 4096 bytes");result+=char(value);
    }
    require(!result.empty()&&rxe::utf8(result),"El programa no produjo una respuesta UTF-8 válida");return result;
}
struct Turn {std::string text;Bytes state;};
inline Turn turn(const Bytes& executable,const Bytes& previous,const std::string& message,unsigned language){
    const auto prefix=input(message,language);const auto app=decode(executable);
    auto source=rxe::disassemble(rxe::decode(app.code));source.erase(0,source.find('\n')+1);
    auto runtime=rvpc2::restore(previous.empty()?app.seed:previous);
    const auto lines=runtime.execute(prefix+source);
    const auto text=speech(lines);return {text,rvpc2::save(runtime)};
}
}
