# AOSP boot image preflight (experimental)

No Android boot or application execution is claimed. This is one host-side
packaging step between Android GKI boot containers and REVERSE's existing
MinimalGuest::load_linux_image(Image, initrd) path. It does not create a second
CPU, XI/Ether state store, QuantumRuntime, InverseMemory or guest RAM authority.

The parser follows AOSP's published structures boot_img_hdr_v3/v4,
vendor_boot_img_hdr_v3/v4 and v4 vendor ramdisk table entries:

https://android.googlesource.com/platform/system/tools/mkbootimg/+/refs/heads/main/include/bootimg/bootimg.h

## Usage

With a lawful, mutually compatible AOSP set, run from the repo root:

    node tools/aosp_boot_preflight.mjs --boot /path/to/boot.img \
      --vendor-boot /path/to/vendor_boot.img \
      --init-boot /path/to/init_boot.img \
      --extract-dir /new/output-directory

Omit --init-boot when the generic ramdisk is already in boot.img (some
Android 11/12 configurations). Omit --extract-dir for read-only inspection.

This checks magics, header versions, fixed 4096-byte boot pages, vendor page
alignment and reported section boundaries. It records SHA-256 hashes
for input containers and extracted kernel, generic/vendor ramdisks, DTB,
signature, v4 ramdisk table and bootconfig, without rewriting these bytes.
It reads init_boot v4 and reports ambiguous v4 vendor ramdisk fragments.
Output kernel.bin, generic_ramdisk.bin, vendor_ramdisk.bin, vendor_dtb.bin,
bootconfig.bin and manifest.json are written only to a new directory.
The 512 MiB per-file limit applies to this host tool; it does not increase guest RAM.

Limits: Container structure is not authenticity or AVB verification. The ARM64
Linux Image magic is only an indicator; compressed kernels, board mismatch,
missing GKI modules, vendor ramdisk fragment selection, bootconfig trailer,
initramfs concatenation/decompression, compatible DTB, Binder/ART, partition
mounts, services, graphics, audio, ADB and Wild Rift are NOT implemented by
this tool. Do not blindly concatenate vendor fragments or substitute a vendor
DTB into REVERSE's virtual board. The manifest deliberately sets
android_boot=false, adb=false, wild_rift=false.

## Reproducible validation

    node --test tests/aosp_boot_preflight.test.mjs

The tests use synthetic boot-container fixtures ONLY: section offset/hash
equality, AOSP v3/v4 fields, init_boot, corrupted image bounds, invalid vendor
fragments, version mismatch, and no-overwrite extraction. A local read-only
parity experiment additionally wrapped the exact previously verified Linux 6.1
Image (SHA-256
1d544da1f95bf33e79ca377104ca328d55f64f2f3fa5997f6b5d987474c2b634)
and initramfs (SHA-256
f97f00df11ae12893b71818931c1c919c6e03edffa2c88d7c3026985a25eb0e3)
in a SYNTHETIC v4 container, then extracted them byte-identically. That
experiment is a packaging parity test, NOT an Android image or boot.

The actual Linux guest /init witness remains PR #32 workflow
https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40779358953
The original Wild Rift APK/splits are not present; PR #30 remains independent.

## Next gate

Source and hash-pin an authentic open-source compatible ARM64 AOSP
boot.img, vendor_boot.img, and init_boot.img if applicable, plus required
system/vendor partition images. Compare section extraction with official AOSP
unpack_bootimg independently, then connect actual GKI kernel, ramdisk assembly
and bootconfig to the original XI/Ether guest, stopping at the first observed
opcode or device failure. Only count Android kernel and /init boot after a
real guest UART trace and saved checkpoint prove it.

## Authentic Android Common GKI kernel-only acceptance gate (next experimental cycle)

A complete compatible Android image set has NOT been provided. To avoid
substituting an unverified Linux kernel or manufacturing an Android boot,
the additive `--gki-kernel-only` mode inspects an authentic GKI `boot.img`
**without** falsely requiring vendor_boot. It only extracts the raw ARM64
Linux Image and records its actual size, container/kernel SHA-256 and Linux
Image header fields, plus whether REVERSE's current **combined 16 MiB**
Image/DTB/initramfs budget is already certainly exceeded by the kernel.
It never claims successful execution.

    node tools/aosp_boot_preflight.mjs --boot /path/to/boot.img \
      --gki-kernel-only --extract-dir /new/output-directory

