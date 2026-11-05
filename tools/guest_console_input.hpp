#pragma once
// A bounded host input event for the existing modeled PL011 receive queue.
#include "guest_file_session.hpp"

namespace reverse_pc::guest {
inline Bytes guest_console_input_file(const char* path) {
  if (!path) return {};
  require(*path != '\0', "Missing console input file path");
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  require(bool(input), "Cannot open console input file");
  const auto size = input.tellg();
  require(size >= 0 && std::uint64_t(size) <= 4096,
          "Console input file exceeds 4096 bytes");
  Bytes bytes(static_cast<std::size_t>(size));
  input.seekg(0);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
    require(bool(input), "Cannot read complete console input file");
  }
  require(input.peek() == std::char_traits<char>::eof(), "Console input file grew while reading");
  return bytes;
}

inline std::string guest_console_input_event(MinimalGuest& guest, const Bytes& bytes) {
  if (bytes.empty()) return {};
  require(bytes.size() <= 4096, "Console input event exceeds 4096 bytes");
  const auto count = guest.registers().instructions;
  const auto phase = guest.machine().counter_latency.subtick;
  const auto sha = GuestSha256::hash(bytes);
  guest.console_input(std::string(bytes.begin(), bytes.end()));
  return ",\"external_uart_input\":{\"schema\":\"reverse-modeled-uart-input-v1\","
         "\"instructions\":" + std::to_string(count) +
         ",\"counter_subtick\":" + std::to_string(phase) +
         ",\"bytes\":" + std::to_string(bytes.size()) +
         ",\"sha256\":" + quote(sha) + "}";
}
} // namespace reverse_pc::guest
