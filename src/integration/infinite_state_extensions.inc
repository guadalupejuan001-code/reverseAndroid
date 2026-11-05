public:
static constexpr std::size_t max_integrated_snapshot_bytes = 64 * 1024 * 1024;
struct QuantumStats {
  std::size_t clifford = 0, adaptive = 0, reduced = 0, coefficients = 0,
              amplitudes = 0, t_gates = 0, payload_bytes = 0;
  double norm = 1.;
};
QuantumStats quantum_stats() const {
  QuantumStats out;
  for (const auto& c : state_->components) {
    if (c->core.is_clifford())
      ++out.clifford;
    else if (c->core.is_adaptive())
      ++out.adaptive;
    else
      ++out.reduced;
    out.coefficients += c->core.reduced_components();
    out.amplitudes += c->core.amplitudes();
    out.t_gates += c->core.t_count();
    out.payload_bytes += c->core.state_bytes();
    out.norm *= c->core.norm();
  }
  return out;
}

private:
bool try_direct_phase(State& state, const EtherCore::Observable& pulled,
                      bool dagger) const {
  if (pulled.terms.empty()) return true;
  auto first = state.locations.find(pulled.terms.front().address);
  if (first == state.locations.end()) return false;
  const auto slot = first->second.component;
  const auto& before = state.components[slot]->core;
  if (before.is_adaptive() ||
      (before.is_clifford() && before.qubits() <= limits_.max_dense_block))
    return false;
  auto p = identity_pauli(before.qubits());
  for (const auto& term : pulled.terms) {
    auto it = state.locations.find(term.address);
    if (it == state.locations.end() || it->second.component != slot)
      return false;
    set_axis(p, it->second.qubit, term.axis);
  }
  const auto bytes = before.state_bytes();
  auto& core = mutable_component(state, slot).core;
  if (!core.phase_pauli(p, dagger != (pulled.sign < 0)))
    throw std::logic_error("direct phase dispatch changed representation");
  state.tableau_bytes =
      checked_add(state.tableau_bytes - bytes, core.state_bytes(),
                  "direct phase payload overflow");
  check_resources(state.locations.size(), core.qubits(), state.tableau_bytes);
  return true;
}
bool try_factor_pair(State& state, const Gate& gate) const {
  const auto a = state.locations.at(gate.first),
             b = state.locations.at(gate.second);
  if (a.component == b.component) return false;
  const auto& left = *state.components[a.component];
  const auto& right = *state.components[b.component];
  if (left.core.is_clifford() && right.core.is_clifford()) return false;
  auto p = identity_pauli(left.ids.size());
  set_axis(p, a.qubit, 'Z');
  int sign = left.core.eigen(p);
  if (sign) {
    if (sign < 0)
      apply_gate(state, {gate.op == 'C' ? 'X' : 'Z', gate.second, {}});
    return true;
  }
  p = identity_pauli(right.ids.size());
  set_axis(p, b.qubit, gate.op == 'C' ? 'X' : 'Z');
  sign = right.core.eigen(p);
  if (sign) {
    if (sign < 0) apply_gate(state, {'Z', gate.first, {}});
    return true;
  }
  return false;
}
bool has_non_clifford(const State& state) const {
  for (const auto& c : state.components)
    if (!c->core.is_clifford()) return true;
  return false;
}
void refresh_payload(State& state) const {
  std::size_t bytes = 0;
  for (const auto& c : state.components) {
    bytes =
        checked_add(bytes, c->core.state_bytes(), "quantum payload overflow");
    check_resources(state.locations.size(), c->ids.size(), bytes);
  }
  state.tableau_bytes = bytes;
}
static bool sample_one(PortableRng& rng, double probability, double zero) {
  return reverse_pc::sample_born_one(rng, probability, zero);
}
int pulled_eigen(const State& state,
                 const EtherCore::Observable& pulled) const {
  TermMap normalized;
  for (const auto& t : pulled.terms) normalized.emplace(t.address, t.axis);
  std::map<std::size_t, mundo_x_bridge::Pauli> queries;
  int sign = pulled.sign;
  for (const auto& t : normalized) {
    if (t.second == 'I') continue;
    auto found = state.locations.find(t.first);
    if (found != state.locations.end()) {
      const auto loc = found->second;
      auto it = queries.find(loc.component);
      if (it == queries.end())
        it =
            queries
                .emplace(
                    loc.component,
                    identity_pauli(state.components[loc.component]->ids.size()))
                .first;
      set_axis(it->second, loc.qubit, t.second);
    } else if (background_ == Background::Zero) {
      if (t.second != 'Z') sign = 0;
    } else {
      auto other = normalized.find(infinite_detail::partner_address(t.first));
      if (other == normalized.end() || other->second != t.second)
        sign = 0;
      else if (!infinite_detail::is_odd(t.first) && t.second == 'Y')
        sign = -sign;
    }
  }
  for (const auto& q : queries)
    sign *= state.components[q.first]->core.eigen(q.second);
  return sign;
}
double pulled_probability(const State& state,
                          const EtherCore::Observable& pulled,
                          bool one = true) const {
  TermMap normalized;
  for (const auto& t : pulled.terms) normalized.emplace(t.address, t.axis);
  if (!has_non_clifford(state)) {
    auto e = raw_expectation(state, normalized, pulled.sign);
    return (one ? 1. - e : 1. + e) * .5;
  }
  std::map<std::size_t, mundo_x_bridge::Pauli> queries;
  int sign = pulled.sign;
  for (const auto& t : normalized) {
    if (t.second == 'I') continue;
    auto found = state.locations.find(t.first);
    if (found != state.locations.end()) {
      const auto loc = found->second;
      auto it = queries.find(loc.component);
      if (it == queries.end())
        it =
            queries
                .emplace(
                    loc.component,
                    identity_pauli(state.components[loc.component]->ids.size()))
                .first;
      set_axis(it->second, loc.qubit, t.second);
    } else if (background_ == Background::Zero) {
      if (t.second != 'Z') sign = 0;
    } else {
      auto other = normalized.find(infinite_detail::partner_address(t.first));
      if (other == normalized.end() || other->second != t.second)
        sign = 0;
      else if (!infinite_detail::is_odd(t.first) && t.second == 'Y')
        sign = -sign;
    }
  }
  if (!sign) return .5;
  long double plus = sign == 1 ? 1 : 0, minus = sign == -1 ? 1 : 0;
  for (const auto& q : queries) {
    auto p = state.components[q.first]->core.born_pair(q.second);
    auto next_plus = plus * p[0] + minus * p[1],
         next_minus = plus * p[1] + minus * p[0];
    plus = next_plus;
    minus = next_minus;
  }
  return reverse_pc::QuantumBlock::represent_probability(one ? minus : plus);
}

