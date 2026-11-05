#pragma once
// REVERSE-PC adaptive factorization layer for the pinned Mundo X Reduced backend.
//
// Stores independent entangled blocks as small dense vectors. It does NOT
// expand their tensor product. A cross-block CX/CZ first checks exact
// single-qubit eigenstate identities; otherwise blocks are joined when small.
// Above the dense-block limit, product states transfer directly into Reduced;
// entangled blocks retain canonical replay as the compatibility fallback.
// No pruning, measurement, qubit truncation, or change to Mundo X is used.
#include "mundo_x/exact_t_experiment.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace reverse_pc {

using Complex = std::complex<double>;
using Reduced = mundo_x::ReducedExactTRepresentation;
enum class Operation : unsigned char { H, X, Z, S, T, TDag, CX, CZ };
struct Gate {
  Operation operation;
  std::size_t a;
  std::size_t b = 0;
};

class AdaptiveReduced {
 public:
  explicit AdaptiveReduced(std::size_t qubits,
                           std::size_t max_dense_block = 14,
                           std::size_t max_components = 65536)
      : n_(qubits), max_block_(max_dense_block), max_components_(max_components),
        owner_(qubits) {
    if (qubits == 0 || qubits > 1000)
      throw std::invalid_argument("adaptive reduced supports 1..1000 qubits");
    if (max_dense_block == 0 || max_dense_block > 20)
      throw std::invalid_argument("max_dense_block must be within 1..20");
    if (max_components == 0)
      throw std::invalid_argument("max_components must be positive");
    blocks_.reserve(n_);
    for (std::size_t q = 0; q < n_; ++q) {
      Block block;
      block.ids = {q};
      block.psi = {Complex{1., 0.}, Complex{0., 0.}};
      blocks_.push_back(std::move(block));
      owner_[q] = q;
    }
  }

