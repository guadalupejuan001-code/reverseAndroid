# Real Android Cuttlefish vendor + generic initramfs engineering gate

## Baseline / source identity
The most recent successful original XI/Ether kernel + generic first-stage initramfs
checkpoint is **8,890,950 ARM64 guest instructions** ([CI 40799502063](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40799502063)),
SHA256 `b613833f175dc1adf3a53edd4e9002f69bde5ab2e8ec8fe57372c7394302ffb6`.
That historical state intentionally lacked the vendor ramdisk and Android bootconfig.
It is not rewritten; the next experiment starts a separate authentic Android boot from
its original pinned inputs, with no second XI/Ether memory authority.

Only **one** official build is used: aosp-android-latest-release,
`aosp_cf_arm64_only_phone-userdebug`, build **16644040**.
The authenticated boot.img, init_boot.img and vendor_boot.img are individually SHA256-
pinned in the experimental CI. The vendor_boot header is v4 and contains precisely
**one** ramdisk fragment, of type **PLATFORM** (1), at offset 0, filling the vendor
ramdisk section. No RECOVERY/unknown fragments are silently selected.

## New bounded host-side staging tool (not a CPU)
`tools/aosp_cuttlefish_stage.mjs` uses the existing AOSP v4 parser, validates
the actual Cuttlefish fragment, checks LZ4 legacy magic on both original
compressed ramdisks, and concatenates **original vendor fragment bytes first,
generic init_boot ramdisk second, with no page padding**. It appends the exact
117-byte vendor bootconfig data with Linux's 4-byte aligned bootconfig footer:
`[configuration][zero padding][size_le32][checksum_le32][#BOOTCONFIG\n]`.
The checksum is the uint32 sum of actual bootconfig bytes plus zero padding.
This is a host bootloader-input preparation only; no binary is committed.

The final initramfs is bounded to 64 MiB; the original Linux RVI1 backing to
128 MiB. Original ELF 16 MiB quota and existing finite host memory remain.
New tests verify fragment type/order, exact footer and checksum, no-overwrite,
failure for incompatible FDT, compression and malformed inputs. A separate
CI compares the prefix and suffix to the original extracted vendor and generic
bytes using GNU cmp, validates source hashes, then tests original XI/Ether ARM64
execution and direct-vs-resumed checkpoint equality.

## Independent sources and future architectural blockers
- Android official [vendor boot documentation](https://source.android.com/docs/core/architecture/partitions/vendor-boot-partitions) requires generic ramdisk loaded last, no padding, same compression.
- Android official [bootconfig specification](https://source.android.com/docs/core/architecture/bootloader/implementing-bootconfig) and [Linux kernel bootconfig ABI](https://docs.kernel.org/admin-guide/bootconfig.html) define the trailer.
- The original REVERSE virtual FDT currently fixes bootargs to `console=ttyAMA0 earlycon=pl011,0x09000000 keep_bootcon nokaslr maxcpus=1`. It does **not** contain `bootconfig`, `init=/init` nor the Cuttlefish vendor command line. The new staging tool reports that **bootconfig availability is not equivalent to bootconfig consumption by the guest kernel**. Updating those DTB args would require its own separate, checkpoint-compatible and tested change, not a silent mutation.
- Cuttlefish's 72-byte vendor DTB is only structurally recognized; it is **not** mapped directly to REVERSE's custom PL011/GIC/timer board.
- Before any Android userspace acceptance, REVERSE still needs compatible virtio block, partitions from `super.img`, first-stage fstab, kernel modules, device identities and Android services (Binder/ART/Bionic), plus ADB transport. None may be inferred from CPU transition count.

No Cuttlefish ZIP or game binaries are uploaded to the repo. Neither
`/init` nor ADB nor Wild Rift is declared running without real guest evidence.

## Verified real-build result — CI 40801279513

GitHub Actions run https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40801279513
**passed** on the same original XI/Ether interpreter, with exact Mundo-X dependency
e2778d962af22f6f1a0794058dd4685fc2573966. The new 6 native
host-stage tests, 16 AOSP parser tests and 7 ZIP-safety tests all passed.
Independent GNU cmp checked the vendor and generic raw stream boundaries.

Official build 16644040:
- Vendor PLATFORM ramdisk compressed: 18,816,074 bytes;
  SHA256 4e1e07579aa388f7fc61137b3455a5a66e521636ec60e9622d96cff39b839000.
- Generic init_boot ramdisk compressed: 2,992,408 bytes;
  SHA256 17bee5a35891bc9ff2e2d7a3a9ce26e9e7f42220aad7438e07aaf8b136d6c132.
- Verified original vendor bootconfig 117 bytes; additive checksum 11871;
  1 alignment pad byte, bootconfig size field 118, final 12-byte magic.
- Exact combined compressed vendor+generic initramfs with bootconfig footer:
  **21,808,620 bytes**, SHA256
  **21f6a9e2498f6ea30c4a4edffcd3efbaea3feec79524004492041650a1d4fb64**.
- Original vendor ramdisk expands to 42,513,920 bytes, **522 CPIO entries**.
  Module inventory includes virtio_blk.ko, virtio_pci.ko, virtio_console.ko,
  virtio_net.ko, virtio-gpu.ko, and fstab.cf.ext4/f2fs variants.

Guest test: original XI/Ether CPU ran **230,000 actual ARM64 transitions**
at EL1 with **0 architectural exceptions**, PC 0x404075c0, next opcode
0x710001bf, stop reason instruction_budget, UART empty. A direct 30,000
step run and 1 + 29,999 step restored RVG were byte-identical. Exact
final checkpoint **63,909,375 bytes**, SHA256
**d93b07e7c816f060dd813fd098b02e93b74d76c21f07379d46d105db09822324**.
Evidence artifact:
https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40801279513/artifacts/12866087082

## Next blockers in likely causal order (to be verified, not assumed)

1. Vendor kernel bootcmdline contains:
   printk.devkmsg=on audit=1 panic=-1 8250.nr_uarts=1
   binder.impl=rust cma=0 firmware_class.path=/vendor/etc/
   loop.max_part=7 init=/init bootconfig.
   REVERSE's original generated DTB still passes only console=ttyAMA0
   earlycon=pl011,0x09000000 keep_bootcon nokaslr maxcpus=1.
   Thus valid bootconfig bytes **do not prove the guest has parsed them**.
   An explicit guest-FDT bootargs capability would need versioned
   checkpoint-preserving tests; no silent modification of old RVGs.
2. Original REVERSE virtual board exposes no virtio PCI/block devices;
   modules shipped in the Cuttlefish vendor ramdisk cannot invent
   hardware. Need a bounded original virtio block PCI/MMIO device and
   real super.img/AVB metadata before Android's first-stage mounts.
3. Android /init requires actual guest EL0 entry and output/activity
   proof; empty UART and instruction_budget remain kernel-only evidence.
4. ADB requires adbd, binder/services and a backed transport; Wild Rift
   additionally requires its authorized client, graphics/GLES, input and
   network. No compatibility or performance metrics can be claimed.

Another SHA-pinned, bounded real XI/Ether continuation is tracked by
.github/workflows/aosp-cuttlefish-vendor-continue.yml.
