// Runnable and measurable factorization experiment for REVERSE-PC.
// The gate stream is identical for adaptive and unmodified Mundo X Reduced.
#include "adaptive_reduced.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using reverse_pc::AdaptiveReduced;
using reverse_pc::Complex;
using reverse_pc::Gate;
using reverse_pc::Operation;
using reverse_pc::Reduced;
using Clock = std::chrono::steady_clock;
constexpr double kPI = 3.14159265358979323846;
constexpr double kTol = 2e-8;
using Vec = std::vector<Complex>;
using Ops = std::vector<Gate>;

void ensure(bool ok, const std::string& reason) {
  if (!ok) throw std::runtime_error(reason);
}
double milliseconds(Clock::time_point begin) {
  return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
}

void dense_gate(Vec& v, Gate g) {
  const auto a = std::size_t{1} << g.a;
  const auto b = std::size_t{1} << g.b;
  const double inv = 1. / std::sqrt(2.);
  switch (g.operation) {
    case Operation::H:
      for (std::size_t i = 0; i < v.size(); ++i) {
        if (i & a) continue;
        auto left = v[i], right = v[i | a];
        v[i] = (left + right) * inv;
        v[i | a] = (left - right) * inv;
      }
      break;
    case Operation::X:
      for (std::size_t i = 0; i < v.size(); ++i)
        if (!(i & a)) std::swap(v[i], v[i | a]);
      break;
    case Operation::Z:
    case Operation::S:
    case Operation::T:
    case Operation::TDag: {
      Complex phase = 1.;
      if (g.operation == Operation::Z) phase = -1.;
      else if (g.operation == Operation::S) phase = Complex{0., 1.};
      else if (g.operation == Operation::T) phase = std::polar(1., kPI / 4.);
      else phase = std::polar(1., -kPI / 4.);
      for (std::size_t i = 0; i < v.size(); ++i) if (i & a) v[i] *= phase;
      break;
    }
    case Operation::CX:
      for (std::size_t i = 0; i < v.size(); ++i)
        if ((i & a) && !(i & b)) std::swap(v[i], v[i | b]);
      break;
    case Operation::CZ:
      for (std::size_t i = 0; i < v.size(); ++i)
        if ((i & a) && (i & b)) v[i] = -v[i];
      break;
  }
}
void reduced_gate(Reduced& q, Gate g) {
  switch (g.operation) {
    case Operation::H: q.hadamard(g.a); break;
    case Operation::X: q.pauli_x(g.a); break;
    case Operation::Z: q.pauli_z(g.a); break;
    case Operation::S: q.phase_s(g.a); break;
    case Operation::T: q.phase_t(g.a); break;
    case Operation::TDag: q.phase_t_dagger(g.a); break;
    case Operation::CX: q.cnot(g.a, g.b); break;
    case Operation::CZ: q.controlled_z(g.a, g.b); break;
  }
}

double state_error(const Vec& actual, const Vec& ideal) {
  ensure(actual.size() == ideal.size(), "dense state dimensions disagree");
  Complex overlap{};
  for (std::size_t i = 0; i < actual.size(); ++i)
    overlap += std::conj(ideal[i]) * actual[i];
  ensure(std::abs(overlap) > .2, "dense states nearly orthogonal");
  const auto phase = overlap / std::abs(overlap);
  double maxerror = 0.;
  for (std::size_t i = 0; i < actual.size(); ++i)
    maxerror = std::max(maxerror, std::abs(actual[i] - phase * ideal[i]));
  return maxerror;
}

Ops family(unsigned n) {
  Ops result;
  result.reserve(3 * n);
  for (unsigned i = 0; i < n; ++i) result.push_back({Operation::H, i});
  for (unsigned i = 0; i + 1 < n; ++i)
    result.push_back({Operation::CX, i, i + 1});
  for (unsigned i = 0; i < n; ++i) result.push_back({Operation::T, i});
  return result;
}

