# Cuttlefish ARM64 Android guest provenance gate (one bounded cycle)

## Strict starting point

The most recent proven REVERSE original XI/Ether guest is the Android Common
**GKI kernel-only** snapshot at **59,604,096** ARM64 instructions; its snapshot
SHA-256 is `7bfbdb74a4cdcea31167f00202edafb9265406c0c7dbc2f7cb1e5c6c3d93577f`.
The guest executes genuine upstream Linux kernel code but its immutable RVI1
contains **no initrd**; there is no Android first-stage `/init`, Binder, ADB,
graphics or original Wild Rift process. The verified kernel SHA-256 is
`8d8cf5254f0823655e06ffee100d9c19abbf4ce33adb107b66da3fdb3742e0be`.
The last checkpoint belongs to draft PR #43; PR #44 contains the independent
source-matched symbol diagnosis. Neither should be rewritten or force-merged.

## Required actual Android image set

The source-supported Cuttlefish ARM64 `aosp_cf_arm64_only_phone-userdebug`
image package includes `boot.img`, `init_boot.img`, `vendor_boot.img`,
`super.img`, `userdata.img`, AVB `vbmeta*` and bootloader artifacts.
Inspect an image ZIP from **one exact build ID and target**. Never assemble
a GKI kernel from unrelated build 16707926 with Cuttlefish vendor or system
images without demonstrating KMI, module and board compatibility.

Sources:
- https://source.android.com/docs/devices/cuttlefish/get-started
- https://android.googlesource.com/device/google/cuttlefish/+/0e2b74b4f62500fa9cc2716e9e4aaff810bbe0ea/required_images
- https://android.googlesource.com/device/google/cuttlefish/+/refs/heads/main/shared/BoardConfig.mk
- https://android.googlesource.com/platform/system/tools/mkbootimg/+/refs/heads/main/include/bootimg/bootimg.h

### Acquisition stage 1 (implemented)

A public, read-only GitHub Action discovers the **last known good** build ID
for ARM64 Cuttlefish on `aosp-android-latest-release` using the official
`https://ci.android.com/builds/branches/aosp-android-latest-release/status.json`.
It hashes the exact fetched status, saves the selected target/build, and
stops on missing/invalid source data; it must not invent a build number.

### Subsequent acceptance gates (not yet met)

1. Retrieve the official single-build ARM64 image ZIP and inventory only
   permitted files by exact path/size/CRC, without silently downloading an
   unbounded multi-GB partition or allowing path traversal/ZIP bombs.
2. Verify `boot`, `init_boot`, and `vendor_boot` headers/sections using
   REVERSE's existing AOSP parser and the pinned upstream unpack_bootimg oracle;
   record their SHA-256 and relevant vendor ramdisk fragment/bootconfig.
3. Decompress the real **generic** ramdisk in a bounded host-only tool and
   identify the exact `/init` entry and ISA using ELF metadata or CPIO
   filename, without executing it in host or fabricating console output.
4. Identify board-device and partition differences against the REVERSE
   generated FDT (PL011/GIC/timers, no virtio block/Binder/ART support).
   Do not substitute Cuttlefish's DTB for a different virtual board.
5. Only then run the authentic kernel+initramfs on the original XI/Ether
   interpreter, saving its PC/opcode/EL and UART and a reproducible RVG.
   An Android guest '/init' must be confirmed by *actual* observed guest
   activity; a host Cuttlefish/QEMU oracle is insufficient evidence.

The current REVERSE Linux-only image quotas (kernel 64 MiB; initrd 64 MiB;
total RVI1 128 MiB) and adaptive guest snapshot transport are finite.
Android device's UI transport/partition drivers remain separate blockers.
No runtime changes are necessary for a read-only artifact preflight.
