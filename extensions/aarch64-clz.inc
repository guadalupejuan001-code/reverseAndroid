// Arm DDI 0602 (2025-03), CLZ (vector), p. 1136.
// This extension is inserted into a verified build copy of the original decoder.
// Q and size select all six allocated forms; scalar and 64-bit lanes stay rejected.
if ((i & 0xbf3ffc00u) == 0x2e204800u) {
  if (size == 3) return false;
  fp_access();
  const auto source = floating_.v[rn];
  Vector128 value;
  for (unsigned lane = 0; lane < width / element; ++lane) {
    const auto x = source.lane(lane, element);
    unsigned count = 0;
    for (auto mask = UINT64_C(1) << (element - 1); mask && !(x & mask); mask >>= 1)
      ++count;
    value.lane(lane, element, count);
  }
  commit(value); // The 64-bit vector forms clear the high half.
  return true;
}