struct Results { double max_error = 0.; unsigned fallback = 0, block_joins = 0; };
Results self_test() {
  std::mt19937 rng(20261025);
  Results result;
  for (unsigned trial = 0; trial < 260; ++trial) {
    const unsigned n = 2 + trial % 5;
    const unsigned block_limit = 1 + trial % 4;
    AdaptiveReduced q(n, block_limit);
    Vec ideal(std::size_t{1} << n, Complex{});
    ideal[0] = 1.;
    const unsigned steps = 18 + trial % 17;
    for (unsigned i = 0; i < steps; ++i) {
      const auto a = static_cast<unsigned>(rng() % n);
      const auto b = static_cast<unsigned>((a + 1 + rng() % (n - 1)) % n);
      const auto op = static_cast<Operation>(rng() % 8);
      Gate g{op, a, b};
      q.apply(g.operation, g.a, g.b);
      dense_gate(ideal, g);
      if (q.uses_original_reduced()) ++result.fallback;
      if (q.max_block_width() > 1) ++result.block_joins;
    }
    const double delta = state_error(q.dense(), ideal);
    result.max_error = std::max(result.max_error, delta);
    ensure(delta < kTol, "random differential mismatch trial " + std::to_string(trial));
    ensure(std::abs(q.norm() - 1.) < kTol, "random normalization failed");
    for (unsigned bit = 0; bit < n; ++bit) {
      double z = 0.;
      for (std::size_t index = 0; index < ideal.size(); ++index)
        z += std::norm(ideal[index]) * ((index >> bit) & 1u ? -1. : 1.);
      ensure(std::abs(q.expectation('Z', bit) - z) < kTol,
             "random Z observable mismatch");
    }
  }
  ensure(result.fallback > 0, "fallback never exercised");
  ensure(result.block_joins > 0, "block joining never exercised");

  // A cross-block entangling gate must NOT be incorrectly suppressed.
  AdaptiveReduced entangled(2, 2);
  entangled.apply(Operation::H, 0);
  entangled.apply(Operation::CX, 0, 1);
  ensure(entangled.block_count() == 1, "Bell pair was incorrectly factorized");
  ensure(std::abs(entangled.expectation('Z', 0)) < kTol, "Bell local Z");
  ensure(std::abs(entangled.expectation('Z', 1)) < kTol, "Bell local Z");

  // A tiny block cap forces canonical Reduced replay on a valid circuit.
  AdaptiveReduced forced(3, 1);
  forced.apply(Operation::H, 0);
  forced.apply(Operation::CX, 0, 1);
  ensure(forced.uses_original_reduced(), "small block cap did not trigger replay");

  // 1000 independent T phases, with 999 intervening CX(+,+), should remain
  // a compact product. This test cannot construct a 2^1000 dense oracle.
  AdaptiveReduced thousand(1000);
  for (unsigned q = 0; q < 1000; ++q) thousand.apply(Operation::H, q);
  for (unsigned q = 0; q < 999; ++q) thousand.apply(Operation::CX, q, q+1);
  for (unsigned q = 0; q < 1000; ++q) thousand.apply(Operation::T, q);
  ensure(!thousand.uses_original_reduced(), "1000-qubit case did not stay compact");
  ensure(thousand.block_count() == 1000, "1000-qubit unexpected entanglement");
  ensure(thousand.factor_amplitudes() == 2000, "1000-qubit factor size wrong");
  ensure(std::abs(thousand.norm() - 1.) < 1e-9, "1000-qubit normalization failed");
  ensure(std::abs(thousand.expectation('X', 999) - 1. / std::sqrt(2.)) < kTol,
         "1000-qubit phase observable failed");

  // Out-of-range gates must leave the state unchanged.
  AdaptiveReduced bounded(3);
  bounded.apply(Operation::H, 0);
  auto saved = bounded.dense();
  bool rejected = false;
  try { bounded.apply(Operation::CX, 0, 3); }
  catch (const std::out_of_range&) { rejected = true; }
  ensure(rejected && state_error(bounded.dense(), saved) < kTol,
         "invalid gate changed state");

  return result;
}

struct Perf {
  unsigned n;
  std::size_t reduced_components, reduced_bytes;
  std::size_t factor_blocks, factor_amplitudes, factor_bytes;
  double reduced_ms, factor_ms, speedup, max_error;
};

// Timed portions deliberately exclude dense reconstruction and state checking.
Perf measure(unsigned n) {
  const auto program = family(n);
  constexpr unsigned repeat = 5;
  std::vector<double> baseline_times, optimized_times;
  std::size_t baseline_components = 0, baseline_bytes = 0;
  std::size_t blocks = 0, amplitudes = 0, optimized_bytes = 0;
  Vec original, factor;

  for (unsigned iteration = 0; iteration < repeat; ++iteration) {
    const auto start = Clock::now();
    Reduced q(n, 65536);
    for (const auto& g : program) reduced_gate(q, g);
    baseline_times.push_back(milliseconds(start));
    baseline_components = q.component_count();
    baseline_bytes = q.logical_payload_bytes();
    if (iteration == 0) original = q.dense_from_reduced();

    const auto faster = Clock::now();
    AdaptiveReduced adaptive(n);
    for (const auto& g : program) adaptive.apply(g.operation, g.a, g.b);
    optimized_times.push_back(milliseconds(faster));
    ensure(!adaptive.uses_original_reduced(),
           "structured circuit should never materialize original Reduced");
    blocks = adaptive.block_count();
    amplitudes = adaptive.factor_amplitudes();
    optimized_bytes = adaptive.estimated_payload_bytes();
    if (iteration == 0) factor = adaptive.dense();
  }
  std::sort(baseline_times.begin(), baseline_times.end());
  std::sort(optimized_times.begin(), optimized_times.end());
  const double r = baseline_times[repeat/2], f = optimized_times[repeat/2];
  const double err = state_error(factor, original);
  ensure(err < kTol, "comparison to canonical Reduced failed");
  ensure(amplitudes == 2*n, "family did not stay product factorized");
  ensure(baseline_components == (std::size_t{1} << n),
         "baseline component count changed; this is not an apples-to-apples case");
  return {n, baseline_components, baseline_bytes, blocks, amplitudes, optimized_bytes,
          r, f, r/f, err};
}

