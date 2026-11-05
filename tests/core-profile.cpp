#include "../reverse-os-android/app/src/main/cpp/session.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
  reverse_os::Runtime state;
  if(argc==2){std::ifstream in(argv[1],std::ios::binary);if(!in)return 2;std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};state=reverse_os::restore(bytes);}
  const auto q=state.core().stats();const auto m=state.memory().stats();
  const auto limits=state.core().limits();
  std::cout<<"{\"active_qubits\":"<<q.active_qubits<<",\"components\":"<<q.components
    <<",\"largest_component\":"<<q.largest_component<<",\"tableau_bytes\":"<<q.tableau_bytes
    <<",\"ether_rules\":"<<q.ether_rules<<",\"core_revision\":"<<q.revision
    <<",\"memory_revision\":"<<m.revision<<",\"memory_overrides\":"<<m.overrides
    <<",\"quantum_bindings\":"<<m.quantum_bindings<<",\"state_format\":\"RVPC2\""
    <<",\"max_active_qubits\":"<<limits.max_active_qubits
    <<",\"max_component_qubits\":"<<limits.max_component_qubits
    <<",\"max_state_bytes\":"<<limits.max_state_bytes
    <<",\"max_address_digits\":"<<limits.max_address_digits<<"}\n";
}
