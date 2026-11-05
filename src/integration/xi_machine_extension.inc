// REVERSE-PC machine profile: interpreted by the original XI candidate owner.
// Included inside mundo_x::infinite_program_detail, after execute_line().
// The explicit shebang opt-in keeps legacy XI semantics byte-identical.
inline constexpr const char machine_header[] = "#!reverse-pc-machine-v1";
inline constexpr std::size_t machine_max_steps = 1000000;
inline constexpr std::size_t machine_register_count = 16;

struct MachineInstruction {
  std::vector<std::string> words;
  std::size_t line = 0;
};

inline unsigned machine_register(const std::string& name) {
  if (name.size() < 2 || (name.front() != 'r' && name.front() != 'R'))
    throw std::invalid_argument("expected register r0..r15");
  const auto value = decimal<unsigned>(name.substr(1), "register");
  if (value >= machine_register_count)
    throw std::out_of_range("register out of range (r0..r15)");
  return value;
}

inline std::int64_t machine_integer(const std::string& word) {
  if (word.empty()) throw std::invalid_argument("empty integer");
  const bool negative = word.front() == '-';
  const std::size_t start = (negative || word.front() == '+') ? 1 : 0;
  if (start == word.size()) throw std::invalid_argument("integer needs decimal digits");
  const auto max_positive = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  const auto limit = negative ? max_positive + 1U : max_positive;
  std::uint64_t value = 0;
  for (std::size_t i = start; i < word.size(); ++i) {
    const char ch = word[i];
    if (ch < '0' || ch > '9') throw std::invalid_argument("integer needs decimal digits");
    const auto digit = static_cast<unsigned>(ch - '0');
    if (value > (limit - digit) / 10U)
      throw std::out_of_range("signed 64-bit integer overflow");
    value = value * 10U + digit;
  }
  if (negative && value == max_positive + 1U)
    return std::numeric_limits<std::int64_t>::min();
  const auto signed_value = static_cast<std::int64_t>(value);
  return negative ? -signed_value : signed_value;
}

inline std::int64_t machine_value(const std::string& token,
                                 const std::array<std::int64_t, machine_register_count>& registers) {
  if (!token.empty() && (token.front() == 'r' || token.front() == 'R'))
    return registers[machine_register(token)];
  return machine_integer(token);
}

inline std::int64_t machine_arithmetic(const std::string& op, std::int64_t a,
                                       std::int64_t b) {
  using I = std::int64_t;
  const auto low = std::numeric_limits<I>::min();
  const auto high = std::numeric_limits<I>::max();
  if (op == "add") {
    if ((b > 0 && a > high - b) || (b < 0 && a < low - b))
      throw std::overflow_error("signed addition overflow");
    return a + b;
  }
  if (op == "sub") {
    if ((b < 0 && a > high + b) || (b > 0 && a < low + b))
      throw std::overflow_error("signed subtraction overflow");
    return a - b;
  }
  if (op == "mul") {
    if ((a == low && b == -1) || (b == low && a == -1))
      throw std::overflow_error("signed multiplication overflow");
    if (a > 0) {
      if ((b > 0 && a > high / b) || (b < 0 && b < low / a))
        throw std::overflow_error("signed multiplication overflow");
    } else if (a < 0) {
      if ((b > 0 && a < low / b) || (b < 0 && a < high / b))
        throw std::overflow_error("signed multiplication overflow");
    }
    return a * b;
  }
  if (b == 0) throw std::domain_error("division or remainder by zero");
  if (a == low && b == -1) throw std::overflow_error("signed division overflow");
  if (op == "div") return a / b;
  if (op == "mod") return a % b;
  throw std::invalid_argument("unsupported arithmetic opcode");
}

inline std::string machine_label(const std::string& token) {
  if (token.empty() ||
      !((token.front() >= 'a' && token.front() <= 'z') ||
        (token.front() >= 'A' && token.front() <= 'Z') || token.front() == '_'))
    throw std::invalid_argument("label must start with an ASCII letter or underscore");
  for (const char c : token)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_'))
      throw std::invalid_argument("invalid label character");
  return lower_ascii(token);
}