`aosp-gki-real.yml` requests a **real public Android Common** GKI artifact
identified by source tag `android14-6.1-2026-09_r5`, source commit
`6d8630162a371a04152cf5778f8a5b90e49f30a6`, official build `16707926`,
target `kernel_aarch64`:

https://ci.android.com/builds/submitted/16707926/kernel_aarch64/latest/raw/boot.img

The CI hashes the **bytes actually downloaded**, never assumes a SHA from
an unperformed download, refuses missing/non-ARM64 images, and requires
byte-for-byte parity with the independent AOSP
`unpack_bootimg.py` at commit
`b78811e89d774685e9262339a8fedcbe2fc89efe`.
The independent extractor is fetched only as a CI oracle, not integrated
into REVERSE or XI/Ether. If public access to the official artifact fails,
the workflow must **fail**, and evidence may not be reported as a pass.
No kernel, boot partition or unrelated game binary is committed.

This is a GKI kernel-only extraction gate, **not** a bootable Android
system: matching board/vendor_boot, init_boot generic ramdisk, partitions,
device drivers, DTB/bootconfig, Android userspace and ADB remain unverified.
The AOSP kernel could also be too large for REVERSE's existing image budget;
that compatibility limit must be measured before making a narrowly scoped
loader change. No UI/FPS/gaming claims follow from this check.


## Siguiente fase: cargador Linux real sin el límite ELF de 16 MiB (PR #42)

La sección anterior documenta el límite **histórico** de 16 MiB del PR #41.
El nuevo experimento preserva ese límite para ELF, pero introduce presupuestos
Linux **64 MiB kernel / 64 MiB initrd / 128 MiB RVI1** de forma independiente,
con restauración RVG2/3 y retrocompatibilidad. El modo `--gki-kernel-only`
reporta los nuevos límites; nunca afirma ejecutar Android.

El workflow `.github/workflows/aosp-gki-xi-execution.yml` valida el SHA256
original `boot.img` `58827ec86eb7621e4ac0c16640622119645cc20c65261fb74d82fc68a735ef63`,
extrae el Image de 35,777,024 bytes, con SHA256
`8d8cf5254f0823655e06ffee100d9c19abbf4ce33adb107b66da3fdb3742e0be`,
compila el intérprete **XI/Ether original** contra Mundo-X fijado, ejecuta
pruebas ELF/RVG/regresión y compara byte a byte un checkpoint directo con
otro obtenido tras restauración. Solo se informará la instrucción realmente
alcanzada, los logs UART y el motivo preciso de parada.

Esto no crea una segunda autoridad de RAM ni hace una copia de QEMU. Hasta que
existan ramdisks y particiones Android compatibles, una ejecución de GKI sin
`/init` Android sigue siendo **una prueba de arranque del kernel**, no de Android
completo ni ADB. No hay FPS ni ejecución de Wild Rift.

## Continued authentic GKI kernel after 5.5 million instructions (experiment)

The original XI/Ether guest RVG2 at 5,504,096 instructions is pinned to
SHA-256 f483938bdf78117eb3a8f415087daf3a3bec4f48c78ffa568e0a89e43ccec843.
The immutable RVI1 contains the real official Android Common kernel Image
(35,777,024 bytes, SHA-256
8d8cf5254f0823655e06ffee100d9c19abbf4ce33adb107b66da3fdb3742e0be),
a 1,456-byte REVERSE virtual-board DTB, and **zero initrd bytes**.
Android first-stage /init cannot be claimed from this kernel-only image.

The new read-only tools/audit_gki_rvg.mjs validates RVG2/3 and RVI1
section extents, the SHA-256 of the embedded kernel and RVG, and the
internal RVG FNV-1a checksum. It never executes guest code or modifies
QuantumRuntime/InverseMemory. Five synthetic-metadata regressions
cover wrong kernel, wrong checkpoint, corruption and out-of-bounds sizes.

The separate .github/workflows/aosp-gki-after-5m.yml pulls the exact
prior CI artifact by run ID, verifies both hashes, and replays 50,000
original XI/Ether instructions both directly and split 1 + 49,999.
The two resulting RVG states must be byte-identical. Only after that
does it continue in ten bounded batches of up to 3,000,000 transitions
each, recording actual PC, next opcode, architectural stop, console
and the exact saved RVG. An instruction-budget stop never proves boot.

Unchanged limitations: no matching init_boot, vendor_boot, system/vendor
partitions, Binder, Android /init, ADB, graphics, Wild Rift execution
or performance evidence. The 16 MiB ELF limit and all pinned dependencies
remain unchanged; the current Linux-only kernel quota is 64 MiB.
