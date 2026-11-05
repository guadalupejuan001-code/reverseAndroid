# Genuine Android checkpoint in the REVERSE desktop

This branch extends the verified kernel work in PR #53. It addresses the
actual 16 MiB import and 32 MiB Java/JNI packet ceilings that prevented the
large genuine Android RVG from reaching the existing phone interface.

GuestStore now streams imports and exports in 8 KiB buffers. Native code
restores the same MinimalGuest/QuantumRuntime from its private checkpoint,
executes the requested operation, and writes a separate candidate. Java
publishes that candidate with the existing AtomicFile mechanism only after
native validation succeeds. The original desktop/BBY Q transport and its
limits are unchanged. Guest file transport remains bounded to 256 MiB;
large files and actual native allocations still consume host resources.

The existing XI/ARM64 view gains **Importar checkpoint** and
**Continuar arranque**. Continuation submits one 250,000-instruction block
at a time to the existing process-wide RuntimeExecutor, then publishes the
real state and output. Pausing finishes and saves the current block.
Halt, wait, errors, leaving the visible view or pausing the Activity stop
the loop. No Android boot percentage, completed /init or guest screen is
fabricated. Existing manual execution remains capped at 10,000 steps.

Import the extracted `current.rvg` from the continuation package. It already
contains the unchanged kernel, initrd, device tree and exact saved CPU state.
The separate Linux Image importer remains a kernel-only import; it does
not invent an Android initrd or system partitions.

The native file test covers a 42,039,103-byte fixture, byte-identical direct
execution, XI and console operations, checksum rejection, source protection
and the 256 MiB input ceiling. It is explicitly a transport fixture.

The Android CI additionally imports the genuine 78,699,400-byte Cuttlefish
checkpoint at 180,326,448 instructions, pinned by SHA-256
4f139751053c39bc1d33c7012fe1d61a8135f002c67be6f0466b3e8f97e2eda3.
It advances the actual guest 250,000 instructions through the UI bridge and
file persistence, and requires the resulting RVG and streamed export to
match an independently executed host reference byte for byte. A corrupt
large import must preserve the last valid saved state. Separate checks
retain the original 4,096-qubit and desktop persistence witnesses.

Those checks demonstrate transport and continuation of the real kernel.
Android first-stage init, storage/partitions, services, ADB and display
remain separate boot gates. The experimental APK is not a completed
Android guest or a published main-branch release.
