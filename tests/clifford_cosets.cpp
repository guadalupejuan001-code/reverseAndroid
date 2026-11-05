#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <random>

#include "mundo_x/exact_t_experiment.hpp"

namespace allocation {
thread_local bool active = false;
thread_local long fail_after = -1;
thread_local std::size_t calls = 0, bytes = 0;
struct Guard {
  explicit Guard(long failure = -1) {
    calls = bytes = 0;
    fail_after = failure;
    active = true;
  }
  ~Guard() {
    active = false;
    fail_after = -1;
  }
};
}  // namespace allocation
void* operator new(std::size_t n) {
  if (allocation::active) {
    ++allocation::calls;
    allocation::bytes += n;
    if (allocation::fail_after >= 0 && allocation::fail_after-- == 0)
      throw std::bad_alloc();
  }
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using R = mundo_x::ReducedExactTRepresentation;
using Op = mundo_x::ExactExperimentOp;
using Clock = std::chrono::steady_clock;
struct Gate {
  Op op;
  unsigned a, b = 0;
};
void need(bool yes, const char* why) {
  if (!yes) throw std::runtime_error(why);
}
void compact(mundo_x::CompactCore& state, Gate g) {
  switch (g.op) {
    case Op::H:
      state.hadamard(g.a);
      break;
    case Op::S:
      state.phase_s(g.a);
      break;
    case Op::X:
      state.pauli_x(g.a);
      break;
    case Op::Z:
      state.pauli_z(g.a);
      break;
    case Op::CX:
      state.cnot(g.a, g.b);
      break;
    case Op::CZ:
      state.controlled_z(g.a, g.b);
      break;
    default:
      throw std::logic_error("Clifford test gate");
  }
}
void phase(R& state, unsigned q) {
  R::WordArray x{}, z{};
  z[q / 64] = std::uint64_t{1} << (q % 64);
  state.pauli_phase_current(x, z, 0, std::acos(-1.) / 4);
}
mundo_x::CompactCore graph(unsigned n) {
  mundo_x::CompactCore c(n);
  for (unsigned q = 0; q < n; ++q) c.hadamard(q);
  for (unsigned q = 1; q < n; ++q) c.controlled_z(q - 1, q);
  for (unsigned q = 0; q < n / 2; ++q) c.controlled_z(q, q + n / 2);
  return c;
}
R state(unsigned n, unsigned t) {
  auto r = R::from_stabilizer(graph(n), 65536);
  for (unsigned q = 0; q < t; ++q) phase(r, q);
  return r;
}
std::array<unsigned, 2> memory() {
  std::array<unsigned, 2> out{};
  std::ifstream file("/proc/self/status");
  std::string key;
  while (file >> key) {
    if (key == "VmRSS:") file >> out[0];
    if (key == "VmHWM:") file >> out[1];
    std::string rest;
    std::getline(file, rest);
  }
  return out;
}
void self_test() {
  std::mt19937 random(20261025);
  unsigned comparisons = 0, failures = 0;
  constexpr Op ops[] = {Op::H, Op::S, Op::X, Op::Z, Op::CX, Op::CZ};
  for (unsigned trial = 0; trial < 48; ++trial) {
    const unsigned n = 2 + trial % 7;
    auto reference = R::from_stabilizer(graph(n), 65536), candidate = reference;
    for (unsigned j = 0; j < 80; ++j) {
      unsigned a = random() % n, b = (a + 1 + random() % (n - 1)) % n;
      if (j % 5 == 0) {
        phase(reference, a);
        phase(candidate, a);
      } else {
        auto op = ops[random() % 6];
        reference.clifford_current(op, a, b, false);
        candidate.clifford_current(op, a, b, true);
      }
      need(reference.current_snapshot() == candidate.current_snapshot(),
           "Clifford coset differential snapshot mismatch");
      ++comparisons;
    }
  }
  for (unsigned n : {128u, 256u, 512u, 1000u}) {
    auto reference = state(n, 8), candidate = reference;
    for (unsigned j = 0; j < 18; ++j) {
      auto op = ops[j % 6];
      unsigned a = (j * 127) % n, b = (a + n / 2) % n;
      reference.clifford_current(op, a, b, false);
      candidate.clifford_current(op, a, b, true);
      need(reference.current_snapshot() == candidate.current_snapshot(),
           "large coset differential mismatch");
      ++comparisons;
    }
  }
  auto seed = state(32, 4);
  for (bool skip : {false, true})
    for (auto gate : {Gate{Op::H, 0}, Gate{Op::CZ, 30, 31}}) {
      auto successful = seed;
      {
        allocation::Guard guard;
        successful.clifford_current(gate.op, gate.a, gate.b, skip);
      }
      const auto calls = allocation::calls;
      const auto expected = successful.current_snapshot();
      for (std::size_t fail = 0; fail <= calls; ++fail) {
        auto attempt = seed;
        const auto before = attempt.current_snapshot();
        bool rejected = false;
        try {
          allocation::Guard guard(static_cast<long>(fail));
          attempt.clifford_current(gate.op, gate.a, gate.b, skip);
        } catch (const std::bad_alloc&) {
          rejected = true;
          ++failures;
        }
        need(attempt.current_snapshot() == (rejected ? before : expected),
             "allocation failure corrupted Clifford state");
        need(rejected == (fail < calls), "allocation injection missed a site");
      }
    }
  for (auto gate : {Gate{Op::H, 32}, Gate{Op::CX, 0, 0}, Gate{Op::T, 0}}) {
    auto attempt = seed;
    const auto before = attempt.current_snapshot();
    bool rejected = false;
    try {
      attempt.clifford_current(gate.op, gate.a, gate.b);
    } catch (const std::exception&) {
      rejected = true;
    }
    need(rejected && attempt.current_snapshot() == before,
         "invalid Clifford changed state");
  }
  std::cout
      << "{\"suite\":\"certified_unchanged_cosets\",\"snapshot_comparisons\":"
      << comparisons << ",\"injected_allocation_failures\":" << failures
      << ",\"seed\":20261025}\n";
}
void benchmark(unsigned n, unsigned t, const std::string& family, bool skip,
               bool count_allocations = false) {
  need((n == 128 || n == 256 || n == 512 || n == 1000) && (t == 8 || t == 16),
       "benchmark bounds");
  auto r = state(n, t);
  auto oracle = graph(n);
  std::vector<Gate> gates;
  for (unsigned j = 0; j < 96; ++j) {
    if (family == "commuting") {
      unsigned a = (j * 127 + 1) % n;
      gates.push_back({Op::CZ, a, (a + n / 2) % n});
    } else if (family == "affected")
      gates.push_back({Op::H, 0});
    else if (family == "mixed")
      gates.push_back({Op::H, j % n});
    else
      throw std::invalid_argument("benchmark family");
  }
  auto before = memory();
  const auto start = Clock::now();
  allocation::calls = allocation::bytes = 0;
  if (count_allocations) {
    allocation::Guard guard;
    for (auto g : gates) r.clifford_current(g.op, g.a, g.b, skip);
  } else {
    for (auto g : gates) r.clifford_current(g.op, g.a, g.b, skip);
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  const auto calls = allocation::calls, bytes = allocation::bytes;
  auto after = memory();
  for (auto g : gates) compact(oracle, g);
  const auto norm = r.norm_current();
  double error = std::abs(norm - 1.);
  for (unsigned row : {0u, t, n / 2, n - 1}) {
    const auto p = oracle.generator_string(row);
    R::WordArray x{}, z{};
    unsigned phase = p[0] == '-' ? 2 : 0;
    for (unsigned q = 0; q < n; ++q) {
      const auto axis = p[q + 1];
      if (axis == 'X' || axis == 'Y') x[q / 64] |= std::uint64_t{1} << (q % 64);
      if (axis == 'Z' || axis == 'Y') z[q / 64] |= std::uint64_t{1} << (q % 64);
      if (axis == 'Y') ++phase;
    }
    const auto expected = row < t ? 1. / std::sqrt(2.) : 1.;
    error = std::max(
        error, std::abs(r.expectation_current(x, z, phase & 3).real() / norm -
                        expected));
  }
  need(error < 1e-10 && r.component_count() == (1u << t),
       "large graph invariant");
  auto snapshot = r.current_snapshot();
  auto digest = reverse_pc::state_io::hash(snapshot, snapshot.size());
  std::cout << std::setprecision(16) << "{\"qubits\":" << n << ",\"T\":" << t
            << ",\"coefficients\":" << r.component_count() << ",\"family\":\""
            << family << "\",\"skip_unchanged\":" << (skip ? "true" : "false")
            << ",\"gates\":96,\"ms\":" << elapsed
            << ",\"allocations\":" << calls << ",\"allocated_bytes\":" << bytes
            << ",\"RSS_before_kib\":" << before[0]
            << ",\"RSS_after_kib\":" << after[0]
            << ",\"peak_RSS_kib\":" << after[1]
            << ",\"payload_bytes\":" << r.logical_payload_bytes()
            << ",\"max_error\":" << error << ",\"state_digest\":\"" << digest
            << "\"}\n";
}
int main(int argc, char** argv) {
  try {
    if (argc == 5 || argc == 6)
      benchmark(std::stoul(argv[1]), std::stoul(argv[2]), argv[3],
                std::stoi(argv[4]) != 0, argc == 6 && std::stoi(argv[5]) != 0);
    else
      self_test();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
