#pragma once
#include <algorithm>
#include <future>

#include "mundo_x/quantum_runtime.hpp"
namespace reverse_pc {
// Read-only snapshots share their immutable component state. Each worker owns
// its Ether query diagnostics and bounded scratch. Results publish together.
inline std::vector<double> observe_parallel(
    const mundo_x::QuantumRuntime& runtime,
    const std::vector<mundo_x::InfiniteObservable>& observables,
    std::size_t workers = 1) {
  if (!workers || workers > 8 || observables.size() > 4096)
    throw std::length_error("parallel observation work budget");
  std::size_t terms = 0;
  for (const auto& observable : observables) {
    if (observable.terms.size() > 65536 - terms)
      throw std::length_error("parallel observation term budget");
    terms += observable.terms.size();
  }
  std::vector<double> values(observables.size());
  if (values.empty()) return values;
  workers = std::min(workers, values.size());
  std::vector<std::future<void>> pending;
  for (std::size_t worker = 0; worker < workers; ++worker) {
    auto core = runtime.core();
    pending.push_back(std::async(std::launch::async, [core = std::move(core),
                                                      &values, &observables,
                                                      worker, workers] {
      for (auto i = worker; i < observables.size(); i += workers)
        values[i] = core.expectation(observables[i].terms, observables[i].sign);
    }));
  }
  for (auto& future : pending) future.get();
  return values;
}
}  // namespace reverse_pc
