// REVERSE-PC universality witness. C++17, no Python runtime and no copied
// quantum engine. Uses the exact pinned Mundo X Reduced backend.
#include "mundo_x/exact_t_experiment.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Reduced = mundo_x::ReducedExactTRepresentation;
using Complex = std::complex<double>;
using Clock = std::chrono::steady_clock;
constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double EPSILON = 2e-9;
std::uint64_t checks = 0;

void require(bool value, const std::string& why) {
  ++checks;
  if (!value) throw std::runtime_error(why);
}
void near(double actual, double expected, double eps, const std::string& why) {
  ++checks;
  if (!std::isfinite(actual) || std::abs(actual - expected) > eps)
    throw std::runtime_error(why);
}
double elapsed_ms(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Phase-polynomial identity for Boolean inputs a,b,c:
// 4abc = a+b+c-(a xor b)-(a xor c)-(b xor c)+(a xor b xor c).
// T and T-dagger implement its pi/4 terms. H(target) conjugates
// the resulting CCZ into a phase-EXACT Toffoli. Seven T/Tdagger gates.
void toffoli(Reduced& q, std::size_t a, std::size_t b, std::size_t c) {
  if (a == b || a == c || b == c) throw std::invalid_argument("non-distinct Toffoli wires");
  q.hadamard(c);
  q.phase_t(a);
  q.phase_t(b);
  q.phase_t(c);
  auto minus_parity = [&](std::size_t x, std::size_t y) {
    q.cnot(x, y);
    q.phase_t_dagger(y);
    q.cnot(x, y);
  };
  minus_parity(a, b);
  minus_parity(a, c);
  minus_parity(b, c);
  q.cnot(a, c);
  q.cnot(b, c);
  q.phase_t(c);
  q.cnot(b, c);
  q.cnot(a, c);
  q.hadamard(c);
}

// Phase-independent state comparison to an analytic oracle, NOT to
// the gate-history replay inside the Reduced backend.
double max_error_up_to_global_phase(
    const std::vector<Complex>& actual, const std::vector<Complex>& ideal) {
  if (actual.size() != ideal.size()) throw std::runtime_error("oracle dimension");
  Complex overlap{};
  for (std::size_t i = 0; i < actual.size(); ++i)
    overlap += std::conj(ideal[i]) * actual[i];
  if (std::abs(overlap) < .9) throw std::runtime_error("oracle overlap too small");
  const Complex global = overlap / std::abs(overlap);
  double error = 0.;
  for (std::size_t i = 0; i < actual.size(); ++i)
    error = std::max(error, std::abs(actual[i] - global * ideal[i]));
  return error;
}

struct LogicResult {
  double max_error = 0.;
  std::size_t peak_components = 0;
  std::uint64_t truth_rows = 0, nand_rows = 0;
};

LogicResult logic_witness() {
  LogicResult result;
  // Exact 3-bit Toffoli truth table, 8/8 input rows, including phase.
  for (unsigned input = 0; input < 8; ++input) {
    Reduced q(3, 4096);
    for (unsigned bit = 0; bit < 3; ++bit)
      if ((input >> bit) & 1U) q.pauli_x(bit);
    toffoli(q, 0, 1, 2);
    const unsigned output = input ^ ((input & 3U) == 3U ? 4U : 0U);
    std::vector<Complex> expected(8, Complex{});
    expected[output] = Complex{1., 0.};
    const double err = max_error_up_to_global_phase(q.dense_from_reduced(), expected);
    result.max_error = std::max(result.max_error, err);
    near(err, 0., EPSILON, "Toffoli truth-table amplitude mismatch");
    near(q.sparse_norm(), 1., EPSILON, "Toffoli norm mismatch");
    require(q.t_count() == 7, "Toffoli not synthesized with seven T gates");
    for (unsigned bit = 0; bit < 3; ++bit)
      near(q.pauli_expectation('Z', bit), ((output >> bit) & 1U) ? -1. : 1.,
           EPSILON, "wrong Toffoli output observable");
    result.peak_components = std::max(result.peak_components, q.component_count());
    ++result.truth_rows;
  }
  // With a target ancilla initialized to |1>, Toffoli computes NAND.
  // NAND alone can represent every Boolean function.
  for (unsigned a = 0; a < 2; ++a)
    for (unsigned b = 0; b < 2; ++b) {
      Reduced q(3, 4096);
      if (a) q.pauli_x(0);
      if (b) q.pauli_x(1);
      q.pauli_x(2);
      toffoli(q, 0, 1, 2);
      const unsigned nand = 1U ^ (a & b);
      near(q.pauli_expectation('Z', 2), nand ? -1. : 1.,
           EPSILON, "NAND truth-table failure");
      result.peak_components = std::max(result.peak_components, q.component_count());
      ++result.nand_rows;
    }

  // A superposition is essential: a relative-phase Toffoli could fool
  // basis-only tests. Check a coherent superposition against an analytic
  // 8-amplitude reference, and add an observable non-Clifford T phase.
  Reduced coherent(3, 4096);
  coherent.hadamard(0);
  coherent.hadamard(1);
  toffoli(coherent, 0, 1, 2);
  std::vector<Complex> ideal(8, Complex{});
  for (unsigned index : {0U, 1U, 2U, 7U}) ideal[index] = {.5, 0.};
  double err = max_error_up_to_global_phase(coherent.dense_from_reduced(), ideal);
  result.max_error = std::max(result.max_error, err);
  near(err, 0., EPSILON, "Toffoli introduced relative phase");
  coherent.phase_t(2);
  ideal[7] *= std::polar(1., PI / 4.);
  err = max_error_up_to_global_phase(coherent.dense_from_reduced(), ideal);
  result.max_error = std::max(result.max_error, err);
  near(err, 0., EPSILON, "coherent T phase did not survive Toffoli");
  near(coherent.sparse_norm(), 1., EPSILON, "coherent Toffoli/T norm mismatch");
  require(coherent.t_count() == 8, "coherent test missing non-Clifford operation");
  result.peak_components = std::max(result.peak_components, coherent.component_count());
  return result;
}

struct ThousandResult {
  std::size_t qubits = 0, components = 0, tableau_bytes = 0, payload_bytes = 0;
  double x_before = 0., y_before = 0., zz_far = 0.;
};

ThousandResult thousand_witness() {
  Reduced q(1000, 65536);
  q.hadamard(999);
  q.phase_t(999);
  const double root = 1. / std::sqrt(2.);
  const double x = q.pauli_expectation('X', 999);
  const double y = q.pauli_expectation('Y', 999);
  near(x, root, 1e-11, "q999 non-Clifford X mismatch");
  near(y, root, 1e-11, "q999 non-Clifford Y mismatch");
  q.cnot(999, 0);
  std::vector<std::uint64_t> pauli_x(16), pauli_z(16);
  pauli_z[0] = 1ULL;
  pauli_z[999 / 64] |= (1ULL << (999 % 64));
  const double zz = q.pauli_expectation(pauli_x, pauli_z).real() / q.sparse_norm();
  near(zz, 1., 1e-11, "multiword nonlocal ZZ correlation failed");
  near(q.sparse_norm(), 1., 1e-11, "1000q norm failed");
  require(q.qubits() == 1000, "1000q backend not used");
  require(q.t_count() == 1, "T gate not applied to q999");
  require(q.component_count() <= 4, "unexpected 1000q component growth");
  require(q.base_tableau_bytes() == 257000, "1000q tableau size changed");
  return {q.qubits(), q.component_count(), q.base_tableau_bytes(),
          q.logical_payload_bytes(), x, y, zz};
}

struct Sample {
  unsigned qubits = 0;
  std::size_t components = 0, payload = 0;
  double ms = 0., norm = 0.;
};
std::vector<Sample> resource_profile() {
  std::vector<Sample> samples;
  // Same circuit family at growing widths. This is a resource survey,
  // NOT a claim that arbitrary circuits remain compact.
  for (unsigned n : {2U, 4U, 6U, 8U, 10U}) {
    const auto start = Clock::now();
    Reduced q(n, 65536);
    for (unsigned i = 0; i < n; ++i) q.hadamard(i);
    for (unsigned i = 0; i + 1 < n; ++i) q.cnot(i, i + 1);
    for (unsigned i = 0; i < n; ++i) q.phase_t(i);
    const double norm = q.sparse_norm();
    near(norm, 1., 1e-9, "resource profile norm mismatch");
    require(q.t_count() == n, "resource profile T count changed");
    samples.push_back({n,q.component_count(),q.logical_payload_bytes(),
                       elapsed_ms(start),norm});
  }
  return samples;
}

void print_results(bool include_profile) {
  const auto begin = Clock::now();
  const auto logic = logic_witness();
  const auto thousand = thousand_witness();
  std::vector<Sample> profile;
  if (include_profile) profile = resource_profile();
  std::cout << std::setprecision(16)
            << "{\"suite\":\"reverse_pc_compact_universality\","
            << "\"backend\":\"Mundo X ReducedExactTRepresentation\","
            << "\"runtime\":\"C++17\","
            << "\"classical_hardware\":true,"
            << "\"gate_set_approximately_quantum_universal\":true,"
            << "\"gate_set_mathematically_known\":true,"
            << "\"witness_uses_integrated_xi\":false,"
            << "\"witness_scope\":\"standalone_canonical_Reduced\","
            << "\"arbitrary_circuit_efficiency_proven\":false,"
            << "\"logic\":{\"toffoli_inputs\":" << logic.truth_rows
            << ",\"nand_inputs\":" << logic.nand_rows
            << ",\"t_gates_per_toffoli\":7,"
            << "\"superposition_phase_checked\":true,"
            << "\"max_amplitude_error\":" << logic.max_error
            << ",\"max_components\":" << logic.peak_components << "},"
            << "\"qubits_1000\":{\"qubits\":" << thousand.qubits
            << ",\"components\":" << thousand.components
            << ",\"tableau_bytes\":" << thousand.tableau_bytes
            << ",\"payload_bytes\":" << thousand.payload_bytes
            << ",\"x_before\":" << thousand.x_before
            << ",\"y_before\":" << thousand.y_before
            << ",\"zz_far\":" << thousand.zz_far << "},"
            << "\"checks\":" << checks
            << ",\"elapsed_ms\":" << elapsed_ms(begin);
  if (include_profile) {
    std::cout << ",\"resource_profile\":[";
    for (std::size_t i = 0; i < profile.size(); ++i) {
      const auto& s = profile[i];
      if (i) std::cout << ",";
      std::cout << "{\"qubits\":" << s.qubits
                << ",\"T_gates\":" << s.qubits
                << ",\"components\":" << s.components
                << ",\"payload_bytes\":" << s.payload
                << ",\"ms\":" << s.ms
                << ",\"norm\":" << s.norm << "}";
    }
    std::cout << "]";
  }
  std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 2 || (std::string(argv[1]) != "--self-test" &&
                      std::string(argv[1]) != "--profile"))
      throw std::invalid_argument("usage: reverse_pc_universality --self-test|--profile");
    print_results(std::string(argv[1]) == "--profile");
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "REVERSE_PC_UNIVERSALITY_FAIL " << e.what() << "\n";
    return 1;
  }
}
