# A larger bounded host snapshot, with the original guest unchanged

## Recovery and large-input integrity repair

Run `42581684152` at `38eee33995bf125f8e84197824cf6ae44deb0147`
recovered the exact C90 stage 6 source and passed the complete old/new byte,
console, C46 and kernel comparison. The native larger-state trial then completed
300 million instructions, but Node 24 rejected its single SHA-256 update with
`RangeError: data is too long`. This tail was not fully admitted and is not a
continuation source; C90 stage 6 remains canonical.

The explicit host preload now sends a large hash input through zero-copy blocks
of at most 64 MiB. It preserves every byte and the original SHA algorithm;
ordinary updates, encodings, chaining and native errors retain their original
behavior. The regression uses actual inputs above 2 GiB, including an offset
DataView, and compares against a separately constructed incremental hash.
The original codec and observer files remain unchanged. The full real-Android
validation must pass again before this profile can be selected for continuation.

[Recorded failed admission](evidence/c90/recovery-host-hash-failure.json).

C90stage7 stopped while saving a checkpoint: `Guest snapshot exceeds configured host transport capacity`. The last fully admitted canonical state remains C90stage6 at161147727998 instructions, SHA-256 `c8a2aa15bca412139973343813570a07d8f11d9fb7318a884cd64d45025f5714`. Its six complete original admissions and transports are preserved. The failed stage7 heartbeat and logs are diagnostic evidence, not a recoverable admitted boundary or an ISA failure.

The candidate increases only the explicit host RVG file bound from2048MiB to3840MiB. The existing4GiB unpacked transport bound and200MiB pieces remain unchanged, leaving256MiB for metadata. Guest RAM remains2147483648 bytes. CPU instruction semantics, compact XI/Ether memory, zero background, RVG/RVI/MXIM codecs, authenticated disk, seed,64 instructions per tick, IRQs, enforcing policy and original boot/UI gates remain unchanged.

`scripts/prepare-host-transport.py` creates separate build copies. The native change is the host view-capacity constant and truthful host-capacity provenance. The JavaScript copies change only explicit file-size policies and the kernel-observer CLI entry point. The original kernel observer export, BTF/property checks, milestone logic, checksum and memory-line decoder remain byte-identical. All870 imported files remain unchanged. Every copied tool has source/build hashes in the generated manifest.

Node's synchronous file API has a separate approximately2GiB read limit. An explicit preload reads larger `.rvg` files in at most64MiB positional chunks, rejects files above3840MiB, requires complete reads and verifies stable file size/mtime. Ordinary files and text reads retain the original behavior. No decoded-memory substitute, RAM patch or guest instruction lives in this file reader.

Validation first runs the unchanged native and candidate from the same original C89stage15 control source for50000000 instructions. It requires `cmp` equality of every RVG byte, identical console bytes, the exact same original C46 gate and identical full original kernel observation. The duplicate state is retained once with each execution's own bound metadata. These controls are not primary authority.

The candidate then resumes the actual canonical C90stage6 for300000000 instructions, at the boundary where the previous host bound failed. It must produce an actual snapshot larger than2GiB, pass full original native roundtrip and C46 admission, and retain authentic console history, all eight prior events, zero new UART, enforcing SELinux and fresh real peripheral checks. Every complete larger checkpoint piece is preserved. A successful validation authenticates the host policy; it does not claim completed Android boot or a usable UI, and does not silently promote experimental controls.
