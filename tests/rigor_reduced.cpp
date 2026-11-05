// Independent, deterministic differential and stress testing for REVERSE-PC.
// Runs against the pinned Mundo X Reduced backend; no code from that backend is copied.
#include "mundo_x/exact_t_experiment.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using Q = mundo_x::ReducedExactTRepresentation;
using C = std::complex<double>;
using Clock = std::chrono::steady_clock;
constexpr double PI = 3.14159265358979323846;
constexpr double TOL = 2e-8;

struct Gate { char op; unsigned a, b; };
void need(bool good, const std::string& message) {
  if (!good) throw std::runtime_error(message);
}

double error_phase(const std::vector<C>& actual, const std::vector<C>& ideal) {
  need(actual.size() == ideal.size(), "dense dimension mismatch");
  C overlap{};
  for (std::size_t i = 0; i < actual.size(); ++i)
    overlap += std::conj(ideal[i]) * actual[i];
  need(std::abs(overlap) > .5, "state overlap too small");
  const C phase = overlap / std::abs(overlap);
  double error = 0;
  for (std::size_t i = 0; i < actual.size(); ++i)
    error = std::max(error, std::abs(actual[i] - phase * ideal[i]));
  return error;
}

void apply_dense(std::vector<C>& state, const Gate& gate) {
  const std::size_t a = std::size_t{1} << gate.a;
  const std::size_t b = std::size_t{1} << gate.b;
  if (gate.op == 'H') {
    const double inv = 1 / std::sqrt(2.);
    for (std::size_t i = 0; i < state.size(); ++i) {
      if ((i & a) != 0) continue;
      const C x = state[i], y = state[i | a];
      state[i] = (x + y) * inv;
      state[i | a] = (x - y) * inv;
    }
  } else if (gate.op == 'X') {
    for (std::size_t i = 0; i < state.size(); ++i)
      if (!(i & a)) std::swap(state[i], state[i | a]);
  } else if (gate.op == 'T' || gate.op == 'D') {
    const C phase = std::polar(1., (gate.op == 'T' ? 1. : -1.) * PI / 4.);
    for (std::size_t i = 0; i < state.size(); ++i)
      if (i & a) state[i] *= phase;
  } else if (gate.op == 'C') {
    for (std::size_t i = 0; i < state.size(); ++i)
      if ((i & a) && !(i & b)) std::swap(state[i], state[i | b]);
  } else throw std::runtime_error("unknown oracle gate");
}

void apply(Q& q, const Gate& g) {
  switch(g.op) {
    case 'H': q.hadamard(g.a); break;
    case 'X': q.pauli_x(g.a); break;
    case 'T': q.phase_t(g.a); break;
    case 'D': q.phase_t_dagger(g.a); break;
    case 'C': q.cnot(g.a, g.b); break;
    default: throw std::runtime_error("unknown backend gate");
  }
}

Gate inverted(Gate g) {
  if (g.op == 'T') g.op = 'D';
  else if (g.op == 'D') g.op = 'T';
  return g;
}

