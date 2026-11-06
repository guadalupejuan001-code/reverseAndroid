# Cuttlefish peripheral peer (experimental)

The actual original ROM services abort because their terminal channels are absent:
OEM Lock reports `Could not connect to oemlock`; sensors reports `Could not connect
to sensors control`. This adapter supplies binary streams to those original HAL
executables. It does not register Android services or write boot properties.

The wire and port reference is the official `google/android-cuttlefish` repository,
commit `55d6954ef1fbbadea7b28d9ba19b902d3fc08aac`. Relevant files under
`base/cvd/cuttlefish/` are `common/libs/transport/channel.h`,
`common/libs/security/oemlock.h`, `host/commands/secure_env/oemlock/`,
`host/commands/sensors_simulator/sensors_hal_proxy.cpp`, and
`host/libs/vm_manager/vm_manager.h`. The reference is a protocol baseline, not a
claim that this June 2026 ROM was compiled from that commit. Actual original HAL
traffic and process observations must establish compatibility.

| Endpoint | Actual protocol peer | Guest object label | Endpoint owner |
| --- | --- | --- | --- |
| `/dev/hvc10` | OEM Lock request/reply | `oemlock_device` | 1064 |
| `/dev/hvc18` | Sensors control | `sensors_device` | 1000 |
| `/dev/hvc19` | Sensors data | `sensors_device` | 1000 |

Each stream uses the upstream little endian eight byte header: command31,
response1, payload size32. OEM state uses the upstream software model: initial
byte3 (carrier/device allowed, unlocked), independent locked bit, persisted before
acknowledging a setter. This has no effect on AVB or SELinux policy.

The explicit virtual sensor model is a stationary flat phone with +Z up: gravity
9.80665 m/s², zero angular velocity, magnetic field (0,5.9,-48.1) µT, temperature
25 °C, proximity5 cm, light100 lux, pressure1013.25 hPa and humidity45%. The phone
mask is503. Initial reports contain all eight sensors; subsequent one second
reports contain the five continuous sensors. Time comes from the guest kernel's
monotonic clock. Control and data use different PTYs.

`--guest` requires root, real raw Linux PTYs, exact verified SELinux labels and
0600 endpoint modes. It creates aliases only if absent and refuses to overwrite
any device. A root-owned0700 directory and0600 state file hold OEM state in normal
guest storage. Provisioning failures remove only aliases created by that process.
Host-test executables reject `--guest`. SELinux enforcement stays enabled. A
label or access denial is a failed integration test, never a reason to relax policy.

Build with `bash scripts/build-cuttlefish-peripheral-peer.sh`. Literal independent
wire vectors exercise state truth tables, malformed lengths and fragmented
requests. Real PTY tests exercise separate channels, exact binary replies, durable
state across process restart, sensor activation and invalid-request rejection.
The static AArch64 ELF has no dynamic loader or libc dependency.

Passing those host checks does **not** prove Android integration or boot. Before
any primary adoption, deliver authenticated bytes through an explicit UART input
plan into an isolated fully admitted state; verify the guest file hash; inspect
actual original HAL executables, ownership, open descriptors and real requests;
then verify the original admission gates and live SELinux state. A ready marker is
only a peripheral process result. SystemServer, live PID1 boot completion, a real
visible frame and an actual UI input response remain separate required evidence.

A prepared buffered transfer variant avoids interactive long-line redraw.
`python3 scripts/prepare-peripheral-buffered-inputs.py VERIFIED_BUILD NEW_DIRECTORY`
creates a short root capture command, two bounded base64 data lines, activation,
and inspection inputs. The root command uses real blocking `IFS= read -r` calls
and checks each actual file prefix hash before acknowledging it. The exact
verified ELF was recovered in a host PTY/POSIX-shell test;
[host capture evidence](evidence/c80/buffered-transfer-host-capture.json). This
variant was executed in the isolated ABI2 Android control: the complete guest
ELF matched the corrected build. That integration failed at the real enforcing
kernel's filesystem association check, before any successful HAL connection.
All admitted states of that negative control remain preserved. Each fresh
transfer requires distinct source-bound events and actual response checks.

AArch64 portability correction: Linux arm64 retains ARM32 assignments for
`O_DIRECTORY` (16384) and `O_NOFOLLOW` (32768). The first prototype used x86
values, which select `O_DIRECT` and `O_LARGEFILE` on arm64. The corrected peer
uses explicit arm64 definitions and host-test binaries use their system headers.
An unmodified official Linux v6.12 arm64 UAPI fixture independently checks all
eight open flags at compile time. The former build and guest control remain
preserved. Actual guest execution confirmed the corrected ELF and then exposed
the separate filesystem association failure described below.

