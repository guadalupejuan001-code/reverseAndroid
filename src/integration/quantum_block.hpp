#pragma once
#include <variant>

#include "adaptive_reduced.hpp"
#include "integration/state_io.hpp"
#include "mundo_x/pauli_observer.hpp"
namespace reverse_pc {
// Exactly one alternative owns the current finite component. Tableau imports
// are state transfers; no second live register is maintained as an oracle.
class QuantumBlock {
 public:
  using Compact = mundo_x::CompactCore;
  using R = mundo_x::ReducedExactTRepresentation;
  using Pauli = mundo_x_bridge::Pauli;
  using Op = mundo_x::ExactExperimentOp;
  QuantumBlock(Compact state, std::size_t cap = 65536, std::size_t dense = 8,
               bool fast = true)
      : value_(std::move(state)), cap_(cap), dense_(dense), fast_(fast) {
    if (!cap || dense > 14) throw std::invalid_argument("quantum block limits");
  }
  bool is_clifford() const { return std::holds_alternative<Compact>(value_); }
  bool is_adaptive() const {
    return std::holds_alternative<AdaptiveReduced>(value_);
  }
  operator const Compact&() const { return std::get<Compact>(value_); }
  Compact::PackedState packed_state() const {
    return std::get<Compact>(value_).packed_state();
  }
  std::size_t qubits() const {
    return std::visit([](const auto& v) { return v.qubits(); }, value_);
  }
  std::size_t state_bytes() const {
    if (is_clifford()) return std::get<Compact>(value_).state_bytes();
    if (is_adaptive())
      return std::get<AdaptiveReduced>(value_).estimated_payload_bytes();
    return std::get<R>(value_).logical_payload_bytes();
  }
  std::size_t reduced_components() const {
    return std::holds_alternative<R>(value_)
               ? std::get<R>(value_).component_count()
               : 0;
  }
  std::size_t amplitudes() const {
    return is_adaptive() ? std::get<AdaptiveReduced>(value_).factor_amplitudes()
                         : 0;
  }
  std::size_t t_count() const { return t_count_; }
  double norm() const {
    return is_clifford()   ? 1.
           : is_adaptive() ? std::get<AdaptiveReduced>(value_).norm()
                           : std::get<R>(value_).norm_current();
  }
  R as_reduced() const {
    if (is_clifford())
      return R::from_stabilizer(std::get<Compact>(value_), cap_);
    if (is_adaptive()) {
      const auto& a = std::get<AdaptiveReduced>(value_);
      if (a.block_count() == a.qubits())
        return R::from_product(a.product_state(), cap_);
      return R::from_amplitudes(a.dense(), qubits(), cap_);
    }
    return std::get<R>(value_);
  }
  static QuantumBlock tensor_product(const QuantumBlock& a,
                                     const QuantumBlock& b) {
    QuantumBlock out(Compact(1), a.cap_, a.dense_, a.fast_);
    if (a.is_clifford() && b.is_clifford())
      out.value_ = Compact::tensor_product(std::get<Compact>(a.value_),
                                           std::get<Compact>(b.value_));
    else if (a.qubits() + b.qubits() <= a.dense_) {
      auto l = a.local_dense(), r = b.local_dense();
      std::vector<Complex> v(l.size() * r.size());
      for (std::size_t j = 0; j < r.size(); ++j)
        for (std::size_t i = 0; i < l.size(); ++i)
          v[i + j * l.size()] = l[i] * r[j];
      out.value_ =
          AdaptiveReduced::from_dense(a.qubits() + b.qubits(), v, a.cap_);
    } else
      out.value_ = R::tensor_state(a.as_reduced(), b.as_reduced(), a.cap_);
    if (b.t_count_ > std::numeric_limits<std::size_t>::max() - a.t_count_)
      throw std::overflow_error("component T counter overflow");
    out.t_count_ = a.t_count_ + b.t_count_;
    return out;
  }
  void hadamard(std::size_t q) { cliff(Op::H, Operation::H, q); }
  void phase_s(std::size_t q) { cliff(Op::S, Operation::S, q); }
  void pauli_x(std::size_t q) { cliff(Op::X, Operation::X, q); }
  void pauli_z(std::size_t q) { cliff(Op::Z, Operation::Z, q); }
  void cnot(std::size_t a, std::size_t b) {
    cliff(Op::CX, Operation::CX, a, b);
  }
  void controlled_z(std::size_t a, std::size_t b) {
    cliff(Op::CZ, Operation::CZ, a, b);
  }
  void phase_t(std::size_t q, bool dagger = false) {
    if (t_count_ == std::numeric_limits<std::size_t>::max())
      throw std::overflow_error("T counter overflow");
    if (is_clifford()) {
      if (qubits() <= dense_)
        value_ = AdaptiveReduced::from_dense(
            qubits(), R::stabilizer_amplitudes(std::get<Compact>(value_)),
            cap_);
      else
        value_ = R::from_stabilizer(std::get<Compact>(value_), cap_);
    }
    if (is_adaptive()) {
      auto& a = std::get<AdaptiveReduced>(value_);
      a.apply(dagger ? Operation::TDag : Operation::T, q);
      a.discard_unneeded_history();
    } else {
      R::WordArray x{}, z{};
      z[q / 64] |= std::uint64_t{1} << (q % 64);
      std::get<R>(value_).pauli_phase_current(
          x, z, 0, (dagger ? -1. : 1.) * std::acos(-1.) / 4);
    }
    ++t_count_;
  }
  bool phase_pauli(const Pauli& p, bool dagger) {
    if (is_adaptive() || (is_clifford() && qubits() <= dense_)) return false;
    if (t_count_ == std::numeric_limits<std::size_t>::max())
      throw std::overflow_error("T counter overflow");
    if (is_clifford())
      value_ = R::from_stabilizer(std::get<Compact>(value_), cap_);
    R::WordArray x{}, z{};
    for (std::size_t w = 0; w < p.x.size(); ++w) {
      x[w] = p.x[w];
      z[w] = p.z[w];
    }
    std::get<R>(value_).pauli_phase_current(
        x, z, p.phase, (dagger ? -1. : 1.) * std::acos(-1.) / 4);
    ++t_count_;
    return true;
  }
  double expectation(const Pauli& p) const {
    if (is_clifford())
      return mundo_x_bridge::PauliObserver(std::get<Compact>(value_))
          .expectation(p);
    if (is_adaptive()) {
      auto v = std::get<AdaptiveReduced>(value_).dense();
      Complex out{};
      for (std::size_t i = 0; i < v.size(); ++i)
        out += std::conj(v[i ^ p.x[0]]) * v[i] * unit(p.phase) *
               (__builtin_parityll(p.z[0] & i) ? -1. : 1.);
      return out.real() / norm();
    }
    R::WordArray x{}, z{};
    for (std::size_t w = 0; w < p.x.size(); ++w) {
      x[w] = p.x[w];
      z[w] = p.z[w];
    }
    return std::get<R>(value_).expectation_current(x, z, p.phase).real() /
           norm();
  }
  int eigen(const Pauli& p) const {
    if (is_clifford()) {
      auto v = expectation(p);
      return v == 1 ? 1 : v == -1 ? -1 : 0;
    }
    if (is_adaptive()) {
      auto v = std::get<AdaptiveReduced>(value_).dense();
      bool plus = true, minus = true;
      for (std::size_t i = 0; i < v.size(); ++i) {
        auto acted =
            v[i] * unit(p.phase) * (__builtin_parityll(p.z[0] & i) ? -1. : 1.);
        plus = plus && (acted == v[i ^ p.x[0]]);
        minus = minus && (acted == -v[i ^ p.x[0]]);
      }
      return plus ? 1 : minus ? -1 : 0;
    }
    R::WordArray x{}, z{};
    for (std::size_t w = 0; w < p.x.size(); ++w) {
      x[w] = p.x[w];
      z[w] = p.z[w];
    }
    return std::get<R>(value_).eigen_current(x, z, p.phase);
  }
  std::array<long double, 2> born_pair(const Pauli& p) const {
    if (is_clifford()) {
      auto e = expectation(p);
      return {{(1 + e) * .5L, (1 - e) * .5L}};
    }
    if (is_adaptive()) {
      auto v = std::get<AdaptiveReduced>(value_).dense();
      std::array<long double, 2> out{};
      for (std::size_t i = 0; i < v.size(); ++i) {
        auto j = i ^ p.x[0];
        auto acted =
            v[j] * unit(p.phase) * (__builtin_parityll(p.z[0] & j) ? -1. : 1.);
        for (unsigned branch = 0; branch < 2; ++branch) {
          auto c = (v[i] + (branch ? -acted : acted)) * .5;
          long double a = c.real(), b = c.imag();
          out[branch] += a * a + b * b;
        }
      }
      auto total = out[0] + out[1];
      if (!(total > 0)) throw std::runtime_error("Born norm underflow");
      out[0] /= total;
      out[1] /= total;
      return out;
    }
    R::WordArray x{}, z{};
    for (std::size_t w = 0; w < p.x.size(); ++w) {
      x[w] = p.x[w];
      z[w] = p.z[w];
    }
    return std::get<R>(value_).born_pair_current(x, z, p.phase);
  }
  Pauli z_pauli(std::size_t q) const {
    Pauli p;
    p.x.resize((qubits() + 63) / 64);
    p.z = p.x;
    p.z[q / 64] |= std::uint64_t{1} << (q % 64);
    return p;
  }
  int z_eigen(std::size_t q) const { return eigen(z_pauli(q)); }
  double probability_outcome_z(std::size_t q, bool one) const {
    return represent_probability(born_pair(z_pauli(q))[one ? 1 : 0]);
  }
  double probability_one_z(std::size_t q) const {
    return probability_outcome_z(q, true);
  }
  static double represent_probability(long double p) {
    double out = static_cast<double>(p);
    if (!std::isfinite(out) || (p > 0 && out == 0))
      throw std::range_error("Born probability outside double range");
    return out;
  }
  bool measure_z_forced(std::size_t q, bool one) {
    if (is_clifford())
      return std::get<Compact>(value_).measure_z_forced(q, one);
    auto sign = z_eigen(q);
    if (sign) {
      if ((sign == -1) != one)
        throw std::invalid_argument("impossible measurement");
      return false;
    }
    if (is_adaptive()) {
      auto v = std::get<AdaptiveReduced>(value_).dense();
      long double norm = 0;
      for (std::size_t i = 0; i < v.size(); ++i) {
        if (bool((i >> q) & 1) != one)
          v[i] = 0.;
        else {
          long double a = v[i].real(), b = v[i].imag();
          norm += a * a + b * b;
        }
      }
      if (!(norm > 0)) throw std::runtime_error("projection underflow");
      for (auto& a : v)
        a = {static_cast<double>(a.real() / std::sqrt(norm)),
             static_cast<double>(a.imag() / std::sqrt(norm))};
      value_ = AdaptiveReduced::from_dense(qubits(), v, cap_);
    } else {
      R::WordArray x{}, z{};
      z[q / 64] |= std::uint64_t{1} << (q % 64);
      std::get<R>(value_).project_current(x, z, 0, one);
    }
    return true;
  }
  std::string snapshot() const {
    state_io::Writer w;
    w.data = "QBL1";
    w.byte(is_clifford() ? 0 : is_adaptive() ? 1 : 2);
    w.u64(qubits());
    w.u64(t_count_);
    if (is_clifford()) {
      auto p = packed_state();
      for (auto v : p.signs) w.byte(v);
      for (auto v : p.x) w.u64(v);
      for (auto v : p.z) w.u64(v);
    } else if (is_adaptive())
      w.string(std::get<AdaptiveReduced>(value_).local_snapshot());
    else
      w.string(std::get<R>(value_).current_snapshot());
    return w.finish();
  }
  static QuantumBlock from_snapshot(const std::string& saved, std::size_t cap,
                                    std::size_t dense, bool fast) {
    state_io::Reader r(saved);
    r.require(4);
    if (saved.substr(0, 4) != "QBL1")
      throw std::invalid_argument("quantum block version");
    r.at = 4;
    auto tag = r.byte();
    auto n = r.u64(), count = r.u64();
    if (n == 0 || n > 4096 || (tag != 0 && n > 1000))
      throw std::length_error("quantum block width");
    QuantumBlock out(Compact(1), cap, dense, fast);
    if (tag == 0) {
      auto words = (n + 63) / 64;
      r.require(n + 16 * n * words);
      Compact::PackedState p;
      p.qubits = n;
      p.signs.resize(n);
      p.x.resize(n * words);
      p.z.resize(n * words);
      for (auto& v : p.signs) v = r.byte();
      for (auto& v : p.x) v = r.u64();
      for (auto& v : p.z) v = r.u64();
      out.value_ = Compact::from_packed_state(p);
    } else if (tag == 1) {
      if (n > dense || n > 14)
        throw std::length_error("restored dense block budget");
      out.value_ = AdaptiveReduced::from_local_snapshot(
          r.string(64 * 1024 * 1024), cap, dense);
      if (out.qubits() != n)
        throw std::invalid_argument("Adaptive width mismatch");
    } else if (tag == 2) {
      out.value_ = R::from_current_snapshot(r.string(64 * 1024 * 1024), cap);
      if (out.qubits() != n)
        throw std::invalid_argument("restored quantum width mismatch");
    } else
      throw std::invalid_argument("unknown quantum representation");
    r.done();
    out.t_count_ = count;
    return out;
  }