int main() {
  try {
    std::mt19937 rng(20261025);
    double max_error = 0;
    std::size_t max_components = 0;
    int tested = 0;
    for (int trial = 0; trial < 100; ++trial) {
      const unsigned n = 2 + (trial % 4);
      const unsigned len = 20 + (trial % 7);
      Q q(n, 65536);
      std::vector<C> oracle(std::size_t{1} << n, C{});
      oracle[0] = 1.;
      std::vector<Gate> gates;
      for (unsigned i = 0; i < len; ++i) {
        const unsigned a = rng() % n;
        const unsigned op = rng() % 5;
        const unsigned b = (a + 1 + rng() % (n - 1)) % n;
        Gate gate{("HXCTD")[op], a, b};
        apply(q, gate);
        apply_dense(oracle, gate);
        gates.push_back(gate);
      }
      const auto actual = q.dense_from_reduced();
      const double error = error_phase(actual, oracle);
      max_error = std::max(error, max_error);
      need(error < TOL, "random differential state mismatch trial " + std::to_string(trial));
      need(std::abs(q.sparse_norm() - 1.) < TOL,
           "random circuit lost normalization " + std::to_string(trial));
      for (unsigned bit = 0; bit < n; ++bit) {
        double z = 0;
        for (std::size_t i = 0; i < oracle.size(); ++i)
          z += (i & (std::size_t{1} << bit) ? -1. : 1.) * std::norm(oracle[i]);
        need(std::abs(q.pauli_expectation('Z', bit) - z) < TOL,
             "random Pauli expectation mismatch trial " + std::to_string(trial));
      }
      max_components = std::max(max_components, q.component_count());
      for (auto it = gates.rbegin(); it != gates.rend(); ++it) {
        const auto undo = inverted(*it);
        apply(q, undo);
        apply_dense(oracle, undo);
      }
      std::vector<C> zero(oracle.size(), C{});
      zero[0] = 1.;
      need(error_phase(q.dense_from_reduced(), zero) < TOL,
           "inverse sequence failed trial " + std::to_string(trial));
      ++tested;
    }
    // A hard component budget must reject further growth without mutating
    // the representation or silently pruning amplitudes.
    Q limited(12, 256);
    for (unsigned i = 0; i < 12; ++i) limited.hadamard(i);
    for (unsigned i = 0; i < 11; ++i) limited.cnot(i, i + 1);
    for (unsigned i = 0; i < 8; ++i) limited.phase_t(i);
    need(limited.component_count() == 256, "limit fixture failed to saturate");
    const auto saved = limited.dense_from_reduced();
    bool rejected = false;
    try { limited.phase_t(8); }
    catch (const std::runtime_error&) { rejected = true; }
    need(rejected, "component limit silently exceeded");
    need(limited.component_count() == 256, "limit exception altered component count");
    need(error_phase(limited.dense_from_reduced(), saved) < TOL,
         "limit exception damaged quantum state");

    rejected = false;
    try { limited.cnot(12, 0); }
    catch (const std::out_of_range&) { rejected = true; }
    need(rejected, "out-of-range gate was accepted");

    // Profile precisely the structured circuit family in the original report.
    const auto begin = Clock::now();
    std::cout << std::setprecision(15)
              << "{\"suite\":\"rigorous_reduced_differential\","
              << "\"seed\":20261025,\"random_circuits\":" << tested
              << ",\"max_amplitude_error\":" << max_error
              << ",\"max_random_components\":" << max_components
              << ",\"component_limit_rejected_without_mutation\":true"
              << ",\"profile\":[";
    bool first = true;
    for (unsigned n : {8u, 10u, 12u, 14u, 16u}) {
      const auto start = Clock::now();
      Q q(n, 65536);
      for (unsigned i = 0; i < n; ++i) q.hadamard(i);
      for (unsigned i = 0; i + 1 < n; ++i) q.cnot(i, i + 1);
      for (unsigned i = 0; i < n; ++i) q.phase_t(i);
      const double norm = q.sparse_norm();
      need(std::abs(norm - 1.) < TOL, "profile norm failed");
      const double elapsed = std::chrono::duration<double,std::milli>(
        Clock::now() - start).count();
      if (!first) std::cout << ',';
      first = false;
      std::cout << "{\"qubits\":" << n
                << ",\"T_gates\":" << q.t_count()
                << ",\"components\":" << q.component_count()
                << ",\"logical_payload_bytes\":" << q.logical_payload_bytes()
                << ",\"ms\":" << elapsed << "}";
    }
    std::cout << "],\"total_profile_ms\":"
              << std::chrono::duration<double,std::milli>(Clock::now() - begin).count()
              << "}\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "REVERSE_PC_RIGOR_FAIL " << e.what() << '\n';
    return 1;
  }
}
