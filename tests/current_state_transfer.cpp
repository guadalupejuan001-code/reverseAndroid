#include <iostream>
#include <random>

#include "mundo_x/exact_t_experiment.hpp"
using R = mundo_x::ReducedExactTRepresentation;
using C = std::complex<double>;
void need(bool yes, const char* why) {
  if (!yes) throw std::runtime_error(why);
}
void dense(std::vector<C>& v, unsigned op, unsigned a, unsigned b) {
  std::size_t x = std::size_t{1} << a, y = std::size_t{1} << b;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (op == 4) {
      if ((i & x) && !(i & y)) std::swap(v[i], v[i | y]);
    } else if (op == 5) {
      if ((i & x) && (i & y)) v[i] = -v[i];
    } else if (!(i & x)) {
      auto l = v[i], r = v[i | x];
      if (op == 0) {
        v[i] = (l + r) / std::sqrt(2.);
        v[i | x] = (l - r) / std::sqrt(2.);
      } else if (op == 1)
        std::swap(v[i], v[i | x]);
      else if (op == 2)
        v[i | x] = -r;
      else if (op == 3)
        v[i | x] = C{0, 1} * r;
      else
        v[i | x] = r * std::polar(1., op == 6 ? std::acos(-1.) / 4
                                              : -std::acos(-1.) / 4);
    }
  }
}
double error(const std::vector<C>& actual, const std::vector<C>& expected) {
  C overlap{};
  for (std::size_t i = 0; i < actual.size(); ++i)
    overlap += std::conj(expected[i]) * actual[i];
  need(std::abs(overlap) > .9, "transfer overlap");
  auto phase = overlap / std::abs(overlap);
  double e = 0;
  for (std::size_t i = 0; i < actual.size(); ++i)
    e = std::max(e, std::abs(actual[i] - phase * expected[i]));
  return e;
}
void cliff(mundo_x::CompactCore& c, unsigned op, unsigned a, unsigned b) {
  switch (op) {
    case 0:
      c.hadamard(a);
      break;
    case 1:
      c.pauli_x(a);
      break;
    case 2:
      c.pauli_z(a);
      break;
    case 3:
      c.phase_s(a);
      break;
    case 4:
      c.cnot(a, b);
      break;
    case 5:
      c.controlled_z(a, b);
      break;
  }
}
int main() {
  try {
    std::mt19937 rng(20261025);
    double worst = 0;
    const mundo_x::ExactExperimentOp ops[] = {
        mundo_x::ExactExperimentOp::H,  mundo_x::ExactExperimentOp::X,
        mundo_x::ExactExperimentOp::Z,  mundo_x::ExactExperimentOp::S,
        mundo_x::ExactExperimentOp::CX, mundo_x::ExactExperimentOp::CZ};
    for (unsigned trial = 0; trial < 160; ++trial) {
      unsigned n = 2 + trial % 7;
      mundo_x::CompactCore c(n);
      std::vector<C> oracle(std::size_t{1} << n);
      oracle[0] = 1.;
      for (unsigned j = 0; j < 50; ++j) {
        unsigned a = rng() % n, b = (a + 1 + rng() % (n - 1)) % n,
                 op = rng() % 6;
        cliff(c, op, a, b);
        dense(oracle, op, a, b);
      }
      auto r = R::from_stabilizer(c, 65536);
      worst = std::max(worst, error(r.dense_current(), oracle));
      for (unsigned j = 0; j < 35; ++j) {
        unsigned a = rng() % n, b = (a + 1 + rng() % (n - 1)) % n,
                 op = rng() % 8;
        if (op < 6)
          r.clifford_current(ops[op], a, b);
        else {
          R::WordArray x{}, z{};
          z[a / 64] |= std::uint64_t{1} << (a % 64);
          r.pauli_phase_current(x, z, 0,
                                (op == 6 ? 1 : -1) * std::acos(-1.) / 4);
        }
        dense(oracle, op, a, b);
        worst = std::max(worst, error(r.dense_current(), oracle));
        need(worst < 2e-10, "dense current-state differential");
      }
      auto saved = r.current_snapshot();
      auto restored = R::from_current_snapshot(saved);
      need(saved == restored.current_snapshot(), "current snapshot not stable");
      need(error(restored.dense_current(), oracle) < 2e-10, "restore vector");
      auto bad = saved;
      bad[bad.size() / 2] ^= 1;
      bool rejected = false;
      try {
        R::from_current_snapshot(bad);
      } catch (const std::exception&) {
        rejected = true;
      }
      need(rejected, "damaged Reduced snapshot");
    }
    for (unsigned n : {128u, 256u, 512u, 1000u}) {
      mundo_x::CompactCore c(n);
      c.hadamard(0);
      for (unsigned q = 1; q < n; ++q) c.cnot(q - 1, q);
      auto r = R::from_stabilizer(c, 16);
      R::WordArray x{}, z{};
      z[0] = 1;
      r.pauli_phase_current(x, z, 0, std::acos(-1.) / 4);
      for (unsigned q = 0; q < n; ++q)
        x[q / 64] |= std::uint64_t{1} << (q % 64);
      R::WordArray zero{};
      need(std::abs(r.expectation_current(x, zero).real() - 1 / std::sqrt(2.)) <
               1e-10,
           "large GHZ phase correlation");
      z[(n - 1) / 64] |= std::uint64_t{1} << ((n - 1) % 64);
      need(std::abs(r.expectation_current(zero, z).real() - 1) < 1e-10,
           "far Z correlation");
      auto restored = R::from_current_snapshot(r.current_snapshot(), 16);
      z = {};
      z[0] = 1;
      restored.pauli_phase_current(zero, z, 0, -std::acos(-1.) / 4);
      need(std::abs(restored.expectation_current(x, zero).real() - 1) < 1e-10,
           "large inverse phase");
    }
    R projection(1, 4);
    projection.clifford_current(mundo_x::ExactExperimentOp::H, 0);
    R::WordArray px{}, pz{};
    pz[0] = 1;
    projection.pauli_phase_current(px, pz, 0, std::acos(-1.) / 4);
    projection.clifford_current(mundo_x::ExactExperimentOp::H, 0);
    need(projection.eigen_current(px, pz) == 0, "near/eigen classifier");
    auto probability = projection.project_current(px, pz, 0, true);
    need(std::abs(probability - (1 - 1 / std::sqrt(2.)) * .5) < 1e-12 &&
             projection.eigen_current(px, pz) == -1,
         "arbitrary Born projection");
    R a(63, 16), b(65, 16);
    a.clifford_current(mundo_x::ExactExperimentOp::H, 62);
    b.clifford_current(mundo_x::ExactExperimentOp::X, 64);
    auto joined = R::tensor_state(a, b, 16);
    R::WordArray x{}, z{};
    x[0] = std::uint64_t{1} << 62;
    need(std::abs(joined.expectation_current(x, z).real() - 1) < 1e-12,
         "cross-word tensor X");
    x = {};
    z[1] = std::uint64_t{1} << 63;
    need(std::abs(joined.expectation_current(x, z).real() + 1) < 1e-12,
         "cross-word tensor Z");
    R cap(8, 8);
    for (unsigned q = 0; q < 8; ++q)
      cap.clifford_current(mundo_x::ExactExperimentOp::H, q);
    for (unsigned q = 0; q < 3; ++q) {
      R::WordArray x{}, z{};
      z[0] = 1u << q;
      cap.pauli_phase_current(x, z, 0, std::acos(-1.) / 4);
    }
    auto before = cap.current_snapshot();
    bool rejected = false;
    try {
      R::WordArray x{}, z{};
      z[0] = 8;
      cap.pauli_phase_current(x, z, 0, std::acos(-1.) / 4);
    } catch (const std::length_error&) {
      rejected = true;
    }
    need(rejected && before == cap.current_snapshot(),
         "component overflow corrupted state");
    std::cout << std::setprecision(15)
              << "{\"suite\":\"current_state_transfer\",\"random_circuits\":"
                 "160,\"max_error\":"
              << worst
              << ",\"large_sizes\":[128,256,512,1000],\"atomic_cap\":true}\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
