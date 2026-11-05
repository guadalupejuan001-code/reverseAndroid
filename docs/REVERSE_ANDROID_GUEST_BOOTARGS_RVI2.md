# REVERSE Android guest bootargs / RVI2 checkpoint profile

## Purpose and verified starting point

REVERSE-PC's **Android emulator is an invitado of REVERSE-PC**, not an Android
host app and not QEMU. It runs on the project's **existing XI/Ether AArch64
MinimalGuest + QuantumRuntime/InverseMemory**, while REVERSE desktop will
eventually host the emulator's display, input, storage, processes and controls.

The preserved parent experiment is draft PR #48 (commit
`d8833da991f91dc99cbfca90f192d08148dbbb3e`), which ran **10,235,000
original guest ARM64 instructions** on same-build Cuttlefish images without
a confirmed Android `/init` or ADB. Its RVG SHA-256:
`07a69f42c6b2a983196abd745614349956d2b8b02b6b9d5bae3e573c6933d9fb`.

## One minimal guest-CPU integration gate

The original `guest_device_tree()` generated only a Linux-oriented
`/chosen/bootargs`: `console=ttyAMA0 earlycon=pl011,0x09000000
keep_bootcon nokaslr maxcpus=1`. The authentic Cuttlefish build 16644040
`vendor_boot` command line also includes
`printk.devkmsg=on audit=1 panic=-1 8250.nr_uarts=1 binder.impl=rust cma=0
firmware_class.path=/vendor/etc/ loop.max_part=7 init=/init bootconfig`.
Until this option is represented in the **guest DTB**, the presence of a
correct Linux bootconfig trailer is not evidence that the guest kernel
will read it.

This experiment adds an opt-in **named** Android Cuttlefish bootargs
profile. The original Linux FDT bytes remain default. New custom bootargs
are bounded to 1,024 printable ASCII bytes. **RVI1** continues to store
historical Linux kernel, DTB and initrd bytes exactly as before. **RVI2**
stores the exact optional bootargs as a fourth immutable component, after
the existing kernel, DTB and initrd, rather than relying on future host
source code to re-create them. Restore validates RVI2 sizes, bootargs and
recreates the same FDT byte-for-byte; malformed or forged profiles are
rejected. The outer RVG2/RVG3 state structure and the original XI/Ether
memory authority are unchanged.

The host-facing native entrypoints in `reverse_guest` are:

```
reverse_guest android-cuttlefish-init kernel.bin combined_initrd.bin android.rvg
reverse_guest android-cuttlefish-dtb android.dtb 21808620
reverse_guest inspect android.rvg
reverse_guest run android.rvg android-after.rvg 10000
```

They initialize the **same** guest runtime with adaptive InverseMemory
and finite RAM/snapshot limits. `reverse_guest linux-init` preserves its
original legacy behavior. Android RVGs larger than the historical
48 MiB CLI transport can be read up to 256 MiB, but non-adaptive
legacy RVGs are still rejected beyond their individual limits.

The development probe accepts
`guest_linux_probe kernel.bin 10000 android-step.rvg
combined_initrd.bin android-cuttlefish` to perform bounded real CPU
execution and generate exact next-PC/opcode/exception evidence.

### Independent acceptance

The CI `.github/workflows/aosp-reverse-guest-bootargs.yml`:

1. verifies the SHA-pinned **10,235,000-step old RVI1** checkpoint can be
   read by the new code without altering its bytes;
2. runs native ARM64/RVG tests, including rejection of forged
   bootargs after recomputation of the outer RVG checksum;
3. acquires the exact official Cuttlefish ARM64 build **16644040**
   kernel and vendor+generic compressed initramfs, with pinned
   SHA-256 and bootconfig footer, without checking in images;
4. initializes a real RVI2 guest using the user-facing native command;
   compares its embedded FDT byte-for-byte with the standalone
   generator; independently parses `/chosen/bootargs` with `fdtget`;
5. verifies RVI2 kernel and RVG hashes with the standalone read-only
   auditor, then executes the real AOSP kernel in XI/Ether and demands
   byte-identical direct vs sliced checkpoints.

A successful fdtget and checkpoint test proves **arguments are present
in the actual guest DTB**, **not** that the kernel has parsed bootconfig,
exited EL1, mounted system/vendor, launched Android `/init` or
displayed Launcher.

