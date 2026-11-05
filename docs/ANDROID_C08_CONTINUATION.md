# C08: continue the exact verified C07 state

This experiment starts at **6,350,795,356** native transitions, not at C06 or a fresh boot. It uses the original XI/Ether `MinimalGuest`, the same 128 MiB RAM state, immutable Android image, persisted counter stream, and 8,738,832,384-byte disk. No guest code or production engine file is changed.

The host accepts only C07 checkpoint SHA-256 `845fb40da060147a23449ecfd4e7a3d8b11def8d66696093c6e6728798b82694`, matching C07 report SHA-256 `5d426d1fec117b8ef955bbbccb0e944586f36c6fe8594d9c684575dd0067e574`, and original C06 entry report SHA-256 `ec12937b7c33a3aff439f3c388bbd32b1f538e08667d0f83e61736489666bdbf`. These are the bytes published by successful run 40975381521, artifact 12942985906.

The existing C06 verifier is deliberately unchanged: it describes an EL0 first-instruction boundary, whereas C07 ended in EL1 inside module loading. C08's separate, immutable input validation establishes ancestry before execution. The per-run first/prior-entry flags remain false because that entry is not replayed. Ancestry is recorded separately in provenance and independently verified by the read-only audit.

The runner commits bounded transactions and uses the existing atomic two-generation journal. It saves every 15 seconds or 25 million transitions, whichever comes first. Interrupted or failed execution retains its last complete state/report pair. Hard termination before the first committed generation cannot produce a new checkpoint. The workflow always attempts preservation and uploads diagnostics; a failure is never relabelled as Android boot success.

The initial investigation budget is 200 million transitions and 240 seconds of execution. These are host investigation limits, not newly imposed architecture limits. A panic, instruction failure, wait without progress, interruption or inconsistent proof fails the gate. Completing a budget demonstrates only validated execution progress, not the Android framework, screen or games.

`tests/android_c08_audit.test.mjs` tests evidence validation, including clearly labelled synthetic tuples. Those synthetic tuples are never executed or published as Android progress. The workflow also runs native cycle UBSan tests and negative artifact controls.

Run results, exact source revision, trace, checkpoint, report, native next instruction and a build-source archive are preserved in `reverse-c08-exact-continuation`. Initial execution results are not yet asserted by this document.
