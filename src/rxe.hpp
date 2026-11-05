#pragma once
// RXE v1: binary executable image for the existing XI Machine v1 ABI.
// The loader lowers validated bytecode to the original transactional runtime;
// it does not implement another CPU, state authority, or operating system.
#include "mundo_x/quantum_runtime.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace reverse_pc::rxe {
using Bytes=std::vector<std::uint8_t>;
namespace xi=mundo_x::infinite_program_detail;
inline constexpr std::size_t header_size=56,max_image=8*1024*1024,max_constants=65536,max_operands=524288;
inline constexpr unsigned char magic[8]={'R','X','E','1',13,10,26,10};
enum class Kind:std::uint8_t { Constant=0,Register=1,Integer=2,Dynamic=3,Target=4,PauliDynamic=5 };
struct Operand {Kind kind;std::uint64_t value;};
struct Instruction {std::uint32_t line;std::uint16_t opcode;std::vector<Operand> args;};
struct Spec {const char* name;unsigned low,high;};
// IDs are part of the file ABI. Append only; never reorder these entries.
inline const std::vector<Spec>& specs(){static const std::vector<Spec> s={
 {"",0,0},{"set",2,2},{"mov",2,2},{"add",2,2},{"sub",2,2},{"mul",2,2},{"div",2,2},{"mod",2,2},{"cmp",2,2},
 {"load8",2,2},{"store8",2,2},{"print",1,1},{"halt",0,0},{"label",1,1},
 {"jmp",1,1},{"je",1,1},{"jne",1,1},{"jlt",1,1},{"jle",1,1},{"jgt",1,1},{"jge",1,1},
 {"background",1,1},{"begin",0,0},{"end",0,0},{"h",1,1},{"s",1,1},{"x",1,1},{"y",1,1},{"z",1,1},
 {"cx",2,2},{"cz",2,2},{"t",1,1},{"tdg",1,1},{"h_all",0,0},{"cz_neighbors",0,0},
 {"qstats",0,0},{"stats",0,0},{"seed",1,1},{"expect",0,4096},{"probz",1,1},{"measure",1,1},
 {"postselect",2,2},{"compact",0,0},{"mem_reset",1,1},{"mem_store",2,2},{"mem_bind",2,2},
 {"mem_unbind",1,1},{"mem_read",1,1},{"mem_materialize",2,2},{"mem_stats",0,0},{"assert",2,4096}};return s;}