## Next architectural obstacles toward Android UI

The genuine bootloader includes a vendor ramdisk with virtio modules,
but the REVERSE original virtual board currently exposes **PL011 UART,
GIC, basic timers and RAM**, not working virtio PCI/block and all Cuttlefish
devices. The 72-byte vendor DTB is not a replacement for REVERSE's FDT.
The next bounded engineering gate is to check that the kernel
actually consumes bootconfig, then implement compatible virtual
storage/partition and kernel module plumbing with exact rollback.
Only when real first-stage `/init` starts can Binder, adbd/ADB,
SurfaceFlinger, graphics/display, guest input, and a native
**Android Guest window inside REVERSE desktop** become testable.

No Android, ADB, SurfaceFlinger or Wild Rift performance is claimed
from a guest bootargs profile. No game assets are included. No pinned
dependencies are changed, no force pushes, no changes to main.

## Verified results (original XI/Ether and real official AOSP inputs)

- [Profile + legacy CI 40803716435](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40803716435) **passed**, preserving the original Mundo-X pin. The native AArch64 system suite completed **1,808 checks** with a single original 4,096-qubit QuantumRuntime authority. The read-only RVI audit suite passed 7/7 and host staging passed 6/6.
- **Original old RVG verified:** previous authentic, 10,235,000-instruction RVI1 checkpoint SHA-256 `07a69f42c6b2a983196abd745614349956d2b8b02b6b9d5bae3e573c6933d9fb` is readable under the new executable, without modifying its bytes.
- **Real named Android startup:** authentic build 16644040 kernel `f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8` and original combined vendor+generic initrd `21f6a9e2498f6ea30c4a4edffcd3efbaea3feec79524004492041650a1d4fb64` produced an RVI2 image and RVG2 state. Its **actual embedded DTB equals the standalone device tree byte-for-byte**, validated externally with `fdtget`, 217 printable command-line bytes, including `bootconfig`, `init=/init`, original REVERSE PL011 and vendor binder/cma settings. RVI2 `kernel_bytes=42031616`, `dtb_bytes=1676`, `initrd_bytes=21808620`. The initial checkpoint SHA-256 was `9c02c83796c86bed5761f6e7c413ed6a78015626889f2bbf32f843c81b5806b2`.
- **Native guest execution:** 10,000 authentic AArch64 instructions with 0 exceptions, EL1, empty UART, no Android `/init` witness. Direct 10,000 and 1+9,999-step restored guest RVGs compared **byte-identically**. Saved RVG SHA-256 `1ba9d86924a7153416aa8404407ea83d9819018fc8a6e0d448b6d54859d31d20`, [checkpoint artifact 12866534920](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40803716435/artifacts/12866534920).
- [Extended RVI2 Android guest CI 40804061395](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40804061395) **passed**. The same original GKI, official vendor+generic ramdisk, immutable RVI2 profile and original XI/Ether runtime executed **12,020,000 total ARM64 instructions** with **0 architectural exceptions**, EL1, MMU active and empty UART; reason `instruction_budget`, final PC `0x40407588`, next opcode `0xb8b0790d`, classical adaptive delta overrides 124,610. The new 65,718,203-byte RVG SHA-256 is **`174a4582b80139f7fe00213068fe3a091bb7fff15a438ca0e0e290d377934733`**, [exact artifact 12867920418](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40804061395/artifacts/12867920418). The internal `RVI2` inputs were re-audited before continued execution.

**Observed early-kernel processing (not a hardware failure):** sampled PCs stay near `0x40407588–0x404075c8` across 2M-step batches, but register `x19` decreases from `0x3021a4` to `0x258654` and inverse-memory delta count increases from 34,562 to 124,610. This is evidence of changing kernel state, not a proven stationary infinite loop; the exact kernel function and bootconfig-consumption milestone are *not* yet independently identified.

**Scope:** these are changes to native REVERSE **guest boot support in an experimental repository branch**, **not an installed REVERSE Android Guest desktop window or updated APK**. No emulator screen, Launcher, SurfaceFlinger, Android `/init` EL0 userspace or ADB can be claimed. Next independent gate should bind matching symbols/real kernel boot traces to the early scanning region, then implement the missing virtual storage/virtio and AVB/partition interfaces in XI/Ether before second-stage Android services. The genuine original first-stage `/init` ELF remains merely present in the initramfs at this point.
