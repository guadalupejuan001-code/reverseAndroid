// REVERSE-PC architectural bottleneck probe (test-only).
// Characterizes *avoidable* costs without modifying either production runtime.
// Metrics are per-process measurements, not proof of speedups on all workloads.
#include "adaptive_reduced.hpp"
#include "mundo_x/quantum_runtime.hpp"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using reverse_pc::AdaptiveReduced;
using reverse_pc::Operation;
using Runtime = mundo_x::QuantumRuntime;
using Clock = std::chrono::steady_clock;

void require(bool ok, const std::string& what) {
  if (!ok) throw std::runtime_error(what);
}
double milliseconds(Clock::time_point begin) {
  return std::chrono::duration<double, std::milli>(Clock::now()-begin).count();
}

struct Uncompute {
  std::size_t qubits, width_before_fallback, physical_factors;
  int fallback_at_target;
  bool became_reduced;
  double elapsed_ms;
};
Uncompute test_uncompute_growth() {
  constexpr std::size_t n = 32;
  AdaptiveReduced q(n, 14);
  q.apply(Operation::H, 0);
  int fallback_at = -1;
  std::size_t width_before = 0;
  const auto begin = Clock::now();
  for (std::size_t target = 1; target <= 20; ++target) {
    if (!q.uses_original_reduced()) width_before = q.max_block_width();
    q.apply(Operation::CX, 0, target);
    q.apply(Operation::CX, 0, target); // exact inverse, restores a product.
    if (q.uses_original_reduced() && fallback_at < 0)
      fallback_at = static_cast<int>(target);
  }
  const double ms = milliseconds(begin);
  require(std::abs(q.norm() - 1.) < 1e-9, "uncompute norm");
  require(std::abs(q.expectation('X', 0) - 1.) < 1e-9, "control is not |+>");
  for (std::size_t target = 1; target <= 20; ++target)
    require(std::abs(q.expectation('Z', target) - 1.) < 1e-9,
            "cancelled entanglement failed to restore |0>");
  // These logical factors are independently separable after every pair of CXs;
  // this is a structural observation, not a low-rank extraction algorithm.
  return {n, width_before, n, fallback_at, q.uses_original_reduced(), ms};
}

struct History {
  std::size_t gates, initial_bytes, final_bytes, qubits;
  double ms;
};
History test_noop_history_growth() {
  AdaptiveReduced q(4);
  const std::size_t initial = q.estimated_payload_bytes();
  constexpr std::size_t pairs = 25000;
  const auto begin = Clock::now();
  for (std::size_t i = 0; i < pairs; ++i) {
    q.apply(Operation::X, 0);
    q.apply(Operation::X, 0); // identity, but old history remains allocated.
  }
  const double ms = milliseconds(begin);
  require(!q.uses_original_reduced(), "unexpected fallback in history test");
  require(q.gate_count() == 2*pairs, "gate count mismatch");
  require(std::abs(q.expectation('Z', 0) - 1.) < 1e-12, "noop changes quantum state");
  return {q.gate_count(), initial, q.estimated_payload_bytes(), q.qubits(), ms};
}

struct Replay {
  std::size_t prior_gates, output_components;
  bool fallback;
  double trigger_ms, total_ms;
};
Replay test_replay_tax() {
  AdaptiveReduced q(3, 1);
  const auto begin = Clock::now();
  constexpr std::size_t prior_gates = 20000;
  for (std::size_t i = 0; i < prior_gates; ++i) q.apply(Operation::X, 0);
  q.apply(Operation::H, 0);
  const auto trigger = Clock::now();
  q.apply(Operation::CX, 0, 1); // forces replay of prior_gates+1 gates.
  const double fallback_ms = milliseconds(trigger);
  const double full_ms = milliseconds(begin);
  require(std::abs(q.norm() - 1.) < 1e-9, "replay norm");
  require(std::abs(q.expectation('Z', 0)) < 1e-9, "Bell Z0");
  require(std::abs(q.expectation('Z', 1)) < 1e-9, "Bell Z1");
  return {prior_gates, q.component_count(), q.uses_original_reduced(), fallback_ms, full_ms};
}

