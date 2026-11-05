# Explicit normal Android boot inputs

The source vendor ramdisk is a PLATFORM fragment that also carries Android
recovery: system/bin/recovery, its libraries, update_engine_sideload, charger
images and policy resources. The generic ramdisk provides the genuine static
first-stage /init. The original startup profile did not force normal boot.

Android's first-stage init checks androidboot.force_normal_boot in bootconfig
or the kernel command line. It uses that flag to switch to first_stage_ramdisk.
The packaged normal fstab files have suffixes such as cf.ext4.cts; the original
profile did not select one of those actual suffixes.

New opt-in RVI2 profiles explicitly select:
- androidboot.force_normal_boot=1
- androidboot.fstab_suffix=cf.ext4.cts
- androidboot.slot_suffix=_a

The genuine fstab uses slotselect. An independent first-stage run exposed a
real failure without the slot suffix: libfstab rejected the fstab before
looking for partitions. The explicit slot-A profile supplies that required
input; it does not alter the fstab or bypass AVB. Older saved bootargs remain
verbatim when restored, even if they lack this fresh-boot correction.

The additional normal-no-scx variant sets initcall_blacklist=scx_init using
Linux's own supported initcall policy, avoiding only optional sched_ext setup.
The original profile, original kernel and full source initrd are preserved.
A running snapshot is never rewritten into a different boot profile.

The normal-only ramdisk builder accepts only the byte-pinned source initrd.
It keeps original lib/modules files and module configuration, all first-stage
fstabs, and the verbatim generic CPIO, including the original /init at
0x2a4000 with SHA-256
d5c901a96c6e0bad8e3ee8110dc638c9336067fdafd4378860eaead51e4299ba.
All 19 requested modules remain byte-identical. Selected vendor records keep
their exact metadata and contents. It excludes 486 recovery-specific members.
Linux's existing uncompressed newc format avoids unnecessary decompression.

The prepared normal initrd is 11,109,004 bytes versus 48,194,048 decompressed
source CPIO bytes. Its SHA-256 is
c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da.
This size difference comes from component selection, not a claimed universal
compression ratio. The unchanged source /init and each selected module are
verified before any output is written.

The actual kernel UART has confirmed the new force-normal, fstab-suffix and
no-scx command-line arguments and consumption of the real bootconfig.
This is a boot-input milestone, not proof of Android userspace or display.

The separate QEMU payload oracle reached genuine Android first-stage init,
loaded all 19 modules and switched root. This is an independent input check;
QEMU is never embedded in REVERSE and its progress is never attributed to
the XI/Ether interpreter. Its raw console and explicit negative capability
fields distinguish missing Android partitions from early ramdisk defects.

Hardware requirements remain real: this exact GKI has PCI_HOST_GENERIC=y,
VIRTIO_PCI=m and VIRTIO_BLK=m, while VIRTIO_MMIO is not configured. A future
storage device must honor that PCI driver contract or use a separately
declared compatible kernel; a virtio-mmio DT node alone cannot supply the
needed driver. Required system/vendor/metadata partitions, Android services
and graphics remain later milestones.

Primary contracts:
- https://github.com/aosp-mirror/platform_system_core/blob/android-16.0.0_r1/init/first_stage_init.cpp
- https://github.com/torvalds/linux/blob/v6.12/init/main.c

The independent normal-profile workflow verifies image preparation, native
restore parity and bounded genuine execution. It records first-stage /init
only after checking its actual entry, real first opcode and following CPU
transition. It never calls instruction counts or an initrd manifest a boot.