inline void require(bool ok,const char* error){if(!ok)throw std::invalid_argument(error);}
inline bool utf8(const std::string& s){
 for(std::size_t i=0;i<s.size();){unsigned char c=s[i++];if(c<128){if(c<32||c==127)return false;continue;}
  unsigned count;std::uint32_t n,min;
  if(c>=0xc2&&c<=0xdf){count=1;n=c&31;min=0x80;}
  else if(c>=0xe0&&c<=0xef){count=2;n=c&15;min=0x800;}
  else if(c>=0xf0&&c<=0xf4){count=3;n=c&7;min=0x10000;}else return false;
  if(count>s.size()-i)return false;while(count--){unsigned char next=s[i++];if((next&0xc0)!=0x80)return false;n=(n<<6)|(next&63);}
  if(n<min||n>0x10ffff||(n>=0xd800&&n<=0xdfff))return false;
 }return true;
}
inline bool token(const std::string& s){return !s.empty()&&s.size()<=16384&&utf8(s)&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<=32||c=='#';});}
inline void put(Bytes& out,std::uint64_t value,unsigned width){for(unsigned i=0;i<width;i++)out.push_back(std::uint8_t(value>>(8*i)));}
inline void patch(Bytes& out,std::size_t pos,std::uint64_t value,unsigned width){for(unsigned i=0;i<width;i++)out.at(pos+i)=std::uint8_t(value>>(8*i));}
struct Reader {const Bytes& bytes;std::size_t pos=0;
 std::uint64_t get(unsigned width){require(pos<=bytes.size()&&width<=bytes.size()-pos,"RXE truncado");std::uint64_t v=0;for(unsigned i=0;i<width;i++)v|=std::uint64_t(bytes[pos++])<<(8*i);return v;}
 std::string text(std::size_t count){require(pos<=bytes.size()&&count<=bytes.size()-pos,"Texto RXE truncado");std::string s(reinterpret_cast<const char*>(bytes.data()+pos),count);pos+=count;return s;}
};
inline std::uint64_t checksum(const Bytes& b){std::uint64_t h=UINT64_C(14695981039346656037);for(std::size_t i=0;i<b.size();i++)if(i<40||i>=48){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
inline bool has_magic(const std::string& s){return s.size()>=8&&std::equal(std::begin(magic),std::end(magic),reinterpret_cast<const unsigned char*>(s.data()));}
inline unsigned width(Kind kind){switch(kind){case Kind::Register:case Kind::Dynamic:return 1;case Kind::PauliDynamic:return 2;case Kind::Integer:return 8;case Kind::Constant:case Kind::Target:return 4;}throw std::invalid_argument("Tipo de operando RXE desconocido");}
struct Image {
 std::string title;std::uint32_t source_lines=1;std::vector<std::string> constants;std::vector<Instruction> code;
};
inline bool value(Kind k){return k==Kind::Register||k==Kind::Integer;}
inline bool address(Kind k){return k==Kind::Constant||k==Kind::Dynamic;}
inline void validate(const Image& image){
 require(!image.title.empty()&&image.title.size()<=160&&utf8(image.title),"Nombre RXE no válido");
 require(!image.code.empty()&&image.code.size()<=xi::max_commands,"Número de instrucciones RXE no válido");
 require(image.constants.size()<=max_constants,"Demasiadas constantes RXE");
 require(image.source_lines>=2&&image.source_lines<=xi::max_program_bytes,"Mapa de líneas RXE no válido");
 for(const auto& s:image.constants)require(token(s),"Constante RXE no válida");
 std::map<std::string,std::size_t> labels;std::uint32_t last=1;std::size_t operands=0;
 for(std::size_t pc=0;pc<image.code.size();pc++){
  const auto& ins=image.code[pc];require(ins.line>last&&ins.line<=image.source_lines,"Línea RXE fuera del programa");last=ins.line;
  require(ins.opcode>0&&ins.opcode<specs().size(),"Instrucción RXE desconocida");const auto& spec=specs()[ins.opcode];
  require(ins.args.size()>=spec.low&&ins.args.size()<=spec.high,"Cantidad de operandos RXE no válida");
  operands+=ins.args.size();require(operands<=max_operands,"Demasiados operandos RXE");
  for(const auto& a:ins.args){
   (void)width(a.kind);
   if(a.kind==Kind::Constant)require(a.value<image.constants.size(),"Constante RXE fuera de rango");
   if(a.kind==Kind::Register||a.kind==Kind::Dynamic)require(a.value<16,"Registro RXE fuera de rango");
   if(a.kind==Kind::PauliDynamic)require((a.value&255)<16&&(a.value>>16)==0&&std::string("IXYZ").find(char(a.value>>8))!=std::string::npos,"Operando Pauli RXE no válido");
   if(a.kind==Kind::Target)require(a.value<image.code.size()&&image.code[std::size_t(a.value)].opcode==13,"Salto RXE fuera de una etiqueta");
  }
  const auto id=ins.opcode;
  if(id>=1&&id<=7)require(ins.args[0].kind==Kind::Register&&value(ins.args[1].kind),"Operando aritmético RXE no válido");
  else if(id==8)require(value(ins.args[0].kind)&&value(ins.args[1].kind),"Comparación RXE no válida");
  else if(id==9)require(ins.args[0].kind==Kind::Register&&address(ins.args[1].kind),"Carga RXE no válida");
  else if(id==10)require(address(ins.args[0].kind)&&value(ins.args[1].kind),"Escritura RXE no válida");
  else if(id==11)require(ins.args[0].kind==Kind::Register,"Salida RXE no válida");
  else if(id==13){require(ins.args[0].kind==Kind::Constant,"Etiqueta RXE no válida");auto label=xi::machine_label(image.constants[std::size_t(ins.args[0].value)]);require(labels.emplace(label,pc).second,"Etiqueta RXE duplicada");}
  else if(id>=14&&id<=20)require(ins.args[0].kind==Kind::Target,"Salto RXE no válido");
  else for(const auto& a:ins.args)require(a.kind==Kind::Constant||a.kind==Kind::Dynamic||a.kind==Kind::PauliDynamic,"Operando XI RXE no válido");
 }
}
inline std::string signed_integer(std::uint64_t bits){return bits<=std::uint64_t(INT64_MAX)?std::to_string(std::int64_t(bits)):std::to_string(-1-std::int64_t(~bits));}
inline std::string disassemble(const Image& image){
 validate(image);std::string source=std::string(xi::machine_header)+"\n";std::uint32_t line=2;
 auto append=[&](const std::string& s){require(s.size()<=xi::max_program_bytes-source.size(),"El programa RXE expandido supera 16 MiB");source+=s;};
 for(const auto& ins:image.code){
  while(line<ins.line){append("\n");line++;}append(specs()[ins.opcode].name);
  for(const auto& a:ins.args){append(" ");switch(a.kind){
   case Kind::Constant:append(image.constants[std::size_t(a.value)]);break;
   case Kind::Register:append("r"+std::to_string(a.value));break;
   case Kind::Integer:append(signed_integer(a.value));break;
   case Kind::Dynamic:append("$r"+std::to_string(a.value));break;
   case Kind::PauliDynamic:append(std::string(1,char(a.value>>8))+":$r"+std::to_string(a.value&255));break;
   case Kind::Target:append(image.constants[std::size_t(image.code[std::size_t(a.value)].args.at(0).value)]);break;
  }}append("\n");line++;
 }return source;
}
inline Image decode(const Bytes& bytes){
 require(bytes.size()>=header_size&&bytes.size()<=max_image,"Tamaño RXE no válido");
 require(std::equal(std::begin(magic),std::end(magic),bytes.begin()),"No es un ejecutable RXE");Reader in{bytes,8};
 require(in.get(2)==1,"Versión RXE no compatible");require(in.get(2)==1,"ABI RXE no compatible");require(in.get(4)==header_size,"Cabecera RXE no válida");
 require(in.get(4)==bytes.size(),"Longitud RXE no válida");auto count=in.get(4),pool=in.get(4),title_size=in.get(4);
 require(in.get(4)==0&&in.get(4)==0,"Entrada o indicadores RXE no compatibles");const auto hash=in.get(8);auto lines=in.get(4);require(in.get(4)==0,"Campos reservados RXE no válidos");
 require(hash==checksum(bytes),"Integridad RXE incorrecta");require(count>0&&count<=xi::max_commands&&pool<=max_constants&&title_size<=160,"Dimensiones RXE no válidas");
 Image image;image.source_lines=std::uint32_t(lines);image.title=in.text(std::size_t(title_size));
 require(pool<=(bytes.size()-in.pos)/5,"Tabla RXE truncada");image.constants.reserve(std::size_t(pool));
 for(std::uint64_t i=0;i<pool;i++){auto length=in.get(4);require(length>0&&length<=16384,"Longitud de constante RXE no válida");image.constants.push_back(in.text(std::size_t(length)));}
 require(count<=(bytes.size()-in.pos)/8,"Código RXE truncado");image.code.reserve(std::size_t(count));std::size_t total_args=0;
 for(std::uint64_t i=0;i<count;i++){
  Instruction ins;ins.line=std::uint32_t(in.get(4));ins.opcode=std::uint16_t(in.get(2));auto argc=in.get(2);total_args+=std::size_t(argc);require(argc<=4096&&total_args<=max_operands,"Presupuesto de operandos RXE excedido");ins.args.reserve(std::size_t(argc));
  for(std::uint64_t a=0;a<argc;a++){auto kind=static_cast<Kind>(in.get(1));ins.args.push_back({kind,in.get(width(kind))});}image.code.push_back(std::move(ins));
 }
 require(in.pos==bytes.size(),"Datos sobrantes en RXE");validate(image);return image;
}
inline Bytes compile(const std::string& source,const std::string& title){
 require(source.size()<=xi::max_program_bytes,"Fuente XI demasiado grande");auto first=source.substr(0,source.find('\n'));if(!first.empty()&&first.back()=='\r')first.pop_back();require(first==xi::machine_header,"RXE v1 requiere el perfil #!reverse-pc-machine-v1");
 Image image;image.title=title;std::map<std::string,std::uint32_t> intern,labels;std::vector<std::pair<std::size_t,std::string>> jumps;
 auto constant=[&](const std::string& s)->Operand{require(token(s),"Token XI no válido");auto it=intern.find(s);if(it==intern.end()){require(image.constants.size()<max_constants,"Demasiadas constantes RXE");auto id=std::uint32_t(image.constants.size());image.constants.push_back(s);it=intern.emplace(s,id).first;}return {Kind::Constant,it->second};};
 auto integer=[&](const std::string& s)->Operand{if(!s.empty()&&(s[0]=='r'||s[0]=='R'))return {Kind::Register,xi::machine_register(s)};return {Kind::Integer,std::uint64_t(xi::machine_integer(s))};};
 auto generic=[&](const std::string& s)->Operand{
  if(s.size()>1&&s[0]=='$')return {Kind::Dynamic,xi::machine_register(s.substr(1))};
  if(s.size()>3&&s[1]==':'&&s[2]=='$'&&std::string("IXYZ").find(s[0])!=std::string::npos)return {Kind::PauliDynamic,std::uint64_t(xi::machine_register(s.substr(3)))|(std::uint64_t(std::uint8_t(s[0]))<<8)};
  return constant(s);
 };
 std::istringstream input(source);std::string line;std::uint32_t number=0;std::size_t operand_count=0;
 while(std::getline(input,line)){
  ++number;require(line.size()<=xi::max_line_bytes,"Línea XI demasiado grande");auto words=xi::tokens(line);if(words.empty())continue;
  require(image.code.size()<xi::max_commands,"Demasiadas instrucciones RXE");auto name=xi::lower_ascii(words[0]);std::uint16_t id=0;
  for(std::size_t n=1;n<specs().size();n++)if(name==specs()[n].name){id=std::uint16_t(n);break;}require(id!=0,"Instrucción XI no admitida por RXE v1");
  const auto& spec=specs()[id];require(words.size()-1>=spec.low&&words.size()-1<=spec.high,"Cantidad de argumentos XI no válida");operand_count+=words.size()-1;require(operand_count<=max_operands,"Demasiados operandos RXE");Instruction ins{number,id,{}};
  if(id>=14&&id<=20){jumps.push_back({image.code.size(),xi::machine_label(words[1])});ins.args.push_back({Kind::Target,0});}
  else if(id==13){auto label=xi::machine_label(words[1]);require(labels.emplace(label,std::uint32_t(image.code.size())).second,"Etiqueta XI duplicada");ins.args.push_back(constant(label));}
  else for(std::size_t a=1;a<words.size();a++){
   if((id>=1&&id<=7&&a==1)||(id==9&&a==1)||id==11)ins.args.push_back({Kind::Register,xi::machine_register(words[a])});
   else if((id>=1&&id<=7&&a==2)||id==8||(id==10&&a==2))ins.args.push_back(integer(words[a]));
   else ins.args.push_back(generic(words[a]));
  }
  image.code.push_back(std::move(ins));
 }
 image.source_lines=number;
 for(const auto& jump:jumps){auto found=labels.find(jump.second);require(found!=labels.end(),"Etiqueta de salto XI inexistente");image.code[jump.first].args[0].value=found->second;}
 validate(image);(void)disassemble(image);
 Bytes bytes(magic,magic+8);put(bytes,1,2);put(bytes,1,2);put(bytes,header_size,4);put(bytes,0,4);put(bytes,image.code.size(),4);put(bytes,image.constants.size(),4);put(bytes,image.title.size(),4);put(bytes,0,4);put(bytes,0,4);put(bytes,0,8);put(bytes,image.source_lines,4);put(bytes,0,4);
 bytes.insert(bytes.end(),image.title.begin(),image.title.end());
 for(const auto& s:image.constants){put(bytes,s.size(),4);bytes.insert(bytes.end(),s.begin(),s.end());}
 for(const auto& ins:image.code){put(bytes,ins.line,4);put(bytes,ins.opcode,2);put(bytes,ins.args.size(),2);for(const auto& a:ins.args){put(bytes,std::uint8_t(a.kind),1);put(bytes,a.value,width(a.kind));}require(bytes.size()<=max_image,"Ejecutable RXE demasiado grande");}
 patch(bytes,16,bytes.size(),4);patch(bytes,40,checksum(bytes),8);return bytes;
}
inline std::string execute(mundo_x::QuantumRuntime& runtime,const Bytes& bytes){auto image=decode(bytes);return runtime.execute(disassemble(image));}
inline std::string quote(const std::string& s){std::string result="\"";for(char c:s){if(c=='"'||c=='\\')result+='\\';result+=c;}return result+'"';}
inline std::string describe(const Bytes& bytes){auto image=decode(bytes);return "{\"format\":\"RXE\",\"version\":1,\"abi\":\"XI-machine-v1\",\"title\":"+quote(image.title)+",\"instructions\":"+std::to_string(image.code.size())+",\"constants\":"+std::to_string(image.constants.size())+",\"bytes\":"+std::to_string(bytes.size())+"}";}
}
