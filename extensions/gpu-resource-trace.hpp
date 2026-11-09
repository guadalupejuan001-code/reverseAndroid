#pragma once
// Host-only observation. Events inside a run() candidate are provisional until
// the native run completes and its original RVG admission succeeds.
#include <atomic>
#include <cstdio>
#include <cstdint>

namespace reverse_pc::guest {
template<class Buffer, class Pci>
inline void gpu_resource_trace(const char* phase, std::uint64_t instructions,
                               unsigned queue, unsigned type, const Buffer& input,
                               const Pci& pci, unsigned response = 0) noexcept {
  static std::atomic<std::uint64_t> sequence{0};
  const auto seq = sequence.fetch_add(1, std::memory_order_relaxed);
  constexpr std::uint64_t capacity = 16384;
  if (seq > capacity) return;
  if (seq == capacity) {
    std::fputs("REVERSE_GPU_TRACE {\"event\":\"trace_limit\",\"capacity\":16384}\n", stderr);
    return;
  }
  unsigned offset = 0;
  if (type == 0x101 || type == 0x102 || type == 0x106 || type == 0x107) offset = 24;
  if (type == 0x103) offset = 44;
  if (type == 0x104 || type == 0x300) offset = 40;
  if (type == 0x105) offset = 48;
  const bool present = offset && input.size() >= offset + 4;
  std::uint32_t id = 0;
  if (present) for (unsigned i = 0; i < 4; ++i) id |= std::uint32_t(input[offset+i]) << (8*i);
  // Observe the declared device profile without assigning or recreating IDs.
  const auto& r = pci.resources;
  std::fprintf(stderr,
    "REVERSE_GPU_TRACE {\"event\":\"%s\",\"candidate\":true,\"sequence\":%llu,"
    "\"instructions\":%llu,\"queue\":%u,\"command\":%u,\"resource_id_present\":%s,"
    "\"resource_id\":%u,\"response\":%u,\"scanout\":%u,\"flushes\":%llu,"
    "\"resource_ids\":[",
    phase, static_cast<unsigned long long>(seq), static_cast<unsigned long long>(instructions),
    queue, type, present ? "true" : "false", id, response, pci.scanout,
    static_cast<unsigned long long>(pci.flushes));
  for(unsigned i=0;i<r.size();++i)std::fprintf(stderr,"%s%u",i?",":"",r[i].id);
  std::fputs("],\"resource_extents\":[",stderr);
  for(unsigned i=0;i<r.size();++i)std::fprintf(stderr,"%s[%u,%u]",i?",":"",r[i].width,r[i].height);
  std::fputs("]}\n",stderr);
}
} // namespace reverse_pc::guest
