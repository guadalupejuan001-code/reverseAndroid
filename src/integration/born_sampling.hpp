#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace reverse_pc {
// Compare a lazy uniform binary expansion to the exact binary value of a
// double, 64 bits at a time. The source must supply uniform 64-bit words.
// Scaling by 2^64 is exact. If the integer part has >53 bits the scaled double
// is already an integer; otherwise both its floor and remainder are exact.
// Equal prefixes need another word, rather than quantizing the probability.
template <class WordSource>
bool binary_bernoulli(WordSource& source, double probability) {
  static_assert(std::numeric_limits<double>::is_iec559 &&
                std::numeric_limits<double>::digits == 53 &&
                std::numeric_limits<double>::min_exponent == -1021);
  if (!std::isfinite(probability) || probability < 0. || probability > 1.)
    throw std::invalid_argument("Bernoulli probability outside [0,1]");
  if (probability == 0. || probability == 1.) return probability == 1.;
  constexpr int max_words = (53 + 1021 + 63) / 64;
  for (int i = 0; i < max_words; ++i) {
    const double scaled = probability * 0x1.0p64;
    // 0 <= scaled < 2^64: this conversion cannot overflow.
    const auto threshold = static_cast<std::uint64_t>(scaled);
    const std::uint64_t word = source();
    if (word != threshold) return word < threshold;
    probability = scaled - static_cast<double>(threshold);
    // A matched finite threshold is U >= p, including equality.
    if (probability == 0.) return false;
  }
  throw std::logic_error("double probability expansion did not terminate");
}

// The projector weights are computed separately: never form 1-p when that
// could round away the rarer branch. Keep the historical half-probability RNG.
template <class WordSource>
bool sample_born_one(WordSource& source, double one, double zero) {
  if (one == .5 && zero == .5) return (source() & 1U) != 0;
  if (!std::isfinite(one) || !std::isfinite(zero) || one < 0. || zero < 0. ||
      one > 1. || zero > 1. || (one == 0. && zero == 0.))
    throw std::invalid_argument("invalid Born probability pair");
  return one > .5 ? !binary_bernoulli(source, zero)
                  : binary_bernoulli(source, one);
}
}  // namespace reverse_pc
