# GPU resource lifecycle: isolated original-Android trace

The C87 console contains virtio-gpu response `0x1203` for TRANSFER_TO_HOST_2D,
SET_SCANOUT and RESOURCE_FLUSH. This identifies a failed resource lookup, not the
cause of that lookup. Suppressing the error or assigning a replacement resource
would hide the actual lifecycle problem and could associate pixels with the
wrong guest object.

## Implementation

`prepare-gpu-resource-trace.py` extends a separate CLZ/IC IVAU build tree. The
original 870 imported files and their codecs remain byte-identical. It adds a
bounded, host-only record at the selected checkpoint, GPU reset and each control
request/response. Each record includes:

- Original instruction boundary, queue and command.
- Requested resource ID, presence of that field and actual response.
- All four current resource IDs, their extents, scanout ID and flush counter.

No IDs or guest fields are created, repaired or relabelled by the observer. It
does not add UART input or modify the device's semantics, limits, RAM, disk,
clock seed, 64-instruction cadence or interrupt checks. Records are explicitly
provisional: an event inside a rolled-back native run is not proof of a committed
guest operation. The trace has a 16,384-record limit; a capped trace cannot pass
the full comparison's coverage check.

## Actual native protocol regression

`reverseandroid_gpu_resource_trace_test.cpp` uses the original device-test client
and real AArch64 MMIO instructions to create resource42, attach guest memory,
transfer/present/flush pixels, restore the original RVG, reject unknown IDs,
retire42, reuse its slot with43, and reset the device. It verifies that expected
invalid-ID errors remain errors and that successful operations preserve actual
XI-backed pixels. The uninstrumented and instrumented executables must emit
identical whole RVG bytes and identical reports. These fixture pixels are not
Android display or boot evidence.

Reproduce after the standard build:

```sh
bash scripts/build-reverseandroid.sh
bash scripts/build-gpu-resource-trace.sh
```

The second command requires fresh `build/gpu-resource-trace-src` and
`build/gpu-resource-control-src` directories. It also runs the original PCI
regression before producing the diagnostic native executable.

## Controlled Android replay

The independent workflow selects the fully admitted C88 stage1 source at
124,147,727,998 instructions, SHA-256
`8867798e0c98f554c4b7af0bf521ff5a3f39eb44914f759301e0e6e534b0f7d1`.
This was the published current admitted boundary when this experiment was
selected. It is a fixed diagnostic source, not a replacement primary pointer.

All four original transport pieces, the whole original snapshot, its bound
report and unchanged original disk are authenticated before running. The
selected original native SHA-256 is
`957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f`.
Both executables resume that same source with zero new UART input and the same
50-million-instruction budget. Every new output passes the original epoch
admission, kernel, property and frame observations.

The comparison requires identical full original RVG SHA-256 and identical whole
new console bytes, verifies the unchanged source, and reports the actual final
boot gate. One complete copy of an identical resulting checkpoint is preserved;
a distinct admitted diagnostic result is preserved separately if comparison
fails. Metadata and raw logs are retained in both cases.

The running primary C88 workflow, `state/latest.json` and
`state/continuation.json` are untouched. This experiment does not claim that the
GPU caused Zygote's futex wait, that QSSD repairs resource IDs, or that Android
has booted. Its output supplies the IDs and lifecycle evidence needed to choose
an actual device correction or an explicit GPU adapter next.
