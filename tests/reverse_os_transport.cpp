#include "../reverse-os-android/app/src/main/cpp/session.hpp"
#include <cassert>
#include <iostream>
#include <string>
int main(){
  auto initial=reverse_os::save(reverse_os::Runtime{});
  assert(initial.size()>=29);
  auto cpu=reverse_os::restore(initial);
  const auto text=cpu.execute("#!reverse-pc-machine-v1\n"
    "set r0 1071\nset r1 462\nlabel again\ncmp r1 0\nje done\n"
    "mov r2 r0\nmod r2 r1\nmov r0 r1\nmov r1 r2\njmp again\n"
    "label done\nstore8 42 r0\nprint r0\nhalt\n");
  assert(text.find("\"value\":21")!=std::string::npos);
  auto state=reverse_os::save(cpu);
  auto restored=reverse_os::restore(state);
  assert(restored.memory().read("42",restored.core()).value==21);
  assert(reverse_os::save(restored)==state);
  const auto packet=reverse_os::packet(text,state);
  assert(packet.size()==13+text.size()+state.size());
  auto old=reverse_os::save(restored);
  bool failed=false;
  try{restored.execute("#!reverse-pc-machine-v1\nstore8 42 99\ndiv r0 0\n");}
  catch(const std::exception&){failed=true;}
  assert(failed&&reverse_os::save(restored)==old);
  for(std::size_t pos:{std::size_t(0),std::size_t(5),std::size_t(12),std::size_t(18),std::size_t(28)}){
    auto corrupt=state;corrupt[pos]^=0x11;bool bad=false;
    try{(void)reverse_os::restore(corrupt);}catch(const std::exception&){bad=true;}
    assert(bad);
  }
  auto qubits=reverse_os::restore(initial);
  qubits.execute("#!reverse-pc-machine-v1\nh 0\ncx 0 127\nt 127\nexpect Z:0 Z:127\n");
  auto qstate=reverse_os::save(qubits);
  assert(reverse_os::save(reverse_os::restore(qstate))==qstate);
  std::cout<<"REVERSE_OS_TRANSPORT_PASS state="<<qstate.size()
           <<" gcd=21 quantum=128 rollback=true corruption=true\n";
}
