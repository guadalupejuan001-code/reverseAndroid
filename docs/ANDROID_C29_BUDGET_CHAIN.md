# C29: continue the completed C28 budget

C28 completed three native fifteen-minute budgets, adding 2,063,600,000
instructions to the same C27 CPU. Its final count is 4,896,648,839, PC
`0x7f9f931ef4`, actual timer subtick `31`. The native
process exited zero, with no reported CPU error, detected boot failure or reboot.

Android activated its 87 APEX packages, generated linkerconfig successfully,
completed early aconfigd and module initialization, and started the vendor
BoringSSL self-test. The userdata late mount, full framework and usable screen
remain unverified at this source. The saved scanout export reports no scanout.

Source commit: `8dcb7159d22497b46b736227f4447df36c40bb23`.
Run `41146812713`, job `124373726991`, artifact `13023411596`.
Artifact ZIP: 69,373,989 bytes, SHA-256
`a4d62d4525b1fcbbe5259b937d1a4e0ce370995a83e257c319095bcbb06c5687`.

| Evidence | SHA-256 |
| --- | --- |
| checkpoint_sha256 | `70beb3330cb568068cab8c16df8082f98bab42ca1c106aa70fe8cafdbc76ba9b` |
| report_sha256 | `f2d2076e9d923b0abe2c29970556031839593044d4351d9f07d1393816bd28a7` |
| result_sha256 | `fb0242138498c490e363e755b4675a4c9bde0e561dfc249efa11476c732b6b02` |
| process_sha256 | `5c5d4afa2c129da446cd3aaf7bcddabf778b4cbc9ca3821b91dec70a73661092` |

The [fixed-board continuation protocol](ANDROID_C27_PROFILE_CONTINUATION.md)
verifies all four source files and their original C27 root before execution.
The C29 workflow permits up to four sequential bounded invocations, each
containing three fifteen-minute budgets on one CPU instance. Each subsequent
invocation requires the previous completed clean output and its newly measured
four-hash contract. It preserves the original seed, clock phase, registers, RAM,
PCI devices and block-write deltas. No new init entry is claimed.

Each round freezes an immutable fifteen-minute observation, then preserves its
final canonical snapshot and host outcome, exact scanout extraction and live
property/kernel observation. Intermediate observations are never accepted as
completed sources. A failed native process or unclean guest prevents the next
round. The workflow stops early when live framework evidence plus a bound
scanout are ready for visual review. It does not assert unreviewed image content.

Validation: 235 host tests passed. The generic native loader matched the specific
C28 loader's complete snapshot after 150,000 actual instructions from C27. A
separate end-to-end test restored an actual completed C28 child, executed another
50,000 instructions and generated a valid next-source contract. Its final SHA-256
was `fb7d4ffb66b829ba66480242d109e4cde5b93ff1fcfcaba848130023f6097f53`;
this local test is a separate fork and is not added to C28's production count.
The frame export executed zero instructions and correctly returned no scanout.
Both original test sources remained byte-identical. Ten local tool/test/doc
files were independently matched to committed Git blob hashes. The default real
C23 observer output remained byte-identical after adding optional live security
and lifecycle reads.

XI/Ether, Mundo X, kernel, initrd/CPIO, boot arguments, AVB/SELinux settings, CPU
capabilities and production O2 flags are unchanged. Full Android readiness must
be established from actual continuation output and visual review.
