#pragma once
// Host-side evidence binding only. This header neither owns nor executes guest state.
#include <cstdint>
#include <stdexcept>
#include <string>

namespace reverse_pc::guest {
struct BootCycleEntryProof {
  std::string checkpoint_sha256;
  std::uint64_t instructions=0,pc=0;
  // Defaults preserve existing C06 / EL0 aggregate initializers.
  unsigned exception_level=0;
  std::string original_entry_checkpoint_sha256;
};

inline bool boot_cycle_valid_sha256(const std::string& sha){
  return sha.size()==64&&sha.find_first_not_of("0123456789abcdef")==std::string::npos;
}

// The caller must first validate the source's independently reviewed provenance.
// Bind that proof to the complete restored snapshot and exact architectural boundary.
// Return the original entry root, NOT the newer continuation's snapshot identity.
inline std::string boot_cycle_bound_entry_root(const BootCycleEntryProof& proof,
    const std::string& actual_sha,std::uint64_t instructions,std::uint64_t pc,
    bool enabled,unsigned exception_level,bool halted,bool waiting){
  const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
  check(boot_cycle_valid_sha256(proof.checkpoint_sha256),"Invalid proven entry hash");
  check(enabled&&!halted&&!waiting,"Proven continuation must be an active state");
  check(proof.exception_level<=1&&exception_level==proof.exception_level,
        "Proven continuation exception level differs");
  check(instructions==proof.instructions&&pc==proof.pc,"Proven entry count or PC differs");
  check(actual_sha==proof.checkpoint_sha256,"Proven entry does not match the complete native state");
  check(proof.exception_level==0||!proof.original_entry_checkpoint_sha256.empty(),
        "An EL1 continuation requires its original verified EL0 entry root");
  const auto& root=proof.original_entry_checkpoint_sha256.empty()
      ?proof.checkpoint_sha256:proof.original_entry_checkpoint_sha256;
  check(boot_cycle_valid_sha256(root),"Invalid original entry root");
  return root;
}
} // namespace reverse_pc::guest