struct Batching {
  std::size_t active_qubits, repeated_gates;
  double serial_ms, batched_ms;
  std::size_t serial_revision, batched_revision;
};
Batching test_runtime_copy_cost() {
  constexpr unsigned active=256, repeated=128;
  std::ostringstream initialize;
  initialize << "begin\n";
  for (unsigned i=0;i<active;++i) initialize << "x " << i << "\n";
  initialize << "end\n";
  Runtime baseline;
  baseline.execute(initialize.str());
  require(baseline.core().stats().active_qubits == active, "initialization failed");
  Runtime serial=baseline, batched=baseline;
  std::ostringstream regular, batch;
  batch<<"begin\n";
  for (unsigned i=0;i<repeated;++i) {
    regular<<"x 0\n";
    batch<<"x 0\n";
  }
  batch<<"end\n";
  auto start=Clock::now();
  serial.execute(regular.str());
  const double regular_ms = milliseconds(start);
  start=Clock::now();
  batched.execute(batch.str());
  const double batch_ms = milliseconds(start);
  require(serial.core().stats().active_qubits == active &&
          batched.core().stats().active_qubits == active, "active qubit change");
  require(serial.core().probability_one("0") == 1.0 &&
          batched.core().probability_one("0") == 1.0, "batched operation differs physically");
  // Important: the revision differs because the *transaction boundary*
  // changes; do not silently group commands if revisions are observable.
  return {active,repeated,regular_ms,batch_ms,
          static_cast<std::size_t>(serial.core().stats().revision),
          static_cast<std::size_t>(batched.core().stats().revision)};
}

} // namespace

int main() {
  try {
    const auto u=test_uncompute_growth();
    const auto h=test_noop_history_growth();
    const auto r=test_replay_tax();
    const auto b=test_runtime_copy_cost();
    std::cout<<std::setprecision(14)
             <<"{\"suite\":\"reverse_pc_architecture_bottlenecks\","
             <<"\"head\":\"separability_and_transaction_stress\","
             <<"\"uncompute\":{\"qubits\":"<<u.qubits
             <<",\"factors_in_final_state\":"<<u.physical_factors
             <<",\"width_before_fallback\":"<<u.width_before_fallback
             <<",\"fallback_at_target\":"<<u.fallback_at_target
             <<",\"fallback_used\":"<<(u.became_reduced?"true":"false")
             <<",\"ms\":"<<u.elapsed_ms<<"},"
             <<"\"history\":{\"qubits\":"<<h.qubits
             <<",\"gates\":"<<h.gates
             <<",\"initial_payload_bytes\":"<<h.initial_bytes
             <<",\"final_payload_bytes\":"<<h.final_bytes
             <<",\"ms\":"<<h.ms<<"},"
             <<"\"replay\":{\"prior_gates\":"<<r.prior_gates
             <<",\"fallback_used\":"<<(r.fallback?"true":"false")
             <<",\"components_after\":"<<r.output_components
             <<",\"trigger_ms\":"<<r.trigger_ms
             <<",\"total_ms\":"<<r.total_ms<<"},"
             <<"\"runtime_batch\":{\"active_qubits\":"<<b.active_qubits
             <<",\"repeated_gates\":"<<b.repeated_gates
             <<",\"serial_ms\":"<<b.serial_ms
             <<",\"batched_ms\":"<<b.batched_ms
             <<",\"serial_revision\":"<<b.serial_revision
             <<",\"batched_revision\":"<<b.batched_revision
             <<",\"revisions_differ\":"<<(b.serial_revision != b.batched_revision ? "true" : "false")
             <<"}}\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"REVERSE_PC_ARCH_AUDIT_FAIL "<<e.what()<<'\n';
    return 1;
  }
}
