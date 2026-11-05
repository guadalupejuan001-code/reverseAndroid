# C28: continue the clean C27 machine

C27 completed its three native budgets with 2,833,048,839 genuine instructions,
the original `/init` entry observed again, second-stage init and SELinux
enforcement confirmed, and no CPU error, panic or requested reboot. It scanned
87 preinstalled APEX packages and was activating them when its host budget
ended. The new empty userdata partition has not reached its late mount yet.

Source run: `41140864071`, job `124354500478`, artifact `13021720249`, tested
source commit `6a944cc2f9b244b224a409ccc1938ad603524e2e`.

| C27 evidence | SHA-256 |
| --- | --- |
| Artifact ZIP | `342ac9652adfb5e89f0ce56a41f190378438199cfe9f012721c24fd3132f19b4` |
| Snapshot | `9db40b80214344583f855ccc777eb74963ece41ca5a1afab8de0737507f0e939` |
| Report | `7ff064cd92bb1fffa7de798e971abc456dd8032b7cebc0bb7c6cc44aee34e903` |
| Result | `dc1f9d47ed847c5f298ec93e8bc70c963740720408b3886b2b36996e471c7e61` |
| Host outcome | `4670e6d2de854298419f66e5658b241a9ea5e3ccc2ab8eadce4e9c4055c949c9` |

C28 authenticates all four source files and their completed clean-budget
outcome, independently verifies the saved board/CPU/clock and original init
provenance, then checks compiled native pins before opening its disk. The
continuation round trip must preserve every snapshot byte. It retains the
original CPU, RAM, devices, disk deltas, counter seed and cadence across three
further budgets. No new init-entry observation is claimed during continuation.

The exact source PC is `0x5568b464ec`, timer subtick `23`, and seed digest
`699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb`.
The immutable board image remains
`01d583917392fd19438d82d7b135db3bd530949e57bc568181aba54b7a92c75d`;
the independent disk remains the measured C27 disk. Kernel, initrd/CPIO,
bootconfig, AVB, SELinux, CPU capabilities and XI/Ether sources remain intact.
Production compilation remains O2.

The workflow publishes immutable observed prefixes, final canonical evidence,
the exact saved scanout when present, and a read-only live PID 1 property/process
observation. [Scanout export](ANDROID_CHECKPOINT_FRAME.md) executes no CPU
instructions and does not itself claim Android success. Full framework and
display verification require corroborating runtime evidence and visual review.

The existing fixed-C21 generic resumer also receives a host-only watchdog
correction: its documented three fifteen-minute defaults previously calculated
61 minutes, exceeding the supervisor's one-hour bound before any execution.
Reserve startup and final publication once; the defaults now fit in 57 minutes.
Schedules exceeding the bound fail before source I/O. Native CPU semantics,
source acceptance and the explicit transport policy are unchanged.
