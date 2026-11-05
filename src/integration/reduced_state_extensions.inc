// Included inside the pinned Reduced class by the hash-checked build extension.
public:
static ReducedExactTRepresentation from_stabilizer(const CompactCore& state,
                                                   std::size_t cap = 65536) {
  ReducedExactTRepresentation out(state.qubits(), cap);
  out.base_ = state;

  return out;
}
static ReducedExactTRepresentation from_amplitudes(
    const std::vector<std::complex<double>>& amplitudes, std::size_t n,
    std::size_t cap = 65536) {
  if (n > 14 || amplitudes.size() != (std::size_t{1} << n))
    throw std::invalid_argument("local amplitude import limited to 14 qubits");
  ReducedExactTRepresentation out(n, cap);
  out.components_.clear();
  for (std::size_t i = 0; i < amplitudes.size(); ++i) {
    const auto v = amplitudes[i];
    if (!std::isfinite(v.real()) || !std::isfinite(v.imag()))
      throw std::invalid_argument("invalid amplitude");
    if (v == std::complex<double>{}) continue;
    if (out.components_.size() == cap)
      throw std::length_error("amplitude import component budget");
    PauliKey key;
    key.x[0] = i;
    out.components_.emplace(key, v);
  }
  if (!(out.sparse_norm() > 0.) || !std::isfinite(out.sparse_norm()))
    throw std::invalid_argument("zero amplitude state");
  return out;
}
static ReducedExactTRepresentation tensor_state(
    const ReducedExactTRepresentation& a, const ReducedExactTRepresentation& b,
    std::size_t cap = 65536) {
  if (a.n_ + b.n_ > kMaxQubits)
    throw std::length_error("Reduced tensor width budget");
  if (a.components_.size() > cap / b.components_.size())
    throw std::length_error("Reduced tensor component budget");
  auto out =
      from_stabilizer(CompactCore::tensor_product(a.base_, b.base_), cap);
  out.components_.clear();
  out.components_.reserve(a.components_.size() * b.components_.size());
  for (const auto& l : a.components_)
    for (const auto& r : b.components_) {
      auto key = l.first;
      for (std::size_t q = 0; q < b.n_; ++q) {
        if (word_test(r.first.x, q)) word_set(key.x, a.n_ + q);
        if (word_test(r.first.z, q)) word_set(key.z, a.n_ + q);
      }
      out.components_.emplace(key, l.second * r.second);
    }
  if (b.t_count_ > std::numeric_limits<std::size_t>::max() - a.t_count_)
    throw std::overflow_error("Reduced T counter overflow");
  out.t_count_ = a.t_count_ + b.t_count_;
  return out;
}
// Clifford conjugation preserves orthogonality of the current Pauli cosets.
// Canonicalization is deferred to non-Clifford expansion/observation rather
// than rebuilding an echelon basis for every Clifford instruction.
void clifford_current(ExactExperimentOp op, std::size_t a, std::size_t b = 0,
                      bool skip_unchanged_cosets = true) {
  check(a);
  if (op == ExactExperimentOp::CX || op == ExactExperimentOp::CZ) {
    check(b);
    if (a == b) throw std::invalid_argument("equal pair operands");
  }
  ExactExperimentGate gate{op, narrow(a), narrow(b)};
  auto next_base = base_;
  update_base(next_base, gate);
  bool changes = !skip_unchanged_cosets;
  if (!changes) {
    // Certify commutation with every stored Pauli, including phase changes.
    // The tableau still changes: only the coefficient map can be preserved.
    for (const auto& entry : components_) {
      const bool xa = word_test(entry.first.x, a),
                 za = word_test(entry.first.z, a);
      switch (op) {
        case ExactExperimentOp::H:
          changes = xa || za;
          break;
        case ExactExperimentOp::S:
        case ExactExperimentOp::Z:
          changes = xa;
          break;
        case ExactExperimentOp::X:
          changes = za;
          break;
        case ExactExperimentOp::CX:
          changes = xa || word_test(entry.first.z, b);
          break;
        case ExactExperimentOp::CZ:
          changes = xa || word_test(entry.first.x, b);
          break;
        default:
          changes = true;
      }
      if (changes) break;
    }
  }
  ComponentMap next;
  if (changes) {
    next.reserve(components_.size());
    for (const auto& entry : components_) {
      auto p = conjugate({entry.first.x, entry.first.z, 0}, gate);
      next.emplace(PauliKey{p.x, p.z}, entry.second * phase_unit(p.phase));
    }
  }
  auto next_cache = std::make_shared<IntegrationBasisCache>();
  base_ = std::move(next_base);
  if (changes) components_ = std::move(next);
  integration_basis_ = std::move(next_cache);
  circuit_.clear();
  base_circuit_.clear();
  phase_angles_.clear();
  reference_product_.clear();
}
void pauli_phase_current(const WordArray& x, const WordArray& z, unsigned phase,
                         double theta) {
  if (t_count_ == std::numeric_limits<std::size_t>::max())
    throw std::overflow_error("Reduced T counter overflow");
  if (!std::isfinite(theta)) throw std::invalid_argument("nonfinite rotation");
  validate_integration_pauli(x, z, phase);
  const auto& basis = integration_basis();
  const auto e = std::polar(1., theta),
             alpha = (std::complex<double>{1., 0.} + e) * .5,
             beta = (std::complex<double>{1., 0.} - e) * .5;
  ComponentMap next;
  next.reserve(components_.size() * 2);
  auto add = [&](Pauli p, std::complex<double> value) {
    if (value == std::complex<double>{}) return;
    p = canonicalize(p, basis);
    value *= phase_unit(p.phase);
    auto key = PauliKey{p.x, p.z};
    auto it = next.find(key);
    if (it == next.end())
      next.emplace(key, value);
    else {
      it->second += value;
      if (it->second == std::complex<double>{}) next.erase(it);
    }
  };
  for (const auto& entry : components_) {
    Pauli p{entry.first.x, entry.first.z, 0};
    add(p, alpha * entry.second);
    add(multiply({x, z, static_cast<std::uint8_t>(phase & 3)}, p),
        beta * entry.second);
  }
  if (next.empty() || next.size() > max_components_)
    throw std::length_error("Reduced non-Clifford component budget");
  components_ = std::move(next);
  ++t_count_;
  circuit_.clear();
  base_circuit_.clear();
  phase_angles_.clear();
  reference_product_.clear();
}
std::complex<double> expectation_current(const WordArray& x, const WordArray& z,
                                         unsigned phase = 0) const {
  validate_integration_pauli(x, z, phase);
  const auto& basis = integration_basis();
  const auto current = integration_canonical(basis);
  std::complex<double> total{}, correction{};
  for (const auto& entry : integration_sorted(current)) {
    auto acted =
        canonicalize(multiply({x, z, static_cast<std::uint8_t>(phase & 3)},
                              {entry.first.x, entry.first.z, 0}),
                     basis);
    auto it = current.find({acted.x, acted.z});
    if (it != current.end()) {
      const auto term =
          std::conj(it->second) * entry.second * phase_unit(acted.phase) -
          correction;
      const auto next = total + term;
      correction = (next - total) - term;
      total = next;
    }
  }
  return total;
}
std::array<long double, 2> born_pair_current(const WordArray& x,
                                             const WordArray& z,
                                             unsigned phase = 0) const {
  validate_integration_pauli(x, z, phase);
  const auto& basis = integration_basis();
  const auto current = integration_canonical(basis);
  std::array<long double, 2> weights{};
  for (unsigned branch = 0; branch < 2; ++branch) {
    ComponentMap projected;
    for (const auto& e : integration_sorted(current)) {
      projected[e.first] += e.second * .5;
      auto p =
          canonicalize(multiply({x, z, static_cast<std::uint8_t>(phase & 3)},
                                {e.first.x, e.first.z, 0}),
                       basis);
      projected[{p.x, p.z}] +=
          e.second * (branch ? -.5 : .5) * phase_unit(p.phase);
    }
    for (const auto& e : integration_sorted(projected)) {
      long double a = e.second.real(), b = e.second.imag();
      weights[branch] += a * a + b * b;
    }
  }
  const auto total = weights[0] + weights[1];
  if (!(total > 0.) || !std::isfinite(total))
    throw std::runtime_error("invalid Born norm");
  weights[0] /= total;
  weights[1] /= total;
  return weights;
}
double norm_current() const {
  auto current = integration_canonical(integration_basis());
  double sum = 0., correction = 0.;
  for (const auto& e : integration_sorted(current)) {
    double term = std::norm(e.second) - correction;
    double next = sum + term;
    correction = (next - sum) - term;
    sum = next;
  }
  return sum;
}
int eigen_current(const WordArray& x, const WordArray& z,
                  unsigned phase = 0) const {
  validate_integration_pauli(x, z, phase);
  const auto& basis = integration_basis();
  const auto current = integration_canonical(basis);
  bool plus = true, minus = true;
  for (const auto& e : current) {
    auto p = canonicalize(multiply({x, z, static_cast<std::uint8_t>(phase & 3)},
                                   {e.first.x, e.first.z, 0}),
                          basis);
    auto it = current.find({p.x, p.z});
    auto value = e.second * phase_unit(p.phase);
    if (it == current.end()) return 0;
    plus = plus && (value == it->second);
    minus = minus && (value == -it->second);
  }
  return plus ? 1 : minus ? -1 : 0;
}
double project_current(const WordArray& x, const WordArray& z, unsigned phase,
                       bool one) {
  validate_integration_pauli(x, z, phase);
  const auto& basis = integration_basis();
  const auto initial = norm_current();
  ComponentMap next;
  auto add = [&](Pauli p, std::complex<double> value) {
    p = canonicalize(p, basis);
    next[{p.x, p.z}] += value * phase_unit(p.phase);
  };
  for (const auto& e : components_) {
    Pauli p{e.first.x, e.first.z, 0};
    add(p, e.second * .5);
    add(multiply({x, z, static_cast<std::uint8_t>(phase & 3)}, p),
        e.second * (one ? -.5 : .5));
  }
  for (auto it = next.begin(); it != next.end();)
    if (it->second == std::complex<double>{})
      it = next.erase(it);
    else
      ++it;
  long double norm = 0.;
  for (const auto& e : integration_sorted(next)) {
    long double a = e.second.real(), b = e.second.imag();
    norm += a * a + b * b;
  }
  if (!(norm > 0.) || !std::isfinite(norm))
    throw std::runtime_error("impossible or underflowed projection");
  if (next.size() > max_components_)
    throw std::length_error("projection component budget");
  const double probability = static_cast<double>(norm / initial);
  if (probability == 0.)
    throw std::range_error("projection probability outside double range");
  for (auto& e : next)
    e.second = {static_cast<double>(e.second.real() / std::sqrt(norm)),
                static_cast<double>(e.second.imag() / std::sqrt(norm))};
  components_ = std::move(next);
  return probability;
}
std::string current_snapshot() const {
  reverse_pc::state_io::Writer w;
  w.data = "RDX1";
  w.u64(n_);
  w.u64(t_count_);
  const auto packed = base_.packed_state();
  for (auto v : packed.signs) w.byte(v);
  for (auto v : packed.x) w.u64(v);
  for (auto v : packed.z) w.u64(v);
  const auto canonical = integration_canonical(integration_basis());
  std::vector<std::pair<PauliKey, std::complex<double>>> rows(canonical.begin(),
                                                              canonical.end());
  std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
    return a.first.x != b.first.x ? a.first.x < b.first.x
                                  : a.first.z < b.first.z;
  });
  w.u64(rows.size());
  for (const auto& row : rows) {
    for (std::size_t i = 0; i < word_count(); ++i) w.u64(row.first.x[i]);
    for (std::size_t i = 0; i < word_count(); ++i) w.u64(row.first.z[i]);
    w.complex(row.second);
  }
  return w.finish();
}
static ReducedExactTRepresentation from_current_snapshot(
    const std::string& saved, std::size_t cap = 65536) {
  reverse_pc::state_io::Reader r(saved);
  r.require(4);
  if (r.data.substr(0, 4) != "RDX1")
    throw std::invalid_argument("Reduced snapshot version");
  r.at = 4;
  const auto n = r.u64();
  ReducedExactTRepresentation out(n, cap);
  out.t_count_ = r.u64();
  const auto words = out.word_count();
  r.require(n + 16 * n * words);
  CompactCore::PackedState packed;
  packed.qubits = n;
  packed.signs.resize(n);
  packed.x.resize(n * words);
  packed.z.resize(n * words);
  for (auto& v : packed.signs) v = r.byte();
  for (auto& v : packed.x) v = r.u64();
  for (auto& v : packed.z) v = r.u64();
  out.base_ = CompactCore::from_packed_state(packed);
  auto count = r.u64();
  if (count == 0 || count > cap)
    throw std::length_error("Reduced snapshot component budget");
  r.require(count * (16 * words + 16));
  out.components_.clear();
  const auto& basis = out.integration_basis();
  for (std::size_t i = 0; i < count; ++i) {
    PauliKey key;
    for (std::size_t k = 0; k < words; ++k) key.x[k] = r.u64();
    for (std::size_t k = 0; k < words; ++k) key.z[k] = r.u64();
    out.validate_integration_pauli(key.x, key.z, 0, false);
    auto value = r.complex();
    auto p = out.canonicalize({key.x, key.z, 0}, basis);
    if (p.x != key.x || p.z != key.z || p.phase != 0 ||
        value == std::complex<double>{} ||
        !out.components_.emplace(key, value).second)
      throw std::invalid_argument(
          "noncanonical/duplicate/zero Reduced component");
  }
  r.done();
  if (!(out.sparse_norm() > 0.) || !std::isfinite(out.sparse_norm()))
    throw std::invalid_argument("invalid Reduced norm");
  return out;
}
static std::vector<std::complex<double>> stabilizer_amplitudes(
    const CompactCore& core) {
  const auto n = core.qubits();
  if (n > 14)
    throw std::length_error("local stabilizer oracle limited to 14 qubits");
  auto helper = from_stabilizer(core);
  auto basis = helper.stabilizer_basis();
  std::size_t seed = 0, rank_x = 0;
  for (std::size_t i = 0; i < basis.pivots.size(); ++i) {
    if (basis.pivots[i] < n) {
      ++rank_x;
      continue;
    }
    const auto& row = basis.rows[i];
    unsigned parity = 0;
    for (std::size_t q = 0; q < n; ++q)
      if ((seed >> q) & 1) parity ^= word_test(row.z, q);
    const bool target = (row.phase & 2) != 0;
    if (parity != target) seed ^= std::size_t{1} << (basis.pivots[i] - n);
  }
  std::vector<std::complex<double>> v(std::size_t{1} << n);
  v[seed] = std::ldexp(1., -static_cast<int>(rank_x / 2));
  if (rank_x & 1) v[seed] /= std::sqrt(2.);
  for (std::size_t r = 0; r < rank_x; ++r) {
    const auto& p = basis.rows[r];
    auto old = v;
    for (std::size_t i = 0; i < v.size(); ++i) {
      const auto j = i ^ p.x[0];
      const auto sign = __builtin_parityll(p.z[0] & i) ? -1. : 1.;
      v[j] += old[i] * sign * phase_unit(p.phase);
    }
  }
  return v;
}
std::vector<std::complex<double>> dense_current() const {
  auto base = stabilizer_amplitudes(base_);
  std::vector<std::complex<double>> out(base.size());
  for (const auto& entry : components_)
    for (std::size_t i = 0; i < base.size(); ++i)
      out[i ^ entry.first.x[0]] +=
          base[i] * entry.second *
          (__builtin_parityll(entry.first.z[0] & i) ? -1. : 1.);
  return out;
}

