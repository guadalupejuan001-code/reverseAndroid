#!/usr/bin/env python3
"""Build a hash-checked local extension; never edit the pinned checkout."""
import difflib
import hashlib
from pathlib import Path
import sys
source=Path(sys.argv[1])/'native/include/mundo_x'
output=Path(sys.argv[2])
expected={'exact_t_experiment.hpp':'89be6b63b9787d9ef9fe9ace7d9b507b1586a14e887cc99a42234d1b46488590'}
replacements={'exact_t_experiment.hpp':[(
    '    ReducedExactTRepresentation out(product.size());',
    '    ReducedExactTRepresentation out(product.size(), max_components);') ]}
# Whole-program candidates are never published until parsing, observations,
# memory mutations and all gates succeed. Standalone InfiniteCore keeps its
# original strong per-operation guarantee; only the private runtime candidate
# may mutate uniquely owned metadata in place.
expected.update({
    'infinite_core.hpp':'ff18ec11360f90387ee86a8ca6aa42e8841cfa56fa74d27a7efc88024abe7791',
    'quantum_runtime.hpp':'ed721c2e6d45037ac34784437abfd61265d62917fb8e45bd67fbd92aa5fda64f',
})
replacements['infinite_core.hpp']=[(
    '    const auto normalized = normalize_gates(gates);\n    require_next_revision();\n    auto candidate = std::make_shared<State>(*state_);',
    '    const auto normalized = normalize_gates(gates);\n    require_next_revision();\n'
    '    if (program_candidate_ && state_.unique()) {\n'
    '      for (const auto& gate : normalized) apply_physical_gate(*state_, gate);\n'
    '      ++state_->revision;\n'
    '      return;\n'
    '    }\n'
    '    auto candidate = std::make_shared<State>(*state_);'),(
    '  Background background_;',
    '  friend class QuantumRuntime;\n'
    '  bool program_candidate_ = false;\n'
    '  Background background_;')]
replacements['quantum_runtime.hpp']=[(
    '    auto candidate_core = core_;',
    '    auto candidate_core = core_;\n    candidate_core.program_candidate_ = true;'),(
    '        candidate_core, &candidate_memory, program, require_matching_background);',
    '        candidate_core, &candidate_memory, program, require_matching_background);\n'
    '    candidate_core.program_candidate_ = false;')]
replacements['quantum_runtime.hpp'].append((
    '  // Restore both authorities transactionally. A bad core or memory snapshot',
    '''  // Typed byte access to the same authorities, scoped to one unpublished
  // transaction. No XI text parsing and no per-word candidate map copy.
  class ByteAccess {
   public:
    ByteAccess(const ByteAccess&) = delete;
    ByteAccess& operator=(const ByteAccess&) = delete;
    InverseMemory::ReadResult read(const std::string& address) const {
      return memory_.read(address, core_);
    }
    InverseMemory::ReadResult read(std::uint64_t address) const {
      return memory_.read_numeric(address, core_);
    }
    void store(const std::string& address, std::uint8_t value) {
      memory_.store(address, value, core_);
    }
    void store(std::uint64_t address, std::uint8_t value) {
      memory_.store_numeric(address, value, core_);
    }
   private:
    friend class QuantumRuntime;
    ByteAccess(InfiniteCore& core, InverseMemory& memory):core_(core),memory_(memory) {}
    InfiniteCore& core_;
    InverseMemory& memory_;
  };
  template<class Action> void transact_bytes(Action action) {
    auto candidate_core = core_;
    candidate_core.program_candidate_ = true;
    auto candidate_memory = memory_;
    ByteAccess bytes(candidate_core, candidate_memory);
    action(bytes); // A failure discards both candidates, including quantum flips.
    candidate_core.program_candidate_ = false;
    core_ = std::move(candidate_core);
    memory_ = std::move(candidate_memory);
  }

  // Restore both authorities transactionally. A bad core or memory snapshot'''))
replacements['exact_t_experiment.hpp'] += [
    ('#include <array>', '#include <array>\n#include <memory>\n#include <mutex>\n#include "integration/state_io.hpp"'),
    ('  std::size_t n_;\n  std::size_t max_components_;', '#include "integration/reduced_state_extensions.inc"\n  std::size_t n_;\n  std::size_t max_components_;'),
    ('    CompactCore next_base = base_;', '    auto next_integration_cache = std::make_shared<IntegrationBasisCache>();\n    CompactCore next_base = base_;'),
    ('    base_ = std::move(next_base);', '    base_ = std::move(next_base);\n    integration_basis_ = std::move(next_integration_cache);'),
]
from ether_integration import configure
configure(expected,replacements)
# E025 retains the larger learned vocabulary and live working memory. Keep the
# upstream checkout pinned and change only this verified, local compatibility copy.
replacements['inverse_memory.hpp'].append((
    '    std::size_t max_overrides = 65536;',
    '    std::size_t max_overrides = 524288;'))
