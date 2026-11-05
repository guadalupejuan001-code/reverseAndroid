#pragma once
// Non-owning, read-only transport view. The input remains owned by the caller;
// restored state still belongs exclusively to the original QuantumRuntime.
#include <istream>
#include <streambuf>
#include <string_view>
namespace mundo_x::memory_detail {
class SnapshotViewStream final : public std::istream {
  class Buffer final : public std::streambuf {
   public:
    explicit Buffer(std::string_view text){
      // basic_streambuf uses mutable pointers for its get area. No put area or
      // writing operation is exposed, and formatted input only advances gptr.
      auto* first=const_cast<char*>(text.data());setg(first,first,first?first+text.size():first);
    }
  } buffer_;
 public:
  explicit SnapshotViewStream(std::string_view text):std::istream(nullptr),buffer_(text){rdbuf(&buffer_);}
};
}
