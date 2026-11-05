# Android C02: retain translations only across unrelated stores

Native CPU stores previously invalidated all address hints, even when writing
ordinary stack or file data. Each subsequent instruction/data access could
repeat the same page-table walks through the original InverseMemory.

The CPU now records a conservative, nonpersistent 256-bit footprint of physical
pages used for translation-table reads. A <=16-byte store checks both resolved
endpoint pages. A possible overlap clears every hint, including hints installed
while validating that store. Hash collisions only add invalidations. Table
ancestors, remapped stores and physically disjoint cross-page tails are covered.
Public writes, XI/RXE changes and TLBI retain complete invalidation.

Any transaction starting with quantum bindings also retains complete
invalidation. A store through one bound byte may change a PTE on another page
through the same qubits; a physical-page-only test would be insufficient.
The new regression reproduces that exact failure when the guard is removed.

These are address hints only. There is no new RAM cache, owner, snapshot format,
instruction shortcut, kernel/initrd modification or change to virtual ticks.
Mundo-X remains pinned at e2778d962af22f6f1a0794058dd4685fc2573966.

## Validation and measurements

Local tests pass 1,989 system checks, including the original 4,096-qubit witness,
ancestor-table replacement, a cross-page store whose tail replaces a PTE, and
nonlocal quantum aliases. A 1M genuine Android interval and all four runs of a
5M ABBA interval produce byte-identical states and console output on the exact
original and new CPU.

The 500K-instruction profiling comparison also produces the same RVG.
Calls to InverseMemory::read_numeric fall from 8,461,627 to 3,819,627:
54.86% fewer internal byte queries in that measured interval. Guest instructions,
observable state and memory revisions are preserved.

The local 5M ABBA end-to-end timings were 14.068 / 8.475 / 15.641 / 14.695 seconds
(original/new/new/original). The median ratio is about 1.19x, with substantial
timing variation. This is not a universal throughput claim or game FPS estimate.
The dedicated CI repeats exact-state comparisons independently on its genuine
published 180M checkpoint and tests the negative quantum control.

## Better boot observation

C01's unchanged aggregate initramfs fields did not establish stalled extraction.
The read-only observer now also reads task scheduling/accounting and, during
an active copy, the actual file/inode, allocated blocks and filename using the
same immutable kernel's BTF layout. Closed or unreadable file objects are not
treated as fatal CPU errors.

Continuation has independently observed cfg80211.ko, mac80211.ko,
mac80211_hwsim.ko, net_failover.ko, virtio-gpu.ko and virtio_net.ko being copied.
Changing filenames and allocated blocks establish actual extraction progress.
They do not establish loaded drivers, Android services, guest graphics or games.

The original kernel and initrd remain the execution inputs. Continue from the
latest validated checkpoint rather than rebuilding an empty guest. First-stage
Android /init still requires exact ELF identification and actual executed
instructions; later storage, Binder, services and display gates remain separate.
