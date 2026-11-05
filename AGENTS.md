# reverseAndroid

This is the user's dedicated Android emulator repository. Continue emulator work here. REVERSE-PC is the historical source, not the active destination for new emulator cycles.

Read `state/latest.json` when present and `state/continuation.json` before selecting a checkpoint. The latest fully admitted boundary is authoritative. Do not restart from C39, C40 or the C68 import when a later verified state exists. Failed or interrupted tails are diagnostic evidence only.

Preserve the user's compact XI/Ether authority, original zero background and RVG/MXIM/RFP codecs and decoders. Keep the authenticated disk, 2 GiB logical RAM, timing seed, 64 instructions per tick and IRQ checks unless the task specifically changes one of them and validates that change. Preserve every prior valid checkpoint.

Advance in bounded cycles. Keep full original admission, original snapshot roundtrip, checkpoint/report/process binding, kernel and live-property observations. A successful migration or instruction budget is not completed Android boot. Report `system_server`, live PID 1 `sys.boot_completed=1`, SELinux and visible display evidence separately and accurately.

Do not fabricate execution, patch guest snapshots, inject Android properties or silently change scheduling/UART input to bypass a stop. Model any intentionally requested guest input as an explicit, authenticated input event.

The unchanged C68 import is retained on `migration/c68-verified-source`. `state/upstream-snapshot.json` authenticates all 870 imported files, with only the documented workflow/README relocations. A future deliberate ISA fix must preserve that reference, document its exact scope, update the validation appropriate to that change and pass independent instruction/decoder tests. Do not silently remove the original format or admission checks to make a run pass.

Build and test with `bash scripts/build-reverseandroid.sh` after checking out the pinned Mundo X dependency listed in README. Checkpoint transport is split into bounded pieces; authenticate both archive and complete original snapshot bytes before resuming. Future checkpoint/media input pins should point to artifacts in this repository once the migration has completed.
