# Android C31 — bounded original-machine continuation beyond post-fs-data

C30 independently authenticated the C29 fourth completed checkpoint:
`454e29a90cc1af34df95b2c60f1fc90d29927096c72be962e95b2e9fca16598d`
at **13,615,798,839** guest instructions. Its read-only observer found the
actual running `init` PID 1 and a live `keystore2`, among 196 process leaders;
`zygote64`, `system_server`, `surfaceflinger` and display scanout were not verified.

The C29 final console confirms that `vdc --wait cryptfs init_user0`
exited with status 0 at guest time 208.813655 and that later
`post-fs-data` operations proceeded to guest time 212.273984.
A rollback-resistant key operation failed and vold explicitly fell back to a
non-rollback-resistant key; the `init_user0` process then finished normally.
These facts **do not** prove the full framework or display.

C31 resumes **the exact same C27 machine** from the full authenticated C29
artifact. Its scoped CI first authenticates the original four-file contract,
including the root/original-init proof, immutable memory, logical 2 GiB RAM,
board image, original disk hash and the original timer seed. It rebuilds the
exact C27 blank-data disk, confirms compiled native pins, and runs the
**unchanged XI/Ether emulator** for one budget bounded by 120,000 host
milliseconds and 300,000,000 instructions. This is an actual native
continuation, not a simulated step or a replacement for the Android guest.

If and only if the native process completes a valid clean budget, C31
publishes the new checkpoint, canonical provenance, independent live kernel
and property evidence, scanout status and full four-file output contract.
The original C29 artifact is left intact. A failed/terminal/incomplete
execution is not labeled healthy or automatically resumed.

The workflow checks the source `src`, dependencies and native guest tools for
unexpected changes, and runs host policy regressions before execution. A
CI success indicates completed bounded native work; it cannot by itself
assert that the Android framework, Launcher, a display or Wild Rift works.

Output and evidence:
`reverse-c31-authenticated-continuation` and `reverse-c31-checks`
from the C31 GitHub Actions run. The next CPU continuation must authenticate
**its actual output contract**, rather than replacing it with these expected
instructions or assuming screen readiness.