inline bool machine_is_jump(const std::string& name) {
  return name == "jmp" || name == "je" || name == "jne" ||
         name == "jlt" || name == "jle" || name == "jgt" || name == "jge";
}

inline std::string machine_substitute(const std::string& token,
                          const std::array<std::int64_t, machine_register_count>& registers) {
  if (token.size() > 1 && token[0] == '$')
    return std::to_string(registers[machine_register(token.substr(1))]);
  if (token.size() > 3 && token[1] == ':' && token[2] == '$' &&
      (token[0] == 'I' || token[0] == 'X' || token[0] == 'Y' || token[0] == 'Z'))
    return token.substr(0, 2) +
           std::to_string(registers[machine_register(token.substr(3))]);
  return token;
}

// CPU instructions operate on 16 ephemeral signed registers. InverseMemory
// retains durable bytes. Quantum gates retain the sole InfiniteCore authority.
inline ProgramResult run_machine(InfiniteCore& core, InverseMemory* memory,
                                 const std::string& program,
                                 bool require_matching_background = false,
                                 std::size_t max_steps = machine_max_steps) {
  if (program.size() > max_program_bytes)
    throw std::length_error("program exceeds 16 MiB transport budget");
  if (!max_steps) throw std::invalid_argument("machine step budget must be positive");
  std::vector<MachineInstruction> code;
  std::map<std::string, std::size_t> labels;
  std::size_t cursor = 0, source_line = 0;
  while (cursor < program.size()) {
    ++source_line;
    const auto newline = program.find('\n', cursor);
    const auto end = newline == std::string::npos ? program.size() : newline;
    if (end - cursor > max_line_bytes)
      throw std::length_error("line " + std::to_string(source_line) + ": over 1 MiB");
    auto words = tokens(program.substr(cursor, end - cursor));
    cursor = newline == std::string::npos ? program.size() : newline + 1;
    if (words.empty()) continue;
    if (code.size() >= max_commands)
      throw std::length_error("program exceeds 131072 commands");
    const auto cmd = lower_ascii(words.front());
    if (cmd == "label") {
      require_count(words, 2);
      const auto name = machine_label(words[1]);
      if (!labels.emplace(name, code.size()).second)
        throw std::invalid_argument("line " + std::to_string(source_line) + ": duplicate label " + name);
    }
    code.push_back({std::move(words), source_line});
  }
  for (const auto& inst : code) {
    const auto op = lower_ascii(inst.words.front());
    if (machine_is_jump(op)) {
      require_count(inst.words, 2);
      if (!labels.count(machine_label(inst.words[1])))
        throw std::invalid_argument("line " + std::to_string(inst.line) + ": unknown jump label");
    }
  }

  std::array<std::int64_t, machine_register_count> registers{};
  ProgramResult result;
  bool first_command = true, in_batch = false, comparison_valid = false;
  int comparison = 0;
  std::size_t pc = 0, steps = 0, address_bytes = 0;
  std::vector<InfiniteCore::Gate> batch;
  while (pc < code.size()) {
    const auto& inst = code[pc];
    try {
      if (++steps > max_steps)
        throw std::length_error("XI machine instruction-step budget exceeded");
      const auto& words = inst.words;
      const auto op = lower_ascii(words.front());
      auto output_line = [&](const std::string& output) {
        if (output.size() > max_response_bytes - result.output.size())
          throw std::length_error("machine output exceeds 16 MiB");
        result.output += output;
      };
      if (op == "background") {
        require_count(words, 2);
        if (!first_command || in_batch)
          throw std::invalid_argument("background must be the first command");
        const auto background = parse_background(words[1]);
        if (require_matching_background && background != core.background())
          throw std::invalid_argument("program background conflicts with --background");
        core = InfiniteCore(background, core.limits());
        result.explicit_mutation = true;
      } else if (op == "begin") {
        require_count(words, 1);
        if (in_batch) throw std::invalid_argument("nested batches are not supported");
        in_batch = true; batch.clear(); address_bytes = 0;
      } else if (op == "end") {
        require_count(words, 1);
        if (!in_batch) throw std::invalid_argument("end has no matching begin");
        core.apply(batch); batch.clear(); in_batch = false;
      } else if (in_batch) {
        if (!is_gate(op)) throw std::invalid_argument("a batch accepts gates only");
        if (batch.size() == max_batch_gates)
          throw std::length_error("batch exceeds 65536 gates");
        auto expanded = words;
        for (std::size_t i = 1; i < expanded.size(); ++i)
          expanded[i] = machine_substitute(expanded[i], registers);
        auto gate = parse_gate(expanded);
        gate.first = canonical(core, gate.first);
        if (!gate.second.empty()) gate.second = canonical(core, gate.second);
        const auto bytes = gate.first.size() + gate.second.size();
        if (bytes > max_batch_address_bytes - address_bytes)
          throw std::length_error("batch address text exceeds 16 MiB");
        batch.push_back(std::move(gate)); address_bytes += bytes;
      } else if (op == "label") {
        require_count(words, 2);
      } else if (op == "set" || op == "mov") {
        require_count(words, 3);
        registers[machine_register(words[1])] = machine_value(words[2], registers);
      } else if (op == "add" || op == "sub" || op == "mul" ||
                 op == "div" || op == "mod") {
        require_count(words, 3);
        const auto index = machine_register(words[1]);
        const auto new_value = machine_arithmetic(op, registers[index],
                                                  machine_value(words[2], registers));
        registers[index] = new_value;
      } else if (op == "cmp") {
        require_count(words, 3);
        const auto a = machine_value(words[1], registers);
        const auto b = machine_value(words[2], registers);
        comparison = a < b ? -1 : (a > b ? 1 : 0);
        comparison_valid = true;
      } else if (machine_is_jump(op)) {
        require_count(words, 2);
        if (op != "jmp" && !comparison_valid)
          throw std::invalid_argument("conditional jump without cmp");
        const bool take = op == "jmp" ||
          (op == "je" && comparison == 0) ||
          (op == "jne" && comparison != 0) ||
          (op == "jlt" && comparison < 0) ||
          (op == "jle" && comparison <= 0) ||
          (op == "jgt" && comparison > 0) ||
          (op == "jge" && comparison >= 0);
        if (take) { pc = labels.at(machine_label(words[1])); first_command = false; continue; }
      } else if (op == "load8") {
        require_count(words, 3);
        if (!memory) throw std::invalid_argument("Inverse Memory is not enabled");
        const auto index = machine_register(words[1]);
        const auto read = memory->read(machine_substitute(words[2], registers), core);
        if (!read.deterministic)
          throw std::runtime_error("load8 requires a deterministic byte; quantum handle " + read.handle);
        registers[index] = read.value;
      } else if (op == "store8") {
        require_count(words, 3);
        if (!memory) throw std::invalid_argument("Inverse Memory is not enabled");
        const auto address = machine_substitute(words[1], registers);
        const auto value = machine_value(words[2], registers);
        if (value < 0 || value > 255)
          throw std::out_of_range("store8 value must be in 0..255");
        memory->store(address, static_cast<std::uint8_t>(value), core);
      } else if (op == "print") {
        require_count(words, 2);
        const auto index = machine_register(words[1]);
        output_line("{\"type\":\"register\",\"register\":\"r" + std::to_string(index) +
                    "\",\"value\":" + std::to_string(registers[index]) + "}\n");
      } else if (op == "halt") {
        require_count(words, 1);
        break;
      } else {
        auto expanded = words;
        for (std::size_t i = 1; i < expanded.size(); ++i)
          expanded[i] = machine_substitute(expanded[i], registers);
        std::ostringstream line;
        line.imbue(std::locale::classic());
        line.exceptions(std::ios::badbit | std::ios::failbit);
        line << std::setprecision(17);
        execute_line(expanded, core, memory, line, result.explicit_mutation);
        output_line(line.str());
      }
      first_command = false;
      ++pc;
    } catch (const std::exception& error) {
      throw std::runtime_error("line " + std::to_string(inst.line) + ": " + error.what());
    }
  }
  if (in_batch) throw std::invalid_argument("unterminated batch: expected end");
  return result;
}