The corrected build is authenticated in
[ABI2 build evidence](evidence/c80/peripheral-ABI2-static-build.json). A new
isolated buffered control starts at the latest preserved original C80 source.
Its root receiver verifies each real file prefix and then the complete ELF.
The root-capture and raw-data method was checked using real host PTYs and the
exact corrected binary; this remains separate from actual Android integration.
Canonical adoption still requires actual original HAL registrations, the real
reciprocal kernel PTY links, and original admission with enforcing SELinux.
# Private PTY filesystem candidate

The corrected ARM64 ABI2 executable reached the real guest and its SHA256 matched the pinned build. At its admitted 60B boundary the original kernel rejected `oemlock_device` association with the existing `devpts` filesystem, with `permissive=0`. This is an integration failure; the original main does not contain this peer.

The next candidate uses only `/dev/ra-pts/ptmx` and `/dev/ra-pts/<index>`. It requires an independently provisioned private devpts mount whose filesystem security label is `u:object_r:tmpfs:s0`, with root inode label `u:object_r:device:s0`. Its three slave inodes must still acquire and verify their exact original OEM Lock or sensor device labels, owners and mode0600. The existing system PTY instance is not changed. No SELinux allow rule, enforcement setting or Android boot property is changed.

This is a proposed device-filesystem configuration, not a passed guest integration. [AOSP's current filesystem association rules](https://android.googlesource.com/platform/system/sepolicy/+/refs/heads/main/private/file.te) distinguish device nodes on the device filesystem from ordinary devpts labels. [Linux devpts documentation](https://docs.kernel.org/filesystems/devpts.html) describes independent instances. [The mount manual](https://kernel.googlesource.com/pub/scm/utils/util-linux/util-linux/+/refs/tags/v2.41.1/sys-utils/mount.8.adoc) distinguishes the filesystem label from individual inode labels. The new candidate must pass the real enforcing guest, original HAL executables, all three reciprocal PTY links and actual Binder registrations before any canonical adoption.


The private-PTS control starts from the admitted terminal ABI2 failure at61B. It authenticates the existing guest ELF, reconstructs a separate ABI3 filename through real guest `cp`, `dd`, `printf` and `xxd` calls, and verifies the new whole ELF SHA before execution. These are modeled UART inputs and normal guest file writes. The RVG is never edited. The preparation test runs the production blocking-read body against an unmodified upstream Toybox reverse-parser reference and compares every output byte with the compiled candidate. Reproduce the host check with `cc -std=c11 -O2 tests/fixtures/toybox-xxd-reverse-reference.c -o /tmp/xxd`, then run `scripts/prepare-peripheral-privatepts-control.py` with the two authenticated build directories and `--host-xxd /tmp/xxd`. The mount step checks enforcing1 and the unchanged loaded-policy SHA, and the final test requires three distinct actual kernel PTY pairs and both original Binder registrations. This isolated control is not promotion authority.

The separate ABI3 guest file was reconstructed correctly at66B. Its exact hash
was interrupted by a timestamped kernel message on the shared UART; the
[read-only console observation](evidence/c80/privatePTS-stage5-actual-hash-multiplexed.json)
reconstructs that full hash and path while preserving the original bytes and
original admission flags. Only complete timestamp-and-TID-tagged printk records
are removed; prompts, quoted input and incomplete hash lines remain ineligible.

The first activation returned1 before producing any mount or peer result.
`sel_read_enforce` returns digit1 without a newline; a bare shell `read` sets
the value but returns EOF failure under `set -e`. The
[separate continuation from its admitted67B state](../.github/workflows/android-peripheral-privatepts-eof-control.yml)
catches and prints that original read result and uses `cat` for the complete
enforcing-value checks. The real guest confirmed `READ_1 VALUE_1` at68B.
[Evidence](evidence/c80/privatePTS-actual-activation-EOF.json) binds the raw
console bytes, kernel source reference, original admission and every transport
piece. The existing ELF transfer is not repeated. Actual private mount, peer
readiness, policy hash equality, original HAL connections and Binder
registrations are still required; they have not yet all passed.

For a later fresh canonical source,
`scripts/prepare-peripheral-privatepts-buffered-inputs.py VERIFIED_ABI3_BUILD NEW_DIRECTORY`
prepares a complete five-phase ABI3 transfer with the same explicit private
filesystem setup. Real host PTYs verify the produced blocking capture and
reject a corrupt prefix before another read. The actual decode body reproduces
every compiled ELF byte. This prepared variant has not run in Android, has no
source binding and cannot be adopted until the isolated original HAL test
succeeds. Both acknowledgments of each data phase must be observed before the
next phase is allowed.
