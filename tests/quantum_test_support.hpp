#pragma once
#include <complex>
#include <random>
#include <sstream>

#include "mundo_x/quantum_runtime.hpp"
namespace qtest {
using Runtime = mundo_x::QuantumRuntime;
using C = std::complex<double>;
struct Gate {
  char op;
  unsigned a, b = 0;
};
inline void need(bool yes, const std::string& message) {
  if (!yes) throw std::runtime_error(message);
}
inline std::string text(Gate g) {
  std::string op = g.op == 'C'   ? "cx"
                   : g.op == 'D' ? "cz"
                   : g.op == 'A' ? "tdg"
                                 : std::string(1, char(g.op + ('a' - 'A')));
  return op + " " + std::to_string(g.a) +
         (g.op == 'C' || g.op == 'D' ? " " + std::to_string(g.b) : "") + "\n";
}
inline std::vector<Gate> inverse(const std::vector<Gate>& gs) {
  std::vector<Gate> out;
  for (auto it = gs.rbegin(); it != gs.rend(); ++it) {
    auto g = *it;
    if (g.op == 'T')
      g.op = 'A';
    else if (g.op == 'A')
      g.op = 'T';
    else if (g.op == 'S') {
      out.push_back(g);
      out.push_back(g);
    }
    out.push_back(g);
  }
  return out;
}
inline void execute(Runtime& r, const std::vector<Gate>& gates) {
  std::string buffer;
  unsigned count = 0;
  for (auto g : gates) {
    buffer += text(g);
    if (++count == 32768) {
      r.execute(buffer);
      buffer.clear();
      count = 0;
    }
  }
  if (!buffer.empty()) r.execute(buffer);
}
inline void compact(mundo_x::CompactCore& c, Gate g) {
  switch (g.op) {
    case 'H':
      c.hadamard(g.a);
      break;
    case 'S':
      c.phase_s(g.a);
      break;
    case 'X':
      c.pauli_x(g.a);
      break;
    case 'Z':
      c.pauli_z(g.a);
      break;
    case 'C':
      c.cnot(g.a, g.b);
      break;
    case 'D':
      c.controlled_z(g.a, g.b);
      break;
    default:
      throw std::logic_error("non-Clifford independent oracle");
  }
}
inline void dense(std::vector<C>& v, Gate g) {
  const auto a = std::size_t{1} << g.a, b = std::size_t{1} << g.b;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (g.op == 'C') {
      if ((i & a) && !(i & b)) std::swap(v[i], v[i | b]);
    } else if (g.op == 'D') {
      if ((i & a) && (i & b)) v[i] = -v[i];
    } else if (!(i & a)) {
      auto x = v[i], y = v[i | a];
      if (g.op == 'H') {
        v[i] = (x + y) / std::sqrt(2.);
        v[i | a] = (x - y) / std::sqrt(2.);
      } else if (g.op == 'X')
        std::swap(v[i], v[i | a]);
      else if (g.op == 'Z')
        v[i | a] = -y;
      else if (g.op == 'S')
        v[i | a] = C{0, 1} * y;
      else if (g.op == 'T' || g.op == 'A')
        v[i | a] =
            std::polar(1., (g.op == 'T' ? 1 : -1) * std::acos(-1.) / 4) * y;
    }
  }
}
inline double expectation(
    const std::vector<C>& v,
    const std::vector<mundo_x::InfiniteCore::Term>& terms) {
  C out{};
  for (std::size_t i = 0; i < v.size(); ++i) {
    auto j = i;
    C factor = 1.;
    for (const auto& t : terms) {
      auto q = std::stoul(t.address), mask = std::size_t{1} << q;
      if (t.axis == 'X')
        j ^= mask;
      else if (t.axis == 'Z') {
        if (i & mask) factor = -factor;
      } else if (t.axis == 'Y') {
        j ^= mask;
        factor *= i & mask ? C{0, -1} : C{0, 1};
      }
    }
    out += std::conj(v[j]) * factor * v[i];
  }
  return out.real();
}
}  // namespace qtest
