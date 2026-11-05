#include "integration/born_sampling.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using Word = std::uint64_t;
void need(bool condition, const char* why) {
  if (!condition) throw std::runtime_error(why);
}
struct Scripted {
  std::vector<Word> words;
  std::size_t calls = 0;
  Word operator()() {
    if (calls >= words.size())
      throw std::runtime_error("scripted RNG exhausted");
    return words[calls++];
  }
};
Scripted first_one(unsigned bit) {
  Scripted out{std::vector<Word>(17)};
  out.words[(bit - 1) / 64] |= Word{1} << (63 - (bit - 1) % 64);
  return out;
}
void self_test() {
  std::size_t checks = 0, power_checks = 0;
  for (unsigned bits = 1; bits <= 8; ++bits) {
    unsigned denominator = 1u << bits;
    for (unsigned k = 0; k <= denominator; ++k) {
      unsigned ones = 0;
      const double p = double(k) / denominator;
      for (unsigned prefix = 0; prefix < denominator; ++prefix) {
        const Word high = Word{prefix} << (64 - bits);
        for (Word suffix : {Word{0}, (Word{1} << (64 - bits)) - 1}) {
          Scripted source{{high | suffix}};
          need(reverse_pc::binary_bernoulli(source, p) == (prefix < k),
               "dyadic threshold mismatch");
          ++checks;
        }
        // The historical 1/2 path consumes the LSB; make it uniform too.
        Scripted source{{high | (prefix & 1u)}};
        ones += reverse_pc::sample_born_one(source, p, 1. - p);
      }
      need(ones == k, "normalized Born pair has wrong dyadic frequency");
    }
  }
  for (unsigned exponent = 1; exponent <= 1074; ++exponent) {
    const auto p = std::ldexp(1., -int(exponent));
    for (unsigned bit : {exponent - 1, exponent, exponent + 1}) {
      if (!bit) continue;
      auto source = first_one(bit);
      need(reverse_pc::binary_bernoulli(source, p) == (bit > exponent),
           "power-of-two/subnormal threshold mismatch");
      need(source.calls <= 17, "unbounded Bernoulli sampling");
      ++power_checks;
    }
  }
  auto rare = Scripted{{0, ~Word{0}}};
  need(!reverse_pc::binary_bernoulli(rare, 1e-300) && rare.calls == 2,
       "rare tail rounded onto a 53-bit grid");
  const bool old_53_bit_result = (double(Word{0} >> 11) * 0x1.0p-53) < 1e-300;
  need(old_53_bit_result, "coarse-sampler regression case changed");
  const auto minimum = std::numeric_limits<double>::denorm_min();
  Scripted all_zero{std::vector<Word>(17)};
  need(reverse_pc::binary_bernoulli(all_zero, minimum) && all_zero.calls == 17,
       "minimum subnormal branch lost");
  all_zero.calls = 0;
  need(!reverse_pc::sample_born_one(all_zero, 1., minimum) &&
           all_zero.calls == 17,
       "near-one branch lost its separately computed rare complement");
  const auto almost_one = std::nextafter(1., 0.);
  const auto boundary = std::numeric_limits<Word>::max() - 2047;
  for (const Word word : {boundary - 1, boundary, boundary + 1,
                          std::numeric_limits<Word>::max()}) {
    Scripted edge{{word}};
    need(reverse_pc::binary_bernoulli(edge, almost_one) == (word < boundary),
         "largest subunit double threshold/cast overflow");
  }
  std::mt19937_64 source(20261025), reference(20261025);
  for (unsigned i = 0; i < 10000; ++i)
    need(reverse_pc::sample_born_one(source, .5, .5) ==
             ((reference() & 1u) != 0),
         "historical half-probability stream changed");
  need(source == reference, "historical RNG consumption changed");
  for (double bad : {-1., 1.1, std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()}) {
    Scripted unused{{0}};
    bool rejected = false;
    try {
      reverse_pc::binary_bernoulli(unused, bad);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    need(rejected && unused.calls == 0, "invalid probability consumed RNG");
  }
  Scripted unused{{0}};
  need(!reverse_pc::binary_bernoulli(unused, 0.) &&
           reverse_pc::binary_bernoulli(unused, 1.) && unused.calls == 0,
       "deterministic probability consumed RNG");
  std::cout
      << "{\"suite\":\"binary_Born_sampling\",\"dyadic_threshold_checks\":"
      << checks << ",\"power_and_subnormal_checks\":" << power_checks
      << ",\"maximum_words\":17,\"legacy_half_draws_checked\":10000,"
         "\"rare_53_bit_regression_fixed\":true}\n";
}
struct CounterSource {
  std::mt19937_64 generator{20261025};
  std::size_t calls = 0;
  Word operator()() {
    ++calls;
    return generator();
  }
};
bool previous(CounterSource& source, double one, double zero) {
  if (one == .5 && zero == .5) return (source() & 1u) != 0;
  const auto u = double(source() >> 11) * 0x1.0p-53;
  return one > .5 ? !(u < zero) : u < one;
}
void benchmark() {
  std::cout
      << std::setprecision(16)
      << "{\"suite\":\"Born_sampler_comparison\",\"source\":\"mt19937_64\","
         "\"draws_per_sample\":1000000,\"repeats\":7,\"cases\":[";
  const double p = (1. - 1. / std::sqrt(2.)) * .5;
  unsigned index = 0;
  for (const auto pair : {std::pair<double, double>{.5, .5},
                          {p, 1. - p},
                          {1e-300, 1.},
                          {1., 1e-300}}) {
    if (index++) std::cout << ',';
    std::vector<double> samples[2];
    std::size_t checksums[2]{}, calls[2]{};
    auto run = [&](unsigned mode) {
      CounterSource source;
      std::size_t ones = 0;
      const auto begin = std::chrono::steady_clock::now();
      for (unsigned i = 0; i < 1000000; ++i)
        ones +=
            mode ? reverse_pc::sample_born_one(source, pair.first, pair.second)
                 : previous(source, pair.first, pair.second);
      const double ns = std::chrono::duration<double, std::nano>(
                            std::chrono::steady_clock::now() - begin)
                            .count() /
                        1000000;
      checksums[mode] = ones;
      calls[mode] = source.calls;
      return ns;
    };
    run(0);
    run(1);
    for (unsigned i = 0; i < 7; ++i)
      for (unsigned j = 0; j < 2; ++j) {
        const auto mode = (i + j) % 2;
        samples[mode].push_back(run(mode));
      }
    std::cout << "{\"one\":" << pair.first << ",\"zero\":" << pair.second;
    for (unsigned mode = 0; mode < 2; ++mode) {
      auto sorted = samples[mode];
      std::sort(sorted.begin(), sorted.end());
      std::cout << ",\"" << (mode ? "candidate" : "previous")
                << "\":{\"median_ns\":" << sorted[3]
                << ",\"p95_ns\":" << sorted.back()
                << ",\"ones\":" << checksums[mode]
                << ",\"rng_words\":" << calls[mode] << ",\"samples_ns\":[";
      for (unsigned i = 0; i < samples[mode].size(); ++i) {
        if (i) std::cout << ',';
        std::cout << samples[mode][i];
      }
      std::cout << "]}";
    }
    std::cout << '}';
  }
  std::ifstream status("/proc/self/status");
  std::string key;
  unsigned rss = 0, peak = 0;
  while (status >> key) {
    if (key == "VmRSS:") status >> rss;
    if (key == "VmHWM:") status >> peak;
    std::string rest;
    std::getline(status, rest);
  }
  std::cout << "],\"rss_kib\":" << rss << ",\"peak_rss_kib\":" << peak << "}\n";
}
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--benchmark")
      benchmark();
    else
      self_test();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
