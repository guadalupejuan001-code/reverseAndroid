#pragma once
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
namespace reverse_pc::state_io {
inline std::uint64_t hash(const std::string& s, std::size_t n) {
  std::uint64_t h = 14695981039346656037ULL;
  for (std::size_t i = 0; i < n; ++i) {
    h ^= static_cast<unsigned char>(s[i]);
    h *= 1099511628211ULL;
  }
  return h;
}
struct Writer {
  std::string data;
  void byte(unsigned v) { data.push_back(static_cast<char>(v)); }
  void u64(std::uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) {
      byte(v & 255);
      v >>= 8;
    }
  }
  void real(double v) {
    static_assert(sizeof(double) == 8 &&
                  std::numeric_limits<double>::is_iec559);
    std::uint64_t bits;
    std::memcpy(&bits, &v, 8);
    u64(bits);
  }
  void complex(std::complex<double> v) {
    real(v.real());
    real(v.imag());
  }
  void string(const std::string& v) {
    u64(v.size());
    data += v;
  }
  std::string finish() {
    u64(hash(data, data.size()));
    return std::move(data);
  }
};
struct Reader {
  const std::string& data;
  std::size_t at = 0, end;
  explicit Reader(const std::string& s, std::size_t cap = 64 * 1024 * 1024)
      : data(s), end(s.size()) {
    if (s.size() < 8 || s.size() > cap)
      throw std::length_error("snapshot transport budget/truncation");
    end -= 8;
    std::uint64_t digest = 0;
    for (unsigned i = 0; i < 8; ++i)
      digest |= std::uint64_t(static_cast<unsigned char>(s[end + i]))
                << (8 * i);
    if (hash(s, end) != digest)
      throw std::invalid_argument("snapshot checksum");
  }
  void require(std::size_t n) const {
    if (n > end - at) throw std::invalid_argument("truncated state payload");
  }
  unsigned byte() {
    require(1);
    return static_cast<unsigned char>(data[at++]);
  }
  std::uint64_t u64() {
    std::uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= std::uint64_t(byte()) << (8 * i);
    return v;
  }
  double real() {
    auto bits = u64();
    double v;
    std::memcpy(&v, &bits, 8);
    if (!std::isfinite(v))
      throw std::invalid_argument("nonfinite state coefficient");
    return v;
  }
  std::complex<double> complex() {
    double a = real(), b = real();
    return {a, b};
  }
  std::string string(std::size_t cap) {
    auto n = u64();
    if (n > cap) throw std::length_error("snapshot field budget");
    require(n);
    auto v = data.substr(at, n);
    at += n;
    return v;
  }
  void done() const {
    if (at != end) throw std::invalid_argument("trailing state payload");
  }
};
}  // namespace reverse_pc::state_io
