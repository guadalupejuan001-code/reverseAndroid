#include "integration/parallel_observation.hpp"

#include <chrono>
#include <fstream>
#include <iostream>

#include "quantum_test_support.hpp"
using namespace qtest;
int main(int argc, char** argv) {
  try {
    unsigned workers = argc > 1 ? std::stoul(argv[1]) : 4;
    constexpr unsigned n = 1000;
    Runtime r;
    std::vector<Gate> gs;
    for (unsigned q = 0; q < n; ++q) gs.push_back({'H', q});
    for (unsigned q = 1; q < n; ++q) gs.push_back({'D', q - 1, q});
    for (unsigned q = 0; q < 10; ++q) gs.push_back({'T', q});
    execute(r, gs);
    r.execute("h 0\nh 0\n");
    auto snapshot = r.core_snapshot();
    std::vector<mundo_x::InfiniteObservable> queries;
    for (unsigned i = 0; i < 1024; ++i) {
      unsigned q = i % n;
      mundo_x::InfiniteObservable o;
      o.terms.push_back({std::to_string(q), 'X'});
      if (q) o.terms.push_back({std::to_string(q - 1), 'Z'});
      if (q + 1 < n) o.terms.push_back({std::to_string(q + 1), 'Z'});
      queries.push_back(std::move(o));
    }
    auto warm = reverse_pc::observe_parallel(r, {queries.front()}, 1);
    (void)warm;
    auto start = std::chrono::steady_clock::now();
    auto answers = reverse_pc::observe_parallel(r, queries, workers);
    double elapsed = std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - start)
                         .count();
    unsigned rss = 0, peak = 0;
    std::ifstream status("/proc/self/status");
    std::string key;
    while (status >> key) {
      if (key == "VmRSS:") status >> rss;
      if (key == "VmHWM:") status >> peak;
      std::string rest;
      std::getline(status, rest);
    }
    double worst = 0, checksum = 0;
    for (unsigned i = 0; i < answers.size(); ++i) {
      auto expected = i % n < 10 ? 1 / std::sqrt(2.) : 1.;
      worst = std::max(worst, std::abs(answers[i] - expected));
      checksum += answers[i];
    }
    need(worst < 2e-9 && snapshot == r.core_snapshot(),
         "parallel observation mutated state/wrong answer");
    auto rejected = [&](const auto& qs, unsigned threads) {
      try {
        reverse_pc::observe_parallel(r, qs, threads);
      } catch (const std::exception&) {
        need(snapshot == r.core_snapshot(), "failed observation changed state");
        return;
      }
      throw std::logic_error("invalid observation accepted");
    };
    rejected(queries, 0);
    rejected(queries, 9);
    auto invalid = queries;
    invalid.front().terms.front().address = "-1";
    rejected(invalid, workers);
    rejected(std::vector<mundo_x::InfiniteObservable>(4097), 1);
    mundo_x::InfiniteObservable too_many;
    too_many.terms.resize(65537);
    rejected(std::vector<mundo_x::InfiniteObservable>{too_many}, 1);
    std::cout
        << std::setprecision(16)
        << "{\"qubits\":1000,\"components\":1024,\"queries\":1024,\"workers\":"
        << workers << ",\"ms\":" << elapsed << ",\"checksum\":" << checksum
        << ",\"rss_kib\":" << rss << ",\"peak_rss_kib\":" << peak
        << ",\"max_error\":" << worst << "}\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