public:
int z_determinism(const std::string& address) const {
  const auto id = canonical(address);
  if (ether_rule_count())
    return pulled_eigen(*state_, pull_pauli(*state_, {{id, 'Z'}}));
  auto it = state_->locations.find(id);
  return it == state_->locations.end()
             ? (background_ == Background::Zero ? 1 : 0)
             : state_->components[it->second.component]->core.z_eigen(
                   it->second.qubit);
}
double probability_zero(const std::string& address) const {
  const auto id = canonical(address);
  if (ether_rule_count())
    return pulled_probability(*state_, pull_pauli(*state_, {{id, 'Z'}}), false);
  auto it = state_->locations.find(id);
  return it == state_->locations.end()
             ? (background_ == Background::Zero ? 1. : .5)
             : state_->components[it->second.component]
                   ->core.probability_outcome_z(it->second.qubit, false);
}

private:
std::string integrated_snapshot() const {
  reverse_pc::state_io::Writer w;
  w.data = "MXI3:";
  w.byte(background_ == Background::Zero ? 0 : 1);
  w.u64(state_->revision);
  w.u64(state_->rng.position);
  for (auto word : state_->rng.words) w.u64(word);
  auto rules = state_->ether.rule_run();
  w.byte(static_cast<unsigned>(rules.first));
  w.u64(rules.count);
  w.u64(state_->components.size());
  for (const auto& c : state_->components) {
    w.u64(c->ids.size());
    for (const auto& id : c->ids) w.string(id);
    w.string(c->core.snapshot());
  }
  auto result = w.finish();
  if (result.size() > max_integrated_snapshot_bytes)
    throw std::length_error("integrated core snapshot budget exceeded");
  return result;
}
static InfiniteCore from_integrated_snapshot(const std::string& saved,
                                             const Limits& limits) {
  reverse_pc::state_io::Reader r(saved, max_integrated_snapshot_bytes);
  r.require(5);
  if (saved.substr(0, 5) != "MXI3:")
    throw std::invalid_argument("integrated core version");
  r.at = 5;
  auto bg = r.byte();
  if (bg > 1) throw std::invalid_argument("integrated background");
  InfiniteCore out(bg ? Background::BellPairs : Background::Zero, limits);
  auto& state = *out.state_;
  state.revision = r.u64();
  state.rng.position = r.u64();
  for (auto& word : state.rng.words) word = r.u64();
  state.rng.validate();
  auto first = r.byte();
  auto depth = r.u64();
  if (first > 1) throw std::invalid_argument("Ether frame kind");
  state.ether.restore_rule_run(static_cast<EtherCore::Rule>(first), depth,
                               limits.max_ether_rules);
  auto count = r.u64();
  if (count > limits.max_active_qubits)
    throw std::length_error("integrated component count budget");
  for (std::size_t index = 0; index < count; ++index) {
    auto n = r.u64();
    if (!n || n > limits.max_component_qubits ||
        n > limits.max_active_qubits - state.locations.size())
      throw std::length_error("integrated active/width budget");
    out.check_resources(checked_add(state.locations.size(), n,
                                    "restored active count overflow"),
                        n,
                        checked_add(state.tableau_bytes, tableau_bytes(n),
                                    "restored minimum payload overflow"));
    if (n > max_integrated_snapshot_bytes / 9)
      throw std::length_error("snapshot address count budget");
    r.require(n * 9);
    std::vector<std::string> ids;
    ids.reserve(n);
    for (std::size_t q = 0; q < n; ++q) {
      auto id = r.string(limits.max_address_digits);
      if (out.canonical(id) != id ||
          !state.locations.emplace(id, Location{index, q}).second)
        throw std::invalid_argument(
            "integrated address duplicate/noncanonical");
      state.address_bytes = checked_add(state.address_bytes, id.size(),
                                        "address payload overflow");
      ids.push_back(std::move(id));
    }
    auto core = reverse_pc::QuantumBlock::from_snapshot(
        r.string(max_integrated_snapshot_bytes), limits.max_reduced_components,
        limits.max_dense_block, limits.fast_reduced_clifford);
    if (core.qubits() != n)
      throw std::invalid_argument("integrated core width mismatch");
    state.components.push_back(
        std::make_shared<Component>(std::move(ids), std::move(core)));
    out.refresh_payload(state);
  }
  r.done();
  if (out.background_ == Background::BellPairs)
    for (const auto& item : state.locations) {
      auto partner =
          state.locations.find(infinite_detail::partner_address(item.first));
      if (partner == state.locations.end() ||
          partner->second.component != item.second.component)
        throw std::invalid_argument("integrated split Bell background");
    }
  return out;
}
