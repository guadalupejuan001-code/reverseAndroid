#include <iostream>

#include "quantum_test_support.hpp"
using namespace qtest;
int main() {
  try {
    std::mt19937 rng(20261025);
    double worst = 0.;
    unsigned cases = 0;
    for (unsigned dense_limit : {0u, 8u})
      for (unsigned trial = 0; trial < 100; ++trial) {
        unsigned n = 2 + trial % 7;
        Runtime::CoreLimits limits;
        limits.max_dense_block = dense_limit;
        Runtime r(Runtime::Background::Zero, limits, {});
        std::vector<C> oracle(std::size_t{1} << n);
        oracle[0] = 1.;
        std::vector<Gate> gates;
        for (unsigned j = 0; j < 60; ++j) {
          unsigned a = rng() % n, b = (a + 1 + rng() % (n - 1)) % n;
          Gate g{"HSXZCDTA"[rng() % 8], a, b};
          gates.push_back(g);
          r.execute(text(g));
          dense(oracle, g);
          if (j % 6 == 0)
            for (unsigned q = 0; q < n; ++q)
              for (char axis : {'X', 'Y', 'Z'}) {
                std::vector<mundo_x::InfiniteCore::Term> terms = {
                    {std::to_string(q), axis}};
                auto e = std::abs(r.core().expectation(terms) -
                                  expectation(oracle, terms));
                worst = std::max(worst, e);
                need(e < 2e-9, "integrated dense observable");
              }
          if (j == 30) {
            auto core = r.core_snapshot(), mem = r.memory_persistent_snapshot();
            Runtime copy(Runtime::Background::Zero, limits, {});
            copy.restore(core, mem);
            need(copy.core_snapshot() == core &&
                     copy.memory_persistent_snapshot() == mem,
                 "mixed-state snapshot not stable");
            r = std::move(copy);
          }
        }
        execute(r, inverse(gates));
        need(std::abs(r.core().quantum_stats().norm - 1.) < 1e-9,
             "inverse norm");
        for (unsigned q = 0; q < n; ++q)
          need(r.core().probability_one(std::to_string(q)) < 1e-9,
               "mixed inverse did not return zero");
        ++cases;
      }
    // Independent finite-window oracle for an infinite Ether frame. Four guard
    // qubits exceed the light cone of these global layers for queries q0..q3.
    for (unsigned trial = 0; trial < 32; ++trial) {
      constexpr unsigned width = 8;
      Runtime::CoreLimits frame_limits;
      frame_limits.max_dense_block = trial % 2 ? 8 : 0;
      frame_limits.direct_ether_phases = trial % 4 < 2;
      const auto background = trial & 4 ? Runtime::Background::BellPairs
                                        : Runtime::Background::Zero;
      Runtime r(background, frame_limits, {});
      std::vector<C> v(1u << width);
      v[0] = 1.;
      if (background == Runtime::Background::BellPairs)
        for (unsigned q = 0; q < width; q += 2) {
          dense(v, {'H', q});
          dense(v, {'C', q, q + 1});
        }
      for (unsigned q = 0; q < 4; ++q) {
        r.execute(text({'H', q}));
        dense(v, {'H', q});
      }
      r.execute("h_all\ncz_neighbors\n");
      for (unsigned q = 0; q < width; ++q) dense(v, {'H', q});
      for (unsigned q = 1; q < width; ++q) dense(v, {'D', q - 1, q});
      for (unsigned j = 0; j < 15; ++j) {
        unsigned a = rng() % 4, b = (a + 1 + rng() % 3) % 4;
        Gate g{"HSTACD"[rng() % 6], a, b};
        r.execute(text(g));
        dense(v, g);
      }
      for (unsigned q = 0; q < 4; ++q)
        for (char axis : {'X', 'Y', 'Z'}) {
          std::vector<mundo_x::InfiniteCore::Term> terms = {
              {std::to_string(q), axis}};
          double e =
              std::abs(r.core().expectation(terms) - expectation(v, terms));
          worst = std::max(worst, e);
          need(e < 2e-9, "Ether physical gate/pullback mismatch");
        }
      auto core = r.core_snapshot(), mem = r.memory_persistent_snapshot();
      Runtime restored(Runtime::Background::Zero, frame_limits, {});
      restored.restore(core, mem);
      need(restored.core_snapshot() == core, "framed snapshot unstable");
      const auto p = (1 - expectation(v, {{"1", 'Z'}})) * .5;
      if (p > 1e-10 && p < 1 - 1e-10) {
        auto out = restored.execute("postselect 1 1\n");
        (void)out;
        need(restored.core().probability_one("1") == 1.,
             "framed arbitrary Born projection");
      }
      ++cases;
    }
    using Reduced = mundo_x::ReducedExactTRepresentation;
    auto rare = Reduced::from_amplitudes({C{1., 0.}, C{1e-150, 0.}}, 1, 4);
    Reduced::WordArray rx{}, rz{};
    rz[0] = 1;
    auto tiny = rare.born_pair_current(rx, rz)[1];
    need(rare.eigen_current(rx, rz) == 0 &&
             std::abs(static_cast<double>(tiny) / 1e-300 - 1) < 1e-12,
         "tiny Born probability was rounded to determinism");
    auto selected = rare.project_current(rx, rz, 0, true);
    need(std::abs(selected / 1e-300 - 1) < 1e-12 &&
             rare.eigen_current(rx, rz) == -1 &&
             std::abs(rare.norm_current() - 1) < 1e-12,
         "rare branch projection precision");
    Runtime memory;
    memory.execute("mem_store 9 23\nmem_bind 4 0\nh 0\nt 0\nh 0\nseed 912\n");
    need(!memory.memory().read("4", memory.core()).deterministic,
         "non-1/2 memory byte incorrectly classical");
    auto before = memory.core_snapshot(),
         bytes = memory.memory_persistent_snapshot();
    bool rejected = false;
    try {
      memory.execute("mem_store 9 99\nmeasure 0\nmissing_command\n");
    } catch (const std::exception&) {
      rejected = true;
    }
    need(rejected && before == memory.core_snapshot() &&
             bytes == memory.memory_persistent_snapshot(),
         "failed program corrupted RNG/core/memory");
    Runtime restored;
    restored.restore(before, bytes);
    need(restored.memory().stats().revision == memory.memory().stats().revision,
         "memory revision lost on restore");
    need(restored.execute("measure 0\n") == memory.execute("measure 0\n"),
         "RNG not restored");
    need(restored.core_snapshot() == memory.core_snapshot(),
         "continued state after restore");
    Runtime::CoreLimits cap;
    cap.max_reduced_components = 4;
    cap.max_dense_block = 0;
    Runtime bounded(Runtime::Background::Zero, cap, {});
    std::vector<Gate> graph;
    for (unsigned q = 0; q < 16; ++q) graph.push_back({'H', q});
    for (unsigned q = 1; q < 16; ++q) graph.push_back({'D', q - 1, q});
    execute(bounded, graph);
    bounded.execute("t 0\nt 1\n");
    before = bounded.core_snapshot();
    bytes = bounded.memory_persistent_snapshot();
    rejected = false;
    try {
      bounded.execute("mem_store 7 88\nt 2\n");
    } catch (const std::exception&) {
      rejected = true;
    }
    need(rejected && before == bounded.core_snapshot() &&
             bytes == bounded.memory_persistent_snapshot(),
         "budget failure damaged mixed authority");
    for (unsigned i = 0; i < 40; ++i) {
      auto damaged = before;
      damaged[5 + rng() % (damaged.size() - 5)] ^= 1;
      rejected = false;
      try {
        bounded.restore(damaged, bytes);
      } catch (const std::exception&) {
        rejected = true;
      }
      need(rejected && before == bounded.core_snapshot(),
           "damaged mixed snapshot accepted");
    }
    std::cout << std::setprecision(16)
              << "{\"suite\":\"integrated_quantum_correctness\",\"random_"
                 "circuits\":200,\"Ether_window_cases\":32,\"seed\":20261025,"
                 "\"max_observable_error\":"
              << worst
              << ",\"memory_revision_RNG_rollback\":true,\"budget_rejection_"
                 "atomic\":true}\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
