#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>

#include "mundo_x/pauli_observer.hpp"
#include "quantum_test_support.hpp"
using namespace qtest;
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point t) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
unsigned entropy_half(const mundo_x::CompactCore& c) {
  const auto n = c.qubits(), a = n / 2, b = n - a, words = (2 * b + 63) / 64;
  std::vector<std::vector<std::uint64_t>> rows(
      n, std::vector<std::uint64_t>(words));
  for (unsigned r = 0; r < n; ++r) {
    auto p = c.generator_packed(r);
    for (unsigned q = a; q < n; ++q) {
      if ((p.x[q / 64] >> (q % 64)) & 1)
        rows[r][(q - a) / 64] |= std::uint64_t{1} << ((q - a) % 64);
      if ((p.z[q / 64] >> (q % 64)) & 1)
        rows[r][(b + q - a) / 64] |= std::uint64_t{1} << ((b + q - a) % 64);
    }
  }
  unsigned rank = 0;
  for (unsigned col = 0; col < 2 * b && rank < n; ++col) {
    unsigned pivot = rank;
    while (pivot < n && !((rows[pivot][col / 64] >> (col % 64)) & 1)) ++pivot;
    if (pivot == n) continue;
    std::swap(rows[rank], rows[pivot]);
    for (unsigned r = rank + 1; r < n; ++r)
      if ((rows[r][col / 64] >> (col % 64)) & 1)
        for (unsigned w = 0; w < words; ++w) rows[r][w] ^= rows[rank][w];
    ++rank;
  }
  return rank - b;
}
int main(int argc, char** argv) {
  try {
    if (argc < 6 || argc > 8)
      throw std::invalid_argument(
          "usage: N FAMILY DEPTH COMPONENT_CAP FAST [FACTOR [DIRECT_ETHER]]");
    unsigned n = std::stoul(argv[1]), depth = std::stoul(argv[3]),
             cap = std::stoul(argv[4]);
    std::string family = argv[2];
    bool fast = std::stoi(argv[5]) != 0;
    need(n == 128 || n == 256 || n == 512 || n == 1000,
         "required register sizes");
    Runtime::CoreLimits limits;
#ifndef REVERSE_PC_LEGACY_LARGE
    limits.max_reduced_components = cap;
    limits.fast_reduced_clifford = fast;
    limits.max_dense_block = 8;
    limits.factor_pair_shortcuts = argc >= 7 ? std::stoi(argv[6]) != 0 : true;
    limits.direct_ether_phases = argc == 8 ? std::stoi(argv[7]) != 0 : true;
#else
    need(family == "clifford",
         "previous runtime cannot execute non-Clifford XI");
#endif
    Runtime r(Runtime::Background::Zero, limits, {});
    std::vector<Gate> prefix, tail;
    std::set<std::pair<unsigned, unsigned>> edges;
    auto edge = [&](unsigned a, unsigned b) {
      if (a == b) return;
      auto e = std::minmax(a, b);
      auto pair = std::make_pair(e.first, e.second);
      if (!edges.insert(pair).second) edges.erase(pair);
      prefix.push_back({'D', a, b});
    };
    if (family == "random") {
      std::mt19937 random(20261025);
      for (unsigned i = 0; i < 3 * n * depth; ++i) {
        const unsigned a = random() % n, b = (a + 1 + random() % (n - 1)) % n;
        prefix.push_back({"HSCD"[random() % 4], a, b});
      }
    } else if (family == "clifford") {
      for (unsigned layer = 0; layer < depth; ++layer) {
        for (unsigned q = 0; q < n; ++q) prefix.push_back({'H', q});
        for (unsigned q = 0; q < n; ++q) prefix.push_back({'S', q});
        unsigned stride = 1u << (layer % 6);
        for (unsigned q = 0; q < n; ++q)
          if (q != (q + stride) % n)
            prefix.push_back({'C', q, (q + stride) % n});
      }
    } else if (family == "ghz") {
      prefix.push_back({'H', 0});
      for (unsigned q = 1; q < n; ++q) prefix.push_back({'C', q - 1, q});
      for (unsigned j = 0; j < depth; ++j) {
        std::vector<unsigned> far;
        for (unsigned q : {0u, 127u, 255u, 511u, 999u})
          if (q < n) far.push_back(q);
        tail.push_back({'T', far[j % far.size()]});
      }
    } else if (family == "separable") {
      for (unsigned q = 0; q < n; ++q) prefix.push_back({'H', q});
      for (unsigned layer = 0; layer < depth; ++layer)
        for (unsigned q = 0; q < n; ++q) {
          tail.push_back({'T', q});
          tail.push_back({'S', q});
          tail.push_back({'X', q});
        }
    } else if (family == "redundant") {
      for (unsigned q = 0; q < n; ++q) prefix.push_back({q % 2 ? 'H' : 'X', q});
      for (unsigned layer = 0; layer < depth; ++layer) {
        for (unsigned q = 1; q < n; q += 2) {
          tail.push_back({'T', q});
          tail.push_back({'S', q});
          tail.push_back({'C', 0, q});
        }
        tail.push_back({'X', 0});
      }
    } else if (family == "partial") {
      for (unsigned start = 0; start < n; start += 8) {
        prefix.push_back({'H', start});
        for (unsigned q = start + 1; q < std::min(n, start + 8); ++q)
          prefix.push_back({'C', q - 1, q});
        tail.push_back({'T', start});
      }
      for (unsigned layer = 0; layer < depth; ++layer) {
        tail.push_back({'C', 0, n - 1});
        tail.push_back({'C', 0, n - 1});
      }
    } else if (family == "mixed" || family == "adversarial" ||
               family == "ether") {
      for (unsigned q = 0; q < n; ++q) prefix.push_back({'H', q});
      for (unsigned q = 1; q < n; ++q) edge(q - 1, q);
      for (unsigned q = 0; q < n / 2; ++q) edge(q, q + n / 2);
      if (family == "mixed")
        for (unsigned q = 0; q < 6; ++q) tail.push_back({'T', q});
      if (family == "ether")
        for (unsigned q = 0; q < depth; ++q) tail.push_back({'T', 1 + q});
    } else
      throw std::invalid_argument("unknown circuit family");
    mundo_x::CompactCore oracle(n);
    for (auto g : prefix) compact(oracle, g);
    unsigned entropy = entropy_half(oracle);
    auto begin = Clock::now();
    execute(r, prefix);
    double prefix_ms = ms(begin);
#ifndef REVERSE_PC_LEGACY_LARGE
    if (family == "ether") r.execute("cz_neighbors\nh_all\n");
#endif
    begin = Clock::now();
    unsigned accepted = 0;
    bool rejected = false;
    std::vector<double> t_latencies;
    double rejection_ms = 0.;
    if (family == "adversarial") {
      for (unsigned q = 0; q < depth; ++q) {
        auto before = r.core_snapshot();
        auto memory = r.memory_snapshot();
        auto t = Clock::now();
        try {
          r.execute(text({'T', q}));
          tail.push_back({'T', q});
          ++accepted;
          t_latencies.push_back(ms(t));
        } catch (const std::exception& e) {
          need(
              std::string(e.what()).find(
                  "Reduced non-Clifford component budget") != std::string::npos,
              std::string("unexpected adversarial failure: ") + e.what());
          rejection_ms = ms(t);
          rejected = true;
          need(before == r.core_snapshot() && memory == r.memory_snapshot(),
               "adversarial overflow corrupted state");
          break;
        }
      }
    } else {
      const auto before_tail = r.core_snapshot(),
                 before_memory = r.memory_snapshot();
      try {
        execute(r, tail);
      } catch (const std::exception& e) {
#ifndef REVERSE_PC_LEGACY_LARGE
        if (family == "redundant" && !limits.factor_pair_shortcuts) {
          need(before_tail == r.core_snapshot() &&
                   before_memory == r.memory_snapshot(),
               "redundant reference rejection not atomic");
          std::cout << "{\"qubits\":" << n
                    << ",\"family\":\"redundant\",\"reference_budget_rejected_"
                       "atomically\":true}\n";
          return 0;
        }
#endif
        throw;
      }
    }
    double noncliff_ms = ms(begin), post_ms = 0.;
    if (family == "mixed") {
      std::vector<Gate> after;
      for (unsigned j = 0; j < depth; ++j) {
        unsigned a = (j * 127u) % n, b = (a + 1 + (j * 31u) % (n - 1)) % n;
        after.push_back({'H', a});
        after.push_back({'C', a, b});
        after.push_back({'S', b});
      }
      begin = Clock::now();
      execute(r, after);
      post_ms = ms(begin);
      tail.insert(tail.end(), after.begin(), after.end());
    }
    auto execution_memory = []() {
      std::array<unsigned, 2> out{};
      std::ifstream status("/proc/self/status");
      std::string key;
      while (status >> key) {
        if (key == "VmRSS:")
          status >> out[0];
        else if (key == "VmHWM:")
          status >> out[1];
        std::string rest;
        std::getline(status, rest);
      }
      return out;
    };
    auto execution_rss = execution_memory();
    auto observation_start = Clock::now();
    double error = 0.;
    if (family == "clifford" || family == "random") {
      const mundo_x_bridge::PauliObserver observer(oracle);
      for (unsigned q : {0u, 127u, 255u, 511u, 999u})
        if (q < n)
          for (char axis : {'X', 'Y', 'Z'}) {
            auto p = mundo_x_bridge::Pauli::parse(std::string(n, 'I'), n);
            auto mask = std::uint64_t{1} << (q % 64);
            if (axis == 'X' || axis == 'Y') p.x[q / 64] |= mask;
            if (axis == 'Z' || axis == 'Y') p.z[q / 64] |= mask;
            if (axis == 'Y') p.phase = 1;
            error = std::max(
                error,
                std::abs(r.core().expectation({{std::to_string(q), axis}}) -
                         observer.expectation(p)));
          }
      // Current stabilizers independently certify the distributed Clifford
      // state.
      for (unsigned row = 0; row < std::min(n, 12u); ++row) {
        auto p = oracle.generator_string(row);
        std::vector<mundo_x::InfiniteCore::Term> terms;
        for (unsigned q = 0; q < n; ++q)
          if (p[q + 1] != 'I') terms.push_back({std::to_string(q), p[q + 1]});
        error = std::max(
            error,
            std::abs(r.core().expectation(terms, p[0] == '-' ? -1 : 1) - 1.));
      }
    } else if (family == "ghz") {
      error = std::abs(
          r.core().expectation({{"0", 'Z'}, {std::to_string(n - 1), 'Z'}}) -
          1.);
      std::vector<mundo_x::InfiniteCore::Term> all;
      for (unsigned q = 0; q < n; ++q) all.push_back({std::to_string(q), 'X'});
      error = std::max(error, std::abs(r.core().expectation(all) -
                                       std::cos(depth * std::acos(-1.) / 4)));
    } else if (family == "ether") {
      // U = H_all CZ_line transforms graph stabilizers to Z_q X_partner.
      // T affects the X factor only when that partner receives a T.
      for (unsigned i = 1; i <= depth; ++i)
        for (unsigned q : {i, i + n / 2}) {
          const auto partner = (q + n / 2) % n;
          const double expected =
              partner >= 1 && partner <= depth ? 1. / std::sqrt(2.) : 1.;
          error = std::max(
              error,
              std::abs(r.core().expectation({{std::to_string(q), 'Z'},
                                             {std::to_string(partner), 'X'}}) -
                       expected));
        }
    } else if (family == "adversarial") {
      for (unsigned q = 0; q < std::min(n, accepted + 1); ++q) {
        std::vector<mundo_x::InfiniteCore::Term> terms = {
            {std::to_string(q), 'X'}};
        for (auto e : edges) {
          if (e.first == q) terms.push_back({std::to_string(e.second), 'Z'});
          if (e.second == q) terms.push_back({std::to_string(e.first), 'Z'});
        }
        const auto expected = q < accepted ? 1. / std::sqrt(2.) : 1.;
        error =
            std::max(error, std::abs(r.core().expectation(terms) - expected));
      }
    }
    if (family == "redundant" || family == "separable") {
      std::vector<std::vector<C>> local(n, std::vector<C>{1., 0.});
      auto apply_local = [&](Gate g) {
        if (g.op == 'C') {
          need(local[g.a][0] == C{} || local[g.a][1] == C{},
               "product oracle control");
          if (local[g.a][0] == C{}) dense(local[g.b], {'X', 0});
        } else
          dense(local[g.a], {g.op, 0});
      };
      for (auto g : prefix) apply_local(g);
      for (auto g : tail) apply_local(g);
      for (unsigned q : {0u, 127u, 255u, 511u, 999u})
        if (q < n)
          for (char axis : {'X', 'Y', 'Z'})
            error = std::max(
                error,
                std::abs(r.core().expectation({{std::to_string(q), axis}}) -
                         expectation(local[q], {{"0", axis}})));
    }
    need(error < 2e-8, "large circuit invariant");
#ifndef REVERSE_PC_LEGACY_LARGE
    const auto qs = r.core().quantum_stats();
    const auto observation_ms = ms(observation_start);
    need(std::abs(qs.norm - 1.) < 2e-8, "large norm");
    r.execute("mem_store 91 37\nmem_bind 92 0\nseed 1337\n");
    auto save_start = Clock::now();
    const auto core = r.core_snapshot(),
               memory = r.memory_persistent_snapshot();
    const auto save_ms = ms(save_start);
    const bool timing_only = std::getenv("REVERSE_PC_TIMING_ONLY") != nullptr;
    double restore_ms = 0., inverse_ms = 0.;
    if (!timing_only) {
      auto revision = r.core().stats().revision;
      auto memory_revision = r.memory().stats().revision;
      Runtime restored(Runtime::Background::Zero, limits, {});
      begin = Clock::now();
      restored.restore(core, memory);
      restore_ms = ms(begin);
      need(restored.core_snapshot() == core &&
               restored.memory_persistent_snapshot() == memory &&
               restored.core().stats().revision == revision &&
               restored.memory().stats().revision == memory_revision,
           "large snapshot/revision restore");
      if (family == "ether") {
        bool projection_rejected = false;
        try {
          restored.execute("mem_store 91 99\nseed 7\nmeasure 0\n");
        } catch (const std::exception& e) {
          projection_rejected =
              std::string(e.what()).find("projection component budget") !=
              std::string::npos;
        }
        need(projection_rejected && restored.core_snapshot() == core &&
                 restored.memory_persistent_snapshot() == memory,
             "over-budget measurement must roll back state, memory and RNG");
      }
      auto output = r.execute("h_all\nx 0\nx 0\nh_all\n");
      (void)output;
      restored.execute("h_all\nx 0\nx 0\nh_all\n");
      need(r.core_snapshot() == restored.core_snapshot(),
           "continued Ether operations diverged after restore");
      auto rng_r = r, rng_restored = restored;
      const auto sample = family == "ether" ? "measure 1\n" : "measure 0\n";
      need(rng_r.execute(sample) == rng_restored.execute(sample),
           "large restored RNG");
      begin = Clock::now();
      execute(restored, inverse(tail));
      if (family == "ether") restored.execute("h_all\ncz_neighbors\n");
      execute(restored, inverse(prefix));
      inverse_ms = ms(begin);
      for (unsigned q : {0u, 127u, 255u, 511u, 999u})
        if (q < n)
          error = std::max(
              error,
              std::abs(restored.core().probability_one(std::to_string(q))));
      need(error < 2e-8, "large reversible circuit");
    }
#else
    const auto observation_ms = ms(observation_start);
    std::size_t restore_ms = 0, inverse_ms = 0, save_ms = 0;
    std::string core = r.core_snapshot();
#endif
    std::ifstream status("/proc/self/status");
    std::string key;
    unsigned rss = 0, peak = 0;
    while (status >> key) {
      if (key == "VmRSS:")
        status >> rss;
      else if (key == "VmHWM:")
        status >> peak;
      std::string rest;
      std::getline(status, rest);
    }
    std::cout << std::setprecision(16) << "{\"qubits\":" << n
              << ",\"family\":\"" << family << "\",\"depth\":" << depth
              << ",\"component_budget\":" << cap
              << ",\"fast\":" << (fast ? "true" : "false")
              << ",\"prefix_gates\":" << prefix.size()
              << ",\"tail_gates\":" << tail.size()
              << ",\"prefix_ms\":" << prefix_ms
              << ",\"noncliff_ms\":" << noncliff_ms
              << ",\"post_clifford_ms\":" << post_ms
              << ",\"restore_ms\":" << restore_ms
              << ",\"inverse_ms\":" << inverse_ms
              << ",\"execution_rss_kib\":" << execution_rss[0]
              << ",\"execution_peak_rss_kib\":" << execution_rss[1]
              << ",\"save_ms\":" << save_ms
              << ",\"observation_ms\":" << observation_ms
              << ",\"rss_kib\":" << rss << ",\"peak_rss_kib\":" << peak
              << ",\"stabilizer_entropy_half_before_T\":" << entropy
              << ",\"max_error\":" << error
              << ",\"T_accepted_before_budget\":" << accepted
              << ",\"T_rejection_ms\":" << rejection_ms
              << ",\"budget_rejected_atomically\":"
              << (rejected ? "true" : "false")
              << ",\"core_snapshot_bytes\":" << core.size()
#ifndef REVERSE_PC_LEGACY_LARGE
              << ",\"clifford_components\":" << qs.clifford
              << ",\"adaptive_components\":" << qs.adaptive
              << ",\"reduced_components\":" << qs.reduced
              << ",\"coefficients\":" << qs.coefficients
              << ",\"factor_amplitudes\":" << qs.amplitudes
              << ",\"payload_bytes_estimate\":" << qs.payload_bytes
              << ",\"norm\":" << qs.norm
#endif
              << ",\"T_gate_ms\":[";
    for (unsigned i = 0; i < t_latencies.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << t_latencies[i];
    }
    std::cout << "]}\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
