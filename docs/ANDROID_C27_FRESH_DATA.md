# C27: prepare writable data for the selected filesystem

C26 repaired the authenticated CMTST instruction and advanced the original
machine by 2,433,900,000 instructions to 8,996,438,801. The guest then requested
recovery because `init_user0` failed. Its terminal checkpoint
`6629dff51f4bad2a6d3503dde4d6094cf4649af3a8d66ba20286f9fac6c3e00d`
is diagnostic evidence and must not be resumed as a healthy machine.

The original official userdata image is F2FS (magic `f2f52010` at offset 1024),
but the selected, unchanged `fstab.cf.ext4.cts` accepts ext4. At 117.705 guest
seconds init rejected its superblock; `mount_all --late` failed, vold found no
metadata-encryption key, and `/data` remained on the read-only root filesystem.
At 143.083 seconds init requested recovery with `init_user0_failed`.

C27 creates a separate new disk with an empty 2 GiB userdata partition. This
follows Cuttlefish's `InitializeDataImage` / `CreateBlankEmptyImage` behavior:
the official assembler replaces a data image when its filesystem differs from
the selected format, then creates an empty image with `ftruncate`. Android's
original `formattable` fstab entry, vold, KeyMint and formatter must create the
filesystem and encryption keys themselves. No host-generated key or mounted
guest filesystem is substituted.

Reference: [official assembler data-image policy](https://github.com/google/android-cuttlefish/blob/c857a805bd20fe32a791eb3552ed0c43d48b6452/base/cvd/cuttlefish/host/libs/config/data_image.cpp).

| Input or output | SHA-256 |
| --- | --- |
| Original disk, all eight partition payloads retained | `28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c` |
| New empty 2 GiB data input | `a7c744c13cc101ed66c29f672f92455547889cc586ce6d44fe76ae824958ea51` |
| New C27 disk, 10,888,413,184 bytes | `4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e` |
| Unchanged C21 initrd | `5278d736d20865d82be9783593690350496d1bcccb157769795316c5e527eefb` |
| Expected new immutable board image | `01d583917392fd19438d82d7b135db3bd530949e57bc568181aba54b7a92c75d` |

The fresh loader extracts the exact kernel, initrd and boot arguments from the
pinned C21 source `8c26488967ee3a9b2a55313810e92bce5ee2a182dbe81e8374bd2f6d3007ea05`.
It creates a new CPU at instruction zero, 2 GiB RAM and a new OS-generated
counter seed. It does not restore that source's CPU, memory, devices, disk
deltas or init proof. The genuine `/init` entry must be observed again. Three
bounded budgets retain this same new machine and preserve an entry observation
only after the native original entry bytes have actually executed.

The XI/Ether engine, CPU decoder, kernel, CPIO, bootconfig, AVB handoff and
SELinux policy are unchanged. Host-only tools verify compiled pins before disk
open, hash the whole new disk, independently audit each saved binary and freeze
immutable prefixes for inspection. The old media is preserved. The console
classifier now rejects actual late-mount and read-only-data failures and
recognizes both `System halt` and `System halted` as terminal failures.

Preparation and a successful host exit do not prove full Android boot.
Framework, live `sys.boot_completed=1` and real scanout still require runtime
evidence. The workflow preserves failed prefixes instead of labeling them as
healthy or bypassing the failed operation.
