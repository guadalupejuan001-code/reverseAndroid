#include "mundo_x/quantum_runtime.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using Runtime = mundo_x::QuantumRuntime;
void need(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void rejects_unchanged(Runtime& runtime, const std::string& program,
                       const std::string& fragment) {
  const auto original_core = runtime.core_snapshot();
  const auto original_memory = runtime.memory_persistent_snapshot();
  bool rejected = false;
  try { (void)runtime.execute(program); }
  catch (const std::exception& error) {
    rejected = std::string(error.what()).find(fragment) != std::string::npos;
  }
  need(rejected, "expected failure was not reported");
  need(runtime.core_snapshot() == original_core, "failed machine program mutated core/RNG");
  need(runtime.memory_persistent_snapshot() == original_memory,
       "failed machine program mutated memory/revision");
}
}
int main() {
  try {
    constexpr const char* banner = "#!reverse-pc-machine-v1\n";
    Runtime runtime;
    const std::string gcd = std::string(banner) +
      "set r0 1071\nset r1 462\nlabel euclid\n"
      "cmp r1 0\nje finished\nmov r2 r0\nmod r2 r1\n"
      "mov r0 r1\nmov r1 r2\njmp euclid\n"
      "label finished\nprint r0\n"
      "set r3 -12\ndiv r3 2\nprint r3\n"
      "set r4 1000\nlabel down\nsub r4 1\ncmp r4 0\njgt down\n"
      "print r4\nset r5 12\nmul r5 -3\nadd r5 36\nprint r5\n";
    const auto output = runtime.execute(gcd);
    need(output.find("\"register\":\"r0\",\"value\":21") != std::string::npos,
         "Euclid result wrong");
    need(output.find("\"register\":\"r3\",\"value\":-6") != std::string::npos,
         "signed division wrong");
    need(output.find("\"register\":\"r4\",\"value\":0") != std::string::npos,
         "reverse loop wrong");
    need(output.find("\"register\":\"r5\",\"value\":0") != std::string::npos,
         "signed multiplication wrong");
    const auto before_classical = runtime.core_snapshot();
    const auto before_memory = runtime.memory_persistent_snapshot();
    need(before_classical == runtime.core_snapshot() &&
         before_memory == runtime.memory_persistent_snapshot(), "pure register run changed persistent state");

    const std::string quantum = std::string(banner) +
      "set r0 0\nset r1 1\nset r2 128\n"
      "h $r0\nlabel entangle\ncx $r0 $r1\n"
      "add r0 1\nadd r1 1\ncmp r1 r2\njlt entangle\n"
      "t 0\ntdg 127\nexpect Z:0 Z:$r0\n"
      "set r3 42\nstore8 999999999999999999999999999999999 r3\n"
      "load8 r4 999999999999999999999999999999999\nprint r4\n"
      "qstats\nstats\n";
    const auto results = runtime.execute(quantum);
    need(results.find("\"register\":\"r4\",\"value\":42") != std::string::npos,
         "Inverse Ether Memory register loop failed");
    need(results.find("\"quantum_stats\"") != std::string::npos,
         "integrated quantum computation did not run");
    need(runtime.core().stats().active_qubits == 128,
         "register-addressed circuit did not create 128 qubits");
    need(std::abs(runtime.core().expectation({{"0",'Z'}, {"127",'Z'}}) - 1.0) < 1e-9,
         "dynamic GHZ correlations incorrect");
    const auto stored = runtime.memory().read("999999999999999999999999999999999",runtime.core());
    need(stored.deterministic && stored.value == 42, "long-address store did not survive");
    Runtime restored;
    restored.restore(runtime.core_snapshot(), runtime.memory_persistent_snapshot());
    need(restored.core_snapshot() == runtime.core_snapshot() &&
         restored.memory_persistent_snapshot() == runtime.memory_persistent_snapshot(),
         "machine state failed to persist through existing RVPC2 payloads");

    const std::string failure_header = std::string(banner) +
      "set r0 99\nstore8 5 r0\nx 200\n";
    rejects_unchanged(runtime, failure_header + "not_an_instruction\n", "unknown command");
    rejects_unchanged(runtime, failure_header + "set r1 9223372036854775807\nadd r1 1\n", "overflow");
    rejects_unchanged(runtime, failure_header + "set r1 -9223372036854775808\ndiv r1 -1\n", "overflow");
    rejects_unchanged(runtime, failure_header + "set r1 0\ndiv r1 0\n", "zero");
    rejects_unchanged(runtime, failure_header + "jmp missing\n", "unknown jump label");
    rejects_unchanged(runtime, failure_header + "label again\nlabel again\n", "duplicate label");
    rejects_unchanged(runtime, failure_header + "store8 6 256\n", "0..255");
    rejects_unchanged(runtime, failure_header + "set r16 1\n", "out of range");
    rejects_unchanged(runtime, failure_header + "jlt nowhere\n", "unknown jump label");
    rejects_unchanged(runtime, failure_header + "label present\njlt present\n", "without cmp");
    rejects_unchanged(runtime, failure_header + "begin\nset r2 3\nend\n", "batch accepts gates only");
    rejects_unchanged(runtime, failure_header + "begin\nx 5\n", "unterminated batch");
    rejects_unchanged(runtime, failure_header + "set r1 -1\nx $r1\n", "decimal");

    Runtime quantum_memory;
    quantum_memory.execute("h 0\nmem_bind 7 0\n");
    rejects_unchanged(quantum_memory, std::string(banner) +
      "set r0 9\nstore8 5 r0\nload8 r1 7\n", "quantum handle");
    // A new opt-in profile cannot silently extend native XI.
    rejects_unchanged(quantum_memory, "set r0 5\n", "unknown command");

    // The compiler's step guard must reject a permanently cycling program.
    mundo_x::InfiniteCore local_core;
    mundo_x::InverseMemory local_memory;
    bool bounded = false;
    try { (void)mundo_x::infinite_program_detail::run_machine(
      local_core, &local_memory,
      std::string(banner) + "label forever\njmp forever\n", false, 32); }
    catch (const std::exception& error) {
      bounded = std::string(error.what()).find("instruction-step budget") != std::string::npos;
    }
    need(bounded, "infinite loop was not bounded");
    Runtime legacy_a, legacy_b;
    const std::string native_program =
      "mem_store 9 7\nh 0\nt 0\nmem_read 9\nexpect Z:0\nqstats\n";
    const auto native_a = legacy_a.execute(native_program);
    const auto native_b = legacy_b.execute(native_program);
    need(native_a == native_b && legacy_a.core_snapshot() == legacy_b.core_snapshot() &&
         legacy_a.memory_persistent_snapshot() == legacy_b.memory_persistent_snapshot(),
         "unmarked XI changed behavior");
    std::cout << "{\"suite\":\"xi_programmable_machine\","
              << "\"gcd\":21,\"dynamic_qubits\":128,\"long_memory\":42,"
              << "\"rollback\":true,\"bounded_loops\":true,"
              << "\"legacy_xi_preserved\":true}\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr << "xi_machine_test: " << error.what() << '\n';
    return 1;
  }
}