  AdaptiveReduced(const AdaptiveReduced& other)
      : n_(other.n_),
        max_block_(other.max_block_),
        max_components_(other.max_components_),
        gates_(other.gates_),
        replayed_gates_(other.replayed_gates_),
        imported_product_(other.imported_product_),
        blocks_(other.blocks_),
        owner_(other.owner_),
        history_(other.history_),
        reduced_(other.reduced_ ? std::make_unique<Reduced>(*other.reduced_)
                                : nullptr) {
    blocks_.reserve(n_);
  }
  AdaptiveReduced(AdaptiveReduced&&) noexcept = default;
  AdaptiveReduced& operator=(AdaptiveReduced&&) noexcept = default;
  AdaptiveReduced& operator=(const AdaptiveReduced& other) {
    if (this != &other) {
      AdaptiveReduced next(other);
      *this = std::move(next);
    }
    return *this;
  }
  static AdaptiveReduced from_dense(std::size_t n,
                                    const std::vector<Complex>& values,
                                    std::size_t cap = 65536) {
    if (n > 14 || values.size() != (std::size_t{1} << n))
      throw std::invalid_argument("adaptive local import width");
    AdaptiveReduced out(n, n, cap);
    Block block;
    for (std::size_t i = 0; i < n; ++i) block.ids.push_back(i);
    double norm = 0.;
    for (auto v : values) {
      if (!std::isfinite(v.real()) || !std::isfinite(v.imag()))
        throw std::invalid_argument("nonfinite amplitude");
      norm += std::norm(v);
    }
    if (!(norm > 0.) || !std::isfinite(norm)) throw std::invalid_argument("zero/nonfinite local state norm");
    block.psi = values;
    out.blocks_ = refactor(std::move(block));
    out.blocks_.reserve(n);
    out.refresh_owners();
    return out;
  }
  std::vector<Reduced::ProductPair> product_state() const {
    if (reduced_ || blocks_.size() != n_)
      throw std::logic_error("not a singleton product");
    std::vector<Reduced::ProductPair> product(n_);
    for (const auto& b : blocks_) product[b.ids[0]] = {b.psi[0], b.psi[1]};
    return product;
  }
  std::string local_snapshot() const {
    if (n_ > 14 || max_block_ < n_ || reduced_)
      throw std::logic_error("not a local persistent Adaptive state");
    state_io::Writer w;
    w.data = "ADL1";
    w.u64(n_);
    w.u64(gates_);
    w.u64(blocks_.size());
    for (const auto& b : blocks_) {
      w.u64(b.ids.size());
      for (auto id : b.ids) w.u64(id);
      w.u64(b.cuts.size());
      for (auto cut : b.cuts) w.u64(cut);
      for (auto v : b.psi) w.complex(v);
    }
    return w.finish();
  }
  static AdaptiveReduced from_local_snapshot(const std::string& saved,
                                             std::size_t cap,
                                             std::size_t dense) {
    state_io::Reader r(saved);
    r.require(4);
    if (saved.substr(0, 4) != "ADL1")
      throw std::invalid_argument("Adaptive local snapshot version");
    r.at = 4;
    auto n = r.u64();
    if (n == 0 || n > dense || n > 14)
      throw std::length_error("Adaptive restore width");
    AdaptiveReduced out(n, n, cap);
    out.gates_ = r.u64();
    auto count = r.u64();
    if (count == 0 || count > n)
      throw std::invalid_argument("Adaptive factor count");
    out.blocks_.clear();
    std::vector<bool> used(n, false);
    for (std::size_t i = 0; i < count; ++i) {
      Block b;
      auto width = r.u64();
      if (width == 0 || width > n)
        throw std::invalid_argument("Adaptive factor width");
      b.ids.reserve(width);
      for (std::size_t k = 0; k < width; ++k) {
        auto id = r.u64();
        if (id >= n || used[id])
          throw std::invalid_argument("Adaptive duplicate wire");
        used[id] = true;
        b.ids.push_back(id);
      }
      auto cuts = r.u64();
      if (cuts > 2 * width) throw std::length_error("Adaptive cut budget");
      auto full = (std::size_t{1} << width) - 1;
      b.cuts.reserve(cuts);
      for (std::size_t k = 0; k < cuts; ++k) {
        auto cut = r.u64();
        if (!cut || cut >= full)
          throw std::invalid_argument("Adaptive invalid cut");
        b.cuts.push_back(cut);
      }
      auto size = std::size_t{1} << width;
      r.require(size * 16);
      b.psi.resize(size);
      double norm = 0;
      for (auto& v : b.psi) {
        v = r.complex();
        norm += std::norm(v);
      }
      if (!(norm > 0.) || !std::isfinite(norm))
        throw std::invalid_argument("Adaptive norm");
      out.blocks_.push_back(std::move(b));
    }
    if (std::find(used.begin(), used.end(), false) != used.end())
      throw std::invalid_argument("Adaptive incomplete partition");
    r.done();
    out.refresh_owners();
    return out;
  }
  void discard_unneeded_history() {
    if (max_block_ < n_ || reduced_)
      throw std::logic_error("replay history still required");
    std::vector<Gate>().swap(history_);
  }