Operation decode(const std::string& name) {
  if (name == "h") return Operation::H;
  if (name == "x") return Operation::X;
  if (name == "z") return Operation::Z;
  if (name == "s") return Operation::S;
  if (name == "t") return Operation::T;
  if (name == "tdg") return Operation::TDag;
  if (name == "cx") return Operation::CX;
  if (name == "cz") return Operation::CZ;
  throw std::invalid_argument("unsupported instruction: " + name);
}

void run_file(std::size_t n, const std::string& filename) {
  std::ifstream input(filename);
  if (!input) throw std::runtime_error("unable to open program: " + filename);
  AdaptiveReduced state(n);
  std::string line;
  std::size_t line_number = 0;
  constexpr std::size_t max_gates = 1000000;
  const auto begin = Clock::now();
  while (std::getline(input, line)) {
    ++line_number;
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') continue;
    std::istringstream in(line);
    std::string name, extra;
    std::size_t a = 0, b = 0;
    in >> name;
    const auto op = decode(name);
    if (!(in >> a)) throw std::runtime_error("missing operand on line " + std::to_string(line_number));
    if ((op == Operation::CX || op == Operation::CZ) && !(in >> b))
      throw std::runtime_error("missing second operand on line " + std::to_string(line_number));
    if (in >> extra) throw std::runtime_error("unexpected operand on line " + std::to_string(line_number));
    if (state.gate_count() >= max_gates) throw std::runtime_error("program too long");
    state.apply(op, a, b);
  }
  std::cout << std::setprecision(15)
            << "{\"mode\":\"run\",\"qubits\":" << n
            << ",\"gates\":" << state.gate_count()
            << ",\"backend\":\"" << (state.uses_original_reduced() ? "canonical_reduced" : "factorized_blocks")
            << "\",\"blocks\":" << state.block_count()
            << ",\"factor_amplitudes\":" << state.factor_amplitudes()
            << ",\"components\":" << state.component_count()
            << ",\"payload_bytes_estimate\":" << state.estimated_payload_bytes()
            << ",\"norm\":" << state.norm()
            << ",\"z0\":" << state.expectation('Z', 0)
            << ",\"ms\":" << milliseconds(begin) << "}\n";
}

void benchmark() {
  std::cout << std::setprecision(15)
            << "{\"suite\":\"reverse_pc_adaptive_factorization\","
            << "\"reference\":\"pinned Mundo X Reduced\","
            << "\"timing\":\"5 repetitions, median, no dense reconstruction\","
            << "\"payload\":\"estimated container bytes, not resident RAM\","
            << "\"cases\":[";
  bool first = true;
  for (unsigned n : {8u, 10u, 12u, 14u, 16u}) {
    const auto p = measure(n);
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"qubits\":" << p.n
              << ",\"reduced_components\":" << p.reduced_components
              << ",\"reduced_payload_bytes\":" << p.reduced_bytes
              << ",\"factor_blocks\":" << p.factor_blocks
              << ",\"factor_amplitudes\":" << p.factor_amplitudes
              << ",\"factor_payload_bytes\":" << p.factor_bytes
              << ",\"reduced_ms\":" << p.reduced_ms
              << ",\"factor_ms\":" << p.factor_ms
              << ",\"time_ratio\":" << p.speedup
              << ",\"max_amplitude_error\":" << p.max_error
              << "}";
  }
  const auto large_start = Clock::now();
  AdaptiveReduced large(1000);
  for (const auto& g : family(1000)) large.apply(g.operation, g.a, g.b);
  const double large_ms = milliseconds(large_start);
  ensure(!large.uses_original_reduced() && large.block_count() == 1000
         && large.factor_amplitudes() == 2000, "1000q benchmark was not compact");
  ensure(std::abs(large.norm() - 1.) < 1e-9, "1000q product lost normalization");
  std::cout << "],\"large_product\":{\"qubits\":1000"
            << ",\"factor_blocks\":" << large.block_count()
            << ",\"factor_amplitudes\":" << large.factor_amplitudes()
            << ",\"factor_payload_bytes\":" << large.estimated_payload_bytes()
            << ",\"ms\":" << large_ms
            << ",\"baseline_not_run\":true"
            << "}}\n";

}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--self-test") {
      const auto r = self_test();
      std::cout << std::setprecision(15)
                << "{\"suite\":\"reverse_pc_factorized_differential\","
                << "\"random_circuits\":260,\"seed\":20261025,"
                << "\"fallback_observations\":" << r.fallback
                << ",\"block_join_observations\":" << r.block_joins
                << ",\"max_amplitude_error\":" << r.max_error
                << ",\"thousand_qubits_product_test\":true}\n";
    } else if (argc == 2 && std::string(argv[1]) == "--benchmark") {
      benchmark();
    } else if (argc == 4 && std::string(argv[1]) == "--run") {
      const auto n = std::stoull(argv[2]);
      run_file(n, argv[3]);
    } else {
      throw std::invalid_argument("usage: reverse_pc_factorized --self-test|--benchmark|--run QUBITS FILE");
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "REVERSE_PC_FACTORIZATION_FAIL " << e.what() << '\n';
    return 1;
  }
}
