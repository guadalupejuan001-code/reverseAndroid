# C52: exact faster host evidence

The C50 original observer profile spent 13.0 of 30.6 seconds repeatedly resolving and checking the same immutable BTF struct layouts, and 6.9 seconds in the independent JavaScript FNV checksum. Repeated source/output gates magnify this host overhead even for a one-instruction continuation.

Two explicit opt-ins retain the original default/reference paths:

- `REVERSE_PC_FAST_FNV=1` computes the exact original FNV-1a-64 with a 95-byte WebAssembly loop and fixed 64-KiB host scratch memory. It reads every requested byte, wraps the same unsigned 64-bit multiplication and returns the same checksum. The module has no imports and never decodes or executes an Android instruction. The original independent native decoder/checksum and full disk verification are unchanged.
- `REVERSE_PC_FAST_OBSERVER=1` caches a BTF struct only after the existing duplicate-layout validation passes. The cache belongs to one synchronous observation of the same pinned immutable kernel, and is discarded after it. No cross-checkpoint or guest-memory data is cached.

On the genuine 1,111,074,591-byte C50 checkpoint, original and accelerated FNV both matched the embedded native checksum. The original took 7,023 ms; the accelerated check took 1,581 ms (4.44x for this checksum only). Complete live property, Zygote, scheduler, security and kernel observation JSON matched the original byte for byte. Timing varies by host and cache state.

Tests compare exact BigInt vectors, offsets, prefix bounds, all scratch boundaries, repeated calls, retained input bytes, invalid inputs and the existing evidence regressions. CI uses the independently pinned C51 original scripts and the same authenticated C50 checkpoint to compare full output and record elapsed time. It asserts the entire runtime, decoder, compact format and CMake build remain identical to C51.

This changes host verification cost only. It adds no guest RAM, clock changes, skipped instructions, new snapshot format or new guest engine. C51 genuine Android continuation remains separately gated by its hardware-verified instruction semantics. Completed boot and display pixels still require their original live evidence.
