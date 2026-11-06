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
variant has not been executed in Android. It requires five new source-bound
events, handling `REVERSE_PEER_CAPTURE` failures, and the prior successful actual
HAL control. It must not alter or restart the currently running four-phase test.
