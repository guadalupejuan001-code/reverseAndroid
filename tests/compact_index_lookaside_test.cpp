#include "integration/adaptive_override_map.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using Map=mundo_x::memory_detail::AdaptiveOverrideMap;
namespace {
unsigned checks=0;
void check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
void value(const Map& m,std::uint64_t address,unsigned expected){const auto p=m.get(address);check(p&&*p==expected,"Lookup returned missing/stale bytes");}
void missing(const Map& m,std::uint64_t address){check(m.get(address)==nullptr,"Lookup returned a removed/newly absent page");}
std::vector<std::string> records(const Map& m){std::vector<std::string> out;m.for_each_page_record([&](auto n,const auto& mask,const auto& values){out.push_back(std::to_string(n));out.push_back(mask);out.push_back(values);});return out;}
void page_transitions(){
 Map m;missing(m,0);check(m.insert(UINT64_C(0),7),"Negative lookup hid insertion");value(m,0,7);
 for(unsigned n=1;n<160;n++)check(m.insert(std::uint64_t(n),std::uint8_t(n+1)),"Insertion failed");
 value(m,0,7);m.assign_existing(UINT64_C(0),99);value(m,0,99);
 const auto before=records(m);for(unsigned n=0;n<1000;n++)value(m,0,99);check(records(m)==before,"Lookup metadata affected original page encoding");
 for(unsigned n=1;n<160;n++)check(m.erase(std::uint64_t(n)),"Erase failed");
 value(m,0,99);check(m.erase(UINT64_C(0)),"Final erase failed");missing(m,0);
 check(m.insert(UINT64_C(0),12),"Same page reinsertion failed");value(m,0,12);
 for(unsigned p=1;p<128;p++){const auto a=std::uint64_t(p)*4096+17;missing(m,a);m.insert(a,std::uint8_t(p));value(m,a,std::uint8_t(p));}
 const auto edge=UINT64_MAX;missing(m,edge);m.insert(edge,231);value(m,edge,231);m.erase(edge);missing(m,edge);
}
void cow_and_reuse(){
 Map a;for(unsigned n=0;n<512;n++)a.insert(std::uint64_t(n),std::uint8_t(n));
 value(a,17,17);Map b=a;value(b,17,17);b.assign_existing(UINT64_C(17),101);value(a,17,17);value(b,17,101);
 a.assign_existing(UINT64_C(17),222);value(a,17,222);value(b,17,101);
 const auto old=records(a);Map snapshot=a;
 for(unsigned n=0;n<512;n++)a.erase(std::uint64_t(n));
 missing(a,17);value(snapshot,17,222);check(records(snapshot)==old,"COW lookup crossed snapshot pages");
 for(unsigned n=0;n<10000;n++){
  a=Map{};missing(a,17);a.insert(UINT64_C(17),std::uint8_t(n));value(a,17,std::uint8_t(n));
  Map copy=a;value(copy,17,std::uint8_t(n));a.assign_existing(UINT64_C(17),std::uint8_t(n+1));value(a,17,std::uint8_t(n+1));value(copy,17,std::uint8_t(n));
 }
 value(snapshot,17,222);value(b,17,101);
}
void imported_pages_and_threads(){
 Map a;const std::uint64_t number=7123,address=number*4096+35;missing(a,address);
 std::string bitmap(512,'\0');bitmap[35/8]=char(1u<<(35%8));a.load_page_record(number,bitmap,std::string(1,char(177)));value(a,address,177);
 const auto encoded=records(a);for(unsigned n=0;n<32;n++)value(a,address,177);check(records(a)==encoded,"Imported page changed after lookups");
 std::promise<void> ready,changed;auto barrier=changed.get_future();
 auto reader=std::async(std::launch::async,[&]{const auto p=a.get(address);const bool before=p&&*p==177;const bool absent=a.get(address+4096)==nullptr;ready.set_value();barrier.wait();const auto q=a.get(address+4096);return before&&absent&&a.get(address)==nullptr&&q&&*q==73;});
 ready.get_future().wait();a.erase(address);a.insert(address+4096,73);changed.set_value();check(reader.get(),"Another thread retained an invalid dictionary pointer");
}
}
int main(){try{page_transitions();cow_and_reuse();imported_pages_and_threads();std::cout<<"{\"checks\":"<<checks<<",\"lookaside_bytes_are_not_serialized\":true,\"full_android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