  void apply(Operation op, std::size_t a, std::size_t b = 0) {
    if(gates_==std::numeric_limits<std::size_t>::max())throw std::overflow_error("adaptive gate counter overflow");
    if (static_cast<unsigned>(op) > static_cast<unsigned>(Operation::CZ))
      throw std::invalid_argument("invalid adaptive operation");
    check(a);
    if (op == Operation::CX || op == Operation::CZ) {
      check(b);
      if (a == b) throw std::invalid_argument("two-qubit gate requires distinct wires");
    }
    const Gate gate{op, a, b};
    if (reduced_) {
      apply_reduced(*reduced_, gate);
      ++gates_;
      return;
    }
    if (op != Operation::CX && op != Operation::CZ) {
      // Record first to avoid a partially updated state on allocation failure.
      record(gate);
      apply_single(blocks_[owner_[a]], local(owner_[a], a), op);
      ++gates_;
      return;
    }

    const auto first = owner_[a], second = owner_[b];
    if (first == second) {
      // Work on a local candidate: factoring may allocate, so neither the
      // quantum state nor the replay log commits if allocation fails.
      Block candidate = blocks_[first];
      apply_pair(candidate, local(first, a), local(first, b), op);
      auto factors = refactor(std::move(candidate));
      record(gate);
      blocks_[first] = std::move(factors.front());
      for (std::size_t i = 1; i < factors.size(); ++i)
        blocks_.push_back(std::move(factors[i])); // capacity reserved to n_
      refresh_owners();
      ++gates_;
      return;
    }

    // Only simplify exact eigenvectors in the represented floating-point
    // state. Numerically near-eigenstates must NOT be silently approximated.
    const int control_z = eigen(blocks_[first], local(first, a), 'Z');
    const int target_axis = eigen(blocks_[second], local(second, b),
                                  op == Operation::CX ? 'X' : 'Z');
    if (control_z != 0) {
      record(gate);
      if (control_z == -1) apply_single(blocks_[second], local(second, b),
                                       op == Operation::CX ? Operation::X : Operation::Z);
      ++gates_;
      return;
    }
    if (target_axis != 0) {
      record(gate);
      if (target_axis == -1)
        apply_single(blocks_[first], local(first, a), Operation::Z);
      ++gates_;
      return;
    }

    const auto width = blocks_[first].ids.size() + blocks_[second].ids.size();
    if (width > max_block_) {
      // Fallback uses the ORIGINAL canonical Reduced algorithm, with all
      // gates replayed from the same |0...0> initial state.
      std::unique_ptr<Reduced> next;
      bool imported = false;
#ifdef REVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT
      // Only a true product is imported: the pinned backend has no general
      // entangled-block import API. Keep replay as a compatibility fallback
      // if the product's chosen basis exceeds the requested component budget.
      if (blocks_.size() == n_) {
        std::vector<Reduced::ProductPair> product(n_);
        for (const auto& block : blocks_)
          product[block.ids[0]] = {block.psi[0], block.psi[1]};
        try {
          next = std::make_unique<Reduced>(Reduced::from_product(product, max_components_));
          apply_reduced(*next, gate);
          imported = true;
        } catch (const std::length_error&) {
          next.reset(); // A different canonical replay basis may fit the cap.
        } catch (const std::runtime_error&) {
          next.reset(); // The gate itself may have exceeded the component cap.
        }
      }
#endif
      std::size_t replayed = 0;
      if (!next) {
        next = std::make_unique<Reduced>(n_, max_components_);
        for (const auto& old : history_) { apply_reduced(*next, old); ++replayed; }
        apply_reduced(*next, gate);
      }
      reduced_ = std::move(next);
      imported_product_ = imported;
      replayed_gates_ = replayed;
      // clear() alone retains old capacities; release the obsolete containers.
      std::vector<Gate>().swap(history_);
      std::vector<Block>().swap(blocks_);
      std::vector<std::size_t>().swap(owner_);
      ++gates_;
      return;
    }

    // Construct the tensor product before mutating the existing partition.
    Block merged;
    merged.ids = blocks_[first].ids;
    merged.ids.insert(merged.ids.end(), blocks_[second].ids.begin(),
                      blocks_[second].ids.end());
    // Remember the merge tree's cuts, including partitions with no singleton
    // factor (for example, a product of two Bell pairs).
    merged.cuts = blocks_[first].cuts;
    const auto shift = blocks_[first].ids.size();
    for (const auto cut : blocks_[second].cuts) merged.cuts.push_back(cut << shift);
    merged.cuts.push_back((std::size_t{1} << shift) - 1);
    const auto left = blocks_[first].psi.size();
    const auto right = blocks_[second].psi.size();
    merged.psi.resize(left * right);
    for (std::size_t j = 0; j < right; ++j)
      for (std::size_t i = 0; i < left; ++i)
        merged.psi[i | (j << blocks_[first].ids.size())] =
            blocks_[first].psi[i] * blocks_[second].psi[j];
    apply_pair(merged, local(first, a),
               blocks_[first].ids.size() + local(second, b), op);
    record(gate);
    blocks_[first] = std::move(merged);
    blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(second));
    refresh_owners();
    ++gates_;
  }

  std::size_t qubits() const { return n_; }
  std::size_t gate_count() const { return gates_; }
  std::size_t history_size() const { return history_.size(); }
  std::size_t history_capacity_bytes() const { return history_.capacity() * sizeof(Gate); }
  std::size_t replayed_gate_count() const { return replayed_gates_; }
  bool imported_product() const { return imported_product_; }
  bool uses_original_reduced() const { return static_cast<bool>(reduced_); }
  std::size_t block_count() const { return blocks_.size(); }
  std::size_t max_block_width() const {
    std::size_t width = 0;
    for (const auto& block : blocks_) width = std::max(width, block.ids.size());
    return width;
  }
  std::size_t factor_amplitudes() const {
    std::size_t count = 0;
    for (const auto& block : blocks_) count += block.psi.size();
    return count;
  }
  std::size_t component_count() const {
    return reduced_ ? reduced_->component_count() : 0;
  }
  std::size_t estimated_payload_bytes() const {
    if (reduced_) return reduced_->logical_payload_bytes();
    std::size_t bytes = owner_.capacity() * sizeof(std::size_t)
                      + history_.capacity() * sizeof(Gate)
                      + blocks_.capacity() * sizeof(Block);
    for (const auto& block : blocks_)
      bytes += block.ids.capacity() * sizeof(std::size_t)
            + block.psi.capacity() * sizeof(Complex)
            + block.cuts.capacity() * sizeof(std::size_t);
    return bytes;
  }
  double expectation(char axis, std::size_t q) const {
    check(q);
    if (reduced_) return reduced_->pauli_expectation(axis, q);
    if (axis != 'Z' && axis != 'X')
      throw std::invalid_argument("factorized expectation supports Z or X");
    const auto idx = owner_[q];
    double out = expectation_local(blocks_[idx], local(idx, q), axis);
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
      if (i == idx) continue;
      double norm = 0.;
      for (auto value : blocks_[i].psi) norm += std::norm(value);
      out *= norm;
    }
    return out;
  }
  double norm() const {
    if (reduced_) return reduced_->sparse_norm();
    double result = 1.;
    for (const auto& block : blocks_) {
      double local_norm = 0.;
      for (Complex c : block.psi) local_norm += std::norm(c);
      result *= local_norm;
    }
    return result;
  }
  std::vector<Complex> dense() const {
    if (n_ > 20) throw std::runtime_error("dense oracle limited to 20 qubits");
    if (reduced_) return reduced_->dense_from_reduced();
    std::vector<Complex> out(std::size_t{1} << n_, Complex{1., 0.});
    for (std::size_t bits = 0; bits < out.size(); ++bits) {
      for (const auto& block : blocks_) {
        std::size_t local_bits = 0;
        for (std::size_t p = 0; p < block.ids.size(); ++p)
          if ((bits >> block.ids[p]) & 1u) local_bits |= std::size_t{1} << p;
        out[bits] *= block.psi[local_bits];
      }
    }
    return out;
  }

 private:
  struct Block {
    std::vector<std::size_t> ids;
    std::vector<Complex> psi;
    std::vector<std::size_t> cuts;
  };

  static std::size_t gather(std::size_t value, std::size_t mask) {
    std::size_t out = 0;
    for (std::size_t bit = 1; mask; bit <<= 1) {
      const auto lowest = mask & (~mask + 1);
      if (value & lowest) out |= bit;
      mask ^= lowest;
    }
    return out;
  }

  static std::vector<std::size_t> indices(std::size_t mask) {
    std::vector<std::size_t> out{0};
    while (mask) {
      const auto bit = mask & (~mask + 1);
      const auto size = out.size();
      out.resize(size * 2);
      for (std::size_t i = 0; i < size; ++i) out[size + i] = out[i] | bit;
      mask ^= bit;
    }
    return out;
  }

  // A conservative rank-one certificate. No tolerance and no squared residual:
  // every reconstructed complex amplitude must equal the stored double value.
  // This can miss separability through rounding, but cannot prune a residual.
  static bool split(const Block& block, std::size_t mask, std::size_t pivot, const std::vector<std::size_t>& witnesses, Block& a, Block& b) {
    const auto full = (std::size_t{1} << block.ids.size()) - 1;
    std::size_t width = 0;
    for (auto bits = mask; bits; bits &= bits - 1) ++width;
    // Normalize the smaller factor. In particular, peeling a definite qubit
    // from a large GHZ block must not normalize the entire remaining vector.
    if (width > block.ids.size() - width) mask = full ^ mask;
    const auto other = full ^ mask;
    // Cheap necessary rank-one checks. Failure only skips a candidate; full
    // reconstruction below is still required before accepting any split.
    for (const auto sample : witnesses) {
      const auto cross_a = (sample & mask) | (pivot & other);
      const auto cross_b = (pivot & mask) | (sample & other);
      if (block.psi[sample] * block.psi[pivot] != block.psi[cross_a] * block.psi[cross_b])
        return false;
    }
    for (std::size_t bit = 0; bit < block.ids.size(); ++bit)
      ((mask >> bit) & 1 ? a.ids : b.ids).push_back(block.ids[bit]);
    a.psi.resize(std::size_t{1} << a.ids.size());
    b.psi.resize(std::size_t{1} << b.ids.size());
    const auto ai = indices(mask), bi = indices(other);
    const auto row = pivot & mask, column = pivot & other;
    double row_norm = 0.;
    for (std::size_t i = 0; i < a.psi.size(); ++i) {
      a.psi[i] = block.psi[ai[i] | column];
      row_norm = std::hypot(row_norm, std::abs(a.psi[i]));
    }
    if (!(row_norm > 0.) || !std::isfinite(row_norm)) return false;
    for (auto& value : a.psi) value /= row_norm;
    const auto divisor = a.psi[gather(row, mask)];
    for (std::size_t j = 0; j < b.psi.size(); ++j)
      b.psi[j] = divisor == Complex{1., 0.} ? block.psi[row | bi[j]]
                 : block.psi[row | bi[j]] / divisor;
    for (std::size_t j = 0; j < b.psi.size(); ++j)
      for (std::size_t i = 0; i < a.psi.size(); ++i)
        if (a.psi[i] * b.psi[j] != block.psi[ai[i] | bi[j]])
          return false;
    for (const auto cut : block.cuts) {
      for (auto pair : {std::make_pair(&a, mask), std::make_pair(&b, other)}) {
        const auto projected = gather(cut & pair.second, pair.second);
        const auto limit = (std::size_t{1} << pair.first->ids.size()) - 1;
        if (projected && projected != limit &&
            std::find(pair.first->cuts.begin(), pair.first->cuts.end(), projected)
              == pair.first->cuts.end()) pair.first->cuts.push_back(projected);
      }
    }
    return true;
  }
  static std::vector<Block> refactor(Block block) {
    if (block.ids.size() < 2) {
      std::vector<Block> out;
      out.push_back(std::move(block));
      return out;
    }
    std::size_t pivot = 0, last = 0;
    std::vector<std::size_t> witnesses;
    witnesses.reserve(9);
    for (std::size_t i = 0; i < block.psi.size(); ++i) {
      if (std::abs(block.psi[i]) > std::abs(block.psi[pivot])) pivot = i;
      if (block.psi[i] != Complex{}) {
        if (witnesses.size() < 8) witnesses.push_back(i);
        last = i;
      }
    }
    witnesses.push_back(last);
    // Candidate count is bounded by singletons plus the merge tree, rather
    // than an exhaustive exponential search over all possible bipartitions.
    auto candidates = block.cuts;
    for (std::size_t bit = 0; bit < block.ids.size(); ++bit)
      candidates.push_back(std::size_t{1} << bit);
    for (const auto mask : candidates) {
      Block left, right;
      if (!split(block, mask, pivot, witnesses, left, right)) continue;
      auto out = refactor(std::move(left));
      auto rest = refactor(std::move(right));
      out.reserve(out.size() + rest.size());
      for (auto& factor : rest) out.push_back(std::move(factor));
      return out;
    }
    std::vector<Block> out;
    out.push_back(std::move(block));
    return out;
  }

  static constexpr double kEigenTolerance = 0.;
  std::size_t n_, max_block_, max_components_;
  std::size_t gates_ = 0, replayed_gates_ = 0;
  bool imported_product_ = false;
  std::vector<Block> blocks_;
  std::vector<std::size_t> owner_;
  std::vector<Gate> history_;
  std::unique_ptr<Reduced> reduced_;

  void record(Gate gate) {
    const bool pair = gate.operation == Operation::CX || gate.operation == Operation::CZ;
    if (!pair) gate.b = 0;
    const bool exact_inverse = pair || gate.operation == Operation::X || gate.operation == Operation::Z;
    if (exact_inverse && !history_.empty()) {
      const auto& previous = history_.back();
      if (previous.operation == gate.operation && previous.a == gate.a && previous.b == gate.b) {
        history_.pop_back();
        return;
      }
    }
    history_.push_back(gate);
  }

  void check(std::size_t q) const {
    if (q >= n_) throw std::out_of_range("qubit out of bounds");
  }
  std::size_t local(std::size_t group, std::size_t global) const {
    const auto& ids = blocks_[group].ids;
    const auto it = std::find(ids.begin(), ids.end(), global);
    if (it == ids.end()) throw std::logic_error("invalid adaptive owner map");
    return static_cast<std::size_t>(it - ids.begin());
  }
  void refresh_owners() {
    for (std::size_t i = 0; i < blocks_.size(); ++i)
      for (std::size_t q : blocks_[i].ids) owner_[q] = i;
  }
  static void apply_single(Block& block, std::size_t bit, Operation op) {
    const std::size_t mask = std::size_t{1} << bit;
    const double inv = 1. / std::sqrt(2.);
    const Complex t = std::polar(1., 0.785398163397448309616);
    for (std::size_t i = 0; i < block.psi.size(); ++i) {
      if (i & mask) continue;
      Complex& a = block.psi[i];
      Complex& b = block.psi[i | mask];
      switch (op) {
        case Operation::H: {
          const Complex left = a, right = b;
          a = (left + right) * inv;
          b = (left - right) * inv;
          break;
        }
        case Operation::X: std::swap(a, b); break;
        case Operation::Z: b = -b; break;
        case Operation::S: b *= Complex{0., 1.}; break;
        case Operation::T: b *= t; break;
        case Operation::TDag: b *= std::conj(t); break;
        default: throw std::logic_error("two-qubit operation passed as single");
      }
    }
  }
  static void apply_pair(Block& block, std::size_t control,
                         std::size_t target, Operation op) {
    const auto c = std::size_t{1} << control;
    const auto t = std::size_t{1} << target;
    for (std::size_t i = 0; i < block.psi.size(); ++i) {
      if (!(i & c)) continue;
      if (op == Operation::CX) {
        if (!(i & t)) std::swap(block.psi[i], block.psi[i | t]);
      } else if (op == Operation::CZ) {
        if (i & t) block.psi[i] = -block.psi[i];
      } else throw std::logic_error("single operation passed as pair");
    }
  }
  static double expectation_local(const Block& block, std::size_t bit, char axis) {
    const auto mask = std::size_t{1} << bit;
    double total = 0.;
    if (axis == 'Z') {
      for (std::size_t i = 0; i < block.psi.size(); ++i)
        total += (i & mask ? -1. : 1.) * std::norm(block.psi[i]);
    } else {
      for (std::size_t i = 0; i < block.psi.size(); ++i)
        if (!(i & mask))
          total += 2. * std::real(std::conj(block.psi[i]) * block.psi[i | mask]);
    }
    return total;
  }
  // Eigenstate detection by a zero vector residual, not an expectation
  // threshold. Nearby nonzero amplitudes are retained by merging instead.
  static int eigen(const Block& block, std::size_t bit, char axis) {
    const std::size_t mask = std::size_t{1} << bit;
    double norm_sq = 0., plus = 0., minus = 0.;
    if (axis == 'Z') {
      for (std::size_t i = 0; i < block.psi.size(); ++i) {
        const auto value = block.psi[i];
        norm_sq += std::norm(value);
        if (i & mask) { if (value != Complex{}) plus = 1.; }
        else { if (value != Complex{}) minus = 1.; }
      }
      if (plus <= kEigenTolerance * kEigenTolerance * norm_sq) return +1;
      if (minus <= kEigenTolerance * kEigenTolerance * norm_sq) return -1;
    } else {
      for (std::size_t i = 0; i < block.psi.size(); ++i) {
        if (i & mask) continue;
        const Complex a = block.psi[i], b = block.psi[i | mask];
        norm_sq += std::norm(a) + std::norm(b);
        if (a != b) plus = 1.;
        if (a != -b) minus = 1.;
      }
      if (plus <= kEigenTolerance * kEigenTolerance * norm_sq) return +1;
      if (minus <= kEigenTolerance * kEigenTolerance * norm_sq) return -1;
    }
    return 0;
  }
  static void apply_reduced(Reduced& q, const Gate& gate) {
    switch (gate.operation) {
      case Operation::H: q.hadamard(gate.a); break;
      case Operation::X: q.pauli_x(gate.a); break;
      case Operation::Z: q.pauli_z(gate.a); break;
      case Operation::S: q.phase_s(gate.a); break;
      case Operation::T: q.phase_t(gate.a); break;
      case Operation::TDag: q.phase_t_dagger(gate.a); break;
      case Operation::CX: q.cnot(gate.a, gate.b); break;
      case Operation::CZ: q.controlled_z(gate.a, gate.b); break;
    }
  }
};

}  // namespace reverse_pc