# REVERSE-PC's experimental programmable XI profile is explicitly opted in by
# a first-line marker. Historical XI keeps the original interpreter byte-for-byte.
replacements['infinite_program.hpp'] += [
    ('#include <array>',
     '#include <array>\n#include <map>\n#include <limits>'),
    ('// This function acts on a private candidate owned by InfiniteSession.',
     '#include "integration/xi_machine_extension.inc"\n\n'
     '// This function acts on a private candidate owned by InfiniteSession.'),
    ('  if (program.size() > max_program_bytes)\n'
     '    throw std::length_error("program exceeds 16 MiB transport budget");\n'
     '  ProgramResult result;',
     """  if (program.rfind(machine_header, 0) == 0 &&
      (program.size() == sizeof(machine_header) - 1 ||
       program[sizeof(machine_header) - 1] == '\\n' ||
       program[sizeof(machine_header) - 1] == '\\r'))
    return run_machine(core, memory, program, require_matching_background);
  if (program.size() > max_program_bytes)
    throw std::length_error("program exceeds 16 MiB transport budget");
  ProgramResult result;""")
]
# Reviewed local implementation: the original pinned InverseMemory remains the
# only memory owner. 0 is an explicit opt-in "no override-count quota" value.
# Sparse/dense COW pages replace its private classical index, while old MXIM1
# snapshots remain byte-identical for every bounded legacy profile.
replacements['inverse_memory.hpp'] += [
    ('#include "mundo_x/cow_sharded_map.hpp"',
     '#include "mundo_x/cow_sharded_map.hpp"\n#include "integration/adaptive_override_map.hpp"\n#include "integration/snapshot_view_stream.hpp"'),
    ('    if (overrides_->size() == limits_.max_overrides)',
     '    if (limits_.max_overrides && overrides_->size() == limits_.max_overrides)'),
    ('        if (result.overrides_->size() == limits.max_overrides)',
     '        if (limits.max_overrides && result.overrides_->size() == limits.max_overrides)'),
    ('  std::string snapshot() const {',
     '  std::string snapshot() const {\n    if (!limits_.max_overrides) return adaptive_snapshot();'),
    ('    std::istringstream input(saved);',
     '    if (saved.compare(0,6,"MXIM3 ")==0) return from_adaptive_snapshot(saved,limits);\n'
     '    std::istringstream input(saved);'),
    (' private:\n  using AddressMap = memory_detail::CowShardedMap<std::uint8_t>;',
     ' #include "integration/inverse_memory_adaptive.inc"\n'
     ' private:\n  using AddressMap = memory_detail::AdaptiveOverrideMap;'),
]

# Numeric host stores retain the same InverseMemory authority and serialized
# format. Restricted digit budgets and all quantum views use the original API.
replacements['inverse_memory.hpp'].append((
    '  // A binding is an explicit view, not a global address formula.',
    '  void store_numeric(std::uint64_t address,std::uint8_t value,InfiniteCore& core) {\n    if(limits_.max_address_digits<20||!bindings_->empty()) {store(std::to_string(address),value,core);return;}\n    const auto* old=overrides_->get(address);\n    if(value==background_) {\n      if(!old)return;require_revision();writable_overrides().erase(address);++revision_;return;\n    }\n    if(old) {\n      if(*old==value)return;require_revision();writable_overrides().assign_existing(address,value);++revision_;return;\n    }\n    if(limits_.max_overrides&&overrides_->size()==limits_.max_overrides)throw std::length_error("Inverse Memory classical override budget exceeded");\n    require_revision();writable_overrides().insert(address,value);++revision_;\n  }\n\n  // A binding is an explicit view, not a global address formula.'))

diffs=[]
for name,digest in expected.items():
    raw=(source/name).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=digest:
        raise SystemExit(f'Unreviewed upstream header: {name}')
    original=raw.decode(); revised=original
    for old,new in replacements[name]:
        if revised.count(old)!=1: raise SystemExit(f'Ambiguous compatibility patch: {name}')
        revised=revised.replace(old,new)
    destination=output/'mundo_x'/name
    destination.parent.mkdir(parents=True,exist_ok=True)
    if not destination.exists() or destination.read_text()!=revised: destination.write_text(revised)
    diffs.extend(difflib.unified_diff(original.splitlines(True),revised.splitlines(True),fromfile=f'upstream/{name}',tofile=f'reverse-pc/{name}'))
(output/'changes.diff').write_text(''.join(diffs))
