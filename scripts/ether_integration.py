"""Reviewed, exact-anchor extensions of the pinned owner, grammar and memory."""
def configure(expected,replacements):
    expected.update({'infinite_program.hpp':'32c652d6e6e9f4ff93d5f8a7b84ff5f0f3be0533ad54a2c78b29eaa32c3be890','inverse_memory.hpp':'d24303e5ac839354a4af8b7446bfb7d4e13f52c24b2f82c9b0efa549c6650657'})
    replacements['infinite_core.hpp'] += [
        ('#include "mundo_x/ether_core.hpp"','#include "mundo_x/ether_core.hpp"\n#include "integration/quantum_block.hpp"\n#include "integration/born_sampling.hpp"'),
        ('    std::size_t max_active_qubits = 4096;', '    std::size_t max_reduced_components = 65536;\n    std::size_t max_dense_block = 8;\n    bool fast_reduced_clifford = true;\n    bool factor_pair_shortcuts = true;\n    bool direct_ether_phases = true;\n    std::size_t max_active_qubits = 4096;'),
        ('    state_ = std::make_shared<State>();','    if (!limits.max_reduced_components || limits.max_dense_block > 14) throw std::invalid_argument("integrated representation limits");\n    state_ = std::make_shared<State>();'),
        ('    CompactCore core;','    reverse_pc::QuantumBlock core;'),
        ('Component(std::vector<std::string> addresses, CompactCore state)','Component(std::vector<std::string> addresses, reverse_pc::QuantumBlock state)'),
        ('    CompactCore core(count);','    reverse_pc::QuantumBlock core(CompactCore(count), limits_.max_reduced_components, limits_.max_dense_block, limits_.fast_reduced_clifford);'),
        ('        CompactCore::tensor_product(left->core, right->core));','        reverse_pc::QuantumBlock::tensor_product(left->core, right->core));\n    const auto actual_bytes=checked_add(unchanged_bytes,joined->core.state_bytes(),"quantum join overflow");\n    check_resources(state.locations.size(),width,actual_bytes);'),
        ('    state.tableau_bytes = projected_bytes;\n  }\n\n  void apply_gate', '    state.tableau_bytes = actual_bytes;\n  }\n\n  void apply_gate'),
        ('      materialize(state, gate.second);\n      join(state, gate.first, gate.second);', '      materialize(state, gate.second);\n      if (limits_.factor_pair_shortcuts && try_factor_pair(state,gate)) return;\n      join(state, gate.first, gate.second);'),
        ('    auto& core = mutable_component(state, first.component).core;','    const auto previous_bytes=state.components[first.component]->core.state_bytes();\n    auto& core = mutable_component(state, first.component).core;'),
        ("      case 'H': core.hadamard(first.qubit); break;", "      case 'T': core.phase_t(first.qubit); break;\n      case 'A': core.phase_t(first.qubit,true); break;\n      case 'H': core.hadamard(first.qubit); break;"),
        ('      default: throw std::logic_error("unvalidated gate");\n    }\n  }','      default: throw std::logic_error("unvalidated gate");\n    }\n    state.tableau_bytes=checked_add(state.tableau_bytes-previous_bytes,core.state_bytes(),"quantum gate payload overflow");\n    check_resources(state.locations.size(),state.components[first.component]->ids.size(),state.tableau_bytes);\n  }'),
        ("          gate.op != 'Y' && gate.op != 'Z')", "          gate.op != 'Y' && gate.op != 'Z' && gate.op != 'T' && gate.op != 'A')"),
        ("    switch (gate.op) {\n      case 'X': case 'Y': case 'Z':", "    switch (gate.op) {\n      case 'T': case 'A': {\n        const auto pulled=pull_pauli(state,{{gate.first,'Z'}});\n        if(pulled.terms.empty())return;\n        if(limits_.direct_ether_phases && try_direct_phase(state,pulled,gate.op=='A'))return;\n        parity_basis(state,pulled,false);\n        const bool dagger=(gate.op=='A')!=(pulled.sign<0);\n        apply_gate(state,{dagger?'A':'T',pulled.terms.front().address,{}});\n        parity_basis(state,pulled,true);\n        break;\n      }\n      case 'X': case 'Y': case 'Z':"),
        ('      if (observers) {','      if (!state.components[query.first]->core.is_clifford()) {\n        answer *= state.components[query.first]->core.expectation(query.second);\n      } else if (observers) {'),
        ('    const auto& component = *state.components[slot];\n    const mundo_x_bridge::PauliObserver observer(component.core);','    const auto& component = *state.components[slot];\n    if (!component.core.is_clifford()) return false;\n    const mundo_x_bridge::PauliObserver observer(component.core);'),
        ('  std::string snapshot() const {','  std::string snapshot() const {\n    if (has_non_clifford(*state_)) return integrated_snapshot();'),
        ('  static InfiniteCore from_snapshot(const std::string& saved, const Limits& limits) {','  static InfiniteCore from_snapshot(const std::string& saved, const Limits& limits) {\n    if (saved.compare(0,5,"MXI3:")==0) return from_integrated_snapshot(saved,limits);'),
        ('      auto core = CompactCore::from_packed_state(packed);','      auto core = reverse_pc::QuantumBlock(CompactCore::from_packed_state(packed), limits.max_reduced_components, limits.max_dense_block, limits.fast_reduced_clifford);'),
        ('    const auto answer = (1.0 - frame_expectation(*state_, pulled)) * 0.5;','    const auto answer = pulled_probability(*state_, pulled);'),
        ('    if (probability != 0.5)','    if (z_determinism(id) != 0)'),
        ('    const bool outcome_one = (candidate->rng() & 1U) != 0;', '    const bool outcome_one = sample_one(candidate->rng,probability,probability_zero(id));'),
        ('    const CompactCore::ZMeasurement outcome{outcome_one, 0.5, true};','    const CompactCore::ZMeasurement outcome{outcome_one, outcome_one?probability:probability_zero(id), true};\n    refresh_payload(*candidate);'),
        ('    const double probability = outcome_one ? one : 1.0 - one;', '    const double probability = outcome_one ? one : probability_zero(id);'),
        ('    if (probability == 1.0) return 1.0;', '    if (z_determinism(id) != 0) return 1.0;'),
        ('    mutable_component(*candidate, location.component).core.measure_z_forced(\n        location.qubit, outcome_one);','    mutable_component(*candidate, location.component).core.measure_z_forced(\n        location.qubit, outcome_one);\n    refresh_payload(*candidate);'),
        ('    const double one = (1.0 - frame_expectation(*state_, p)) * 0.5;','    const double one = pulled_probability(*state_, p);\n    const double zero = pulled_probability(*state_, p,false);'),
        ('    if (one != 0.5) {','    if (pulled_eigen(*state_,p) != 0) {'),
        ('forced ? requested : (candidate->rng() & 1U) != 0','forced ? requested : sample_one(candidate->rng,one,zero)'),
        ('    return {physical_outcome, 0.5, !forced};','    return {physical_outcome, physical_outcome?one:zero, !forced};'),
        ('    parity_basis(*candidate, p, true);\n    ++candidate->revision;', '    refresh_payload(*candidate);\n    parity_basis(*candidate, p, true);\n    refresh_payload(*candidate);\n    ++candidate->revision;'),
        ('  friend class QuantumRuntime;','#include "integration/infinite_state_extensions.inc"\n  friend class QuantumRuntime;'),
    ]
    replacements['infinite_program.hpp']=[
        ('return command == "h" || command == "s"', 'return command == "t" || command == "tdg" || command == "h" || command == "s"'),
        (": static_cast<char>(command.front() - ('a' - 'A'));", ": command == \"tdg\" ? 'A' : static_cast<char>(command.front() - ('a' - 'A'));"),
        ('  if (command == "stats") {', '''  if (command == "qstats") {
    require_count(words,1);const auto s=core.quantum_stats();
    out<<"{\\"type\\":\\"quantum_stats\\",\\"clifford_components\\":"<<s.clifford<<",\\"adaptive_components\\":"<<s.adaptive<<",\\"reduced_components\\":"<<s.reduced<<",\\"coefficients\\":"<<s.coefficients<<",\\"factor_amplitudes\\":"<<s.amplitudes<<",\\"T_instructions\\":"<<s.t_gates<<",\\"payload_bytes\\":"<<s.payload_bytes<<",\\"norm\\":"<<s.norm<<"}\\n";return;
  }
  if (command == "stats") {'''),
    ]
    replacements['inverse_memory.hpp']=[
        ('  ReadResult read(const std::string& logical_address, const InfiniteCore& core) const {', '''  // Numeric callers share the original rule/delta and quantum bindings.
  // Fall back to the original canonicalization when its digit budget may
  // reject a uint64 address or when any quantum view needs resolution.
  ReadResult read_numeric(std::uint64_t address, const InfiniteCore& core) const {
    if (limits_.max_address_digits < 20 || !bindings_->empty())
      return read(std::to_string(address), core);
    const auto* changed = overrides_->get(address);
    return {true, changed ? *changed : background_, {}};
  }

  ReadResult read(const std::string& logical_address, const InfiniteCore& core) const {'''),
        ('      const double probability = core.probability_one(q);', '      const auto deterministic=core.z_determinism(q);\n      const double probability = deterministic<0 ? 1.0 : 0.0;'),
        ('      if (probability == 0.5)','      if (deterministic == 0)'),
        ('  static InverseMemory from_snapshot(const std::string& saved, Limits limits) {','''  std::string persistent_snapshot() const {
    return "MXIM2 " + std::to_string(revision_) + "\\n" + snapshot();
  }
  static InverseMemory from_snapshot(const std::string& saved, Limits limits) {
    if (saved.compare(0,6,"MXIM2 ")==0) {
      const auto end=saved.find('\\n');
      if(end==std::string::npos || end<7 || end>27)throw std::invalid_argument("memory revision envelope");
      std::uint64_t revision=0;
      for(std::size_t i=6;i<end;++i){const auto c=saved[i];if(c<'0'||c>'9'||revision>(std::numeric_limits<std::uint64_t>::max()-unsigned(c-'0'))/10)throw std::invalid_argument("memory revision range");revision=revision*10+unsigned(c-'0');}
      auto result=from_snapshot(saved.substr(end+1),limits);result.revision_=revision;return result;
    }'''),
    ]
    replacements['quantum_runtime.hpp'] += [
        ('  // Restore both authorities transactionally. A bad core or memory snapshot',
'''  // Original state restoration with a non-owning view of memory transport.
  void restore_view(const std::string& core_saved,std::string_view memory_saved){
    auto candidate_core=InfiniteCore::from_snapshot(core_saved,core_.limits());
    auto candidate_memory=InverseMemory::from_snapshot_view(memory_saved,memory_.limits());
    candidate_memory.validate_quantum_bindings(candidate_core);
    core_=std::move(candidate_core);memory_=std::move(candidate_memory);
  }

  // Restore both authorities transactionally. A bad core or memory snapshot'''),
        ('  std::string memory_snapshot() const { return memory_.snapshot(); }','  std::string memory_snapshot() const { return memory_.snapshot(); }\n  std::string memory_persistent_snapshot() const { return memory_.persistent_snapshot(); }'),
    ]