 private:
  std::variant<Compact, AdaptiveReduced, R> value_;
  std::size_t cap_, dense_, t_count_ = 0;
  bool fast_;
  static Complex unit(unsigned p) {
    switch (p & 3) {
      case 0:
        return 1.;
      case 1:
        return {0, 1};
      case 2:
        return -1.;
      default:
        return {0, -1};
    }
  }
  std::vector<Complex> local_dense() const {
    return is_clifford()   ? R::stabilizer_amplitudes(std::get<Compact>(value_))
           : is_adaptive() ? std::get<AdaptiveReduced>(value_).dense()
                           : std::get<R>(value_).dense_current();
  }
  void cliff(Op op, Operation adaptive, std::size_t a, std::size_t b = 0) {
    if (is_clifford()) {
      auto& c = std::get<Compact>(value_);
      switch (op) {
        case Op::H:
          c.hadamard(a);
          break;
        case Op::X:
          c.pauli_x(a);
          break;
        case Op::Z:
          c.pauli_z(a);
          break;
        case Op::S:
          c.phase_s(a);
          break;
        case Op::CX:
          c.cnot(a, b);
          break;
        case Op::CZ:
          c.controlled_z(a, b);
          break;
        default:
          throw std::invalid_argument("block Clifford");
      }
    } else if (is_adaptive()) {
      auto& c = std::get<AdaptiveReduced>(value_);
      c.apply(adaptive, a, b);
      c.discard_unneeded_history();
    } else if (fast_) {
#ifdef REVERSE_PC_COSET_REFERENCE
      std::get<R>(value_).clifford_current(op, a, b, false);
#else
      std::get<R>(value_).clifford_current(op, a, b);
#endif
    } else {
      auto& c = std::get<R>(value_);
      switch (op) {
        case Op::H:
          c.hadamard(a);
          break;
        case Op::X:
          c.pauli_x(a);
          break;
        case Op::Z:
          c.pauli_z(a);
          break;
        case Op::S:
          c.phase_s(a);
          break;
        case Op::CX:
          c.cnot(a, b);
          break;
        case Op::CZ:
          c.controlled_z(a, b);
          break;
        default:
          throw std::invalid_argument("block Clifford");
      }
    }
  }
};
}  // namespace reverse_pc