private:
struct IntegrationBasisCache {
  std::once_flag once;
  std::unique_ptr<StabilizerBasis> basis;
};
std::shared_ptr<IntegrationBasisCache> integration_basis_ =
    std::make_shared<IntegrationBasisCache>();
const StabilizerBasis& integration_basis() const {
  std::call_once(integration_basis_->once, [&] {
    integration_basis_->basis =
        std::make_unique<StabilizerBasis>(stabilizer_basis());
  });
  return *integration_basis_->basis;
}
static std::vector<std::pair<PauliKey, std::complex<double>>>
integration_sorted(const ComponentMap& map) {
  std::vector<std::pair<PauliKey, std::complex<double>>> out(map.begin(),
                                                             map.end());
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    return a.first.x != b.first.x ? a.first.x < b.first.x
                                  : a.first.z < b.first.z;
  });
  return out;
}
ComponentMap integration_canonical(const StabilizerBasis& basis) const {
  ComponentMap out;
  out.reserve(components_.size());
  for (const auto& e : components_) {
    auto p = canonicalize({e.first.x, e.first.z, 0}, basis);
    auto key = PauliKey{p.x, p.z};
    out[key] += e.second * phase_unit(p.phase);
  }
  for (auto it = out.begin(); it != out.end();)
    if (it->second == std::complex<double>{})
      it = out.erase(it);
    else
      ++it;
  return out;
}
void validate_integration_pauli(const WordArray& x, const WordArray& z,
                                unsigned phase, bool hermitian = true) const {
  for (std::size_t q = n_; q < kMaxQubits + 24; ++q)
    if (word_test(x, q) || word_test(z, q))
      throw std::out_of_range("Pauli outside register");
  unsigned parity = 0;
  for (std::size_t w = 0; w < word_count(); ++w)
    parity ^= __builtin_parityll(x[w] & z[w]);
  if (hermitian && ((phase & 1) != parity))
    throw std::invalid_argument("non-Hermitian Pauli");
}
