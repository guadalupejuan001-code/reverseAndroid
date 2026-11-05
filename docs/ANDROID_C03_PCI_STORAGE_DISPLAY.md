# C03: own PCI storage and persisted 2-D display

The optional PCI board extends the existing MinimalGuest interpreter. It does
not embed QEMU, replace XI/Ether or introduce another mutable RAM authority.
Historical RVI1/RVI2 images retain their original DTB and machine encoding.

The new RVI3 image explicitly records the PCI board and an external disk's
byte length and SHA-256. RVG still contains the original QuantumRuntime,
InverseMemory, CPU and finite device state. It contains no host disk path.
Restoring a disk-backed state requires an explicitly supplied matching image.
Android resolves attached images from its private catalog by content hash;
untrusted checkpoint text cannot select arbitrary host files.

PCI ECAM is at 0x30000000 with buses 0..15. The 32-bit memory aperture starts
at 0x10000000. Bus 0 device 1 exposes modern virtio-block (1af4:1042), and
device 2 exposes modern virtio-gpu (1af4:1050). Their INTx lines map to GICv2
SPI 16/17 (interrupt IDs 48/49). The device tree declares the real register
windows, DMA coherence, bus range and interrupt map. MSI-X, indirect/packed
queues, IOMMU, virgl and 3-D acceleration are not advertised.

The own implementation supports feature negotiation, BAR sizing/assignment,
split queues, DMA bounds, used rings, ISR acknowledgement and device reset.
Block reads combine an immutable verified base with XOR deltas at disjoint
logical addresses in the same XI memory. Writes never alter the source file.
Each cached 64-KiB base block is checked against its attach-time hash. DMA
invalidates address hints and exclusive monitors. Malformed requests roll back
the whole native transaction. FLUSH acknowledges XI state; durable storage is
published with the guest checkpoint, rather than by modifying the base image.

The GPU supports 2-D resources, scatter-gather backing (up to 2,048 entries),
transfers, rectangular flushes, scanout, cursor and fence responses. VRAM and
the presented plane are logical bytes in the original XI authority. A CPU
write or transfer does not become visible before the appropriate flush.
The bounded device supports four resources up to 1280x1024. The Android host
shows a PNG of those actual pixels; large frames are scaled for presentation.
No Android desktop, service or game is inferred from this hardware witness.

The built-in graphics check is an actual AArch64 program that negotiates PCI
virtio, submits GPU commands and powers off after producing four color regions.
Its starting and finished states can be imported independently of any Android
disk. Native tests compare complete snapshots across instruction boundaries,
check pixels, and bind a display byte and a RAM byte to the same quantum view.
The original 4,096-qubit system witness remains part of regression coverage.
Android instrumentation verifies the PNG's exact corner pixels, a read-only
display request, and restoration after the app process is restarted.

`tools/android_partition_disk.mjs` assembles primary/backup GPT headers and
partition tables from pinned raw or Android sparse source images. It streams
RAW/FILL/DONT_CARE chunks, verifies CRC32 chunks, preserves source payloads,
and publishes the completed image exclusively. A fixture is explicitly marked
as a fixture. Android mode requires the actual boot_a, init_boot_a, metadata,
super and vbmeta inputs; missing partitions cannot become empty fake systems.

`tools/android_partition_acquire.mjs` obtains those inputs from the same official
build 16644040 and checks ZIP member CRC32/SHA-256 and the pinned GKI hash. If
the package omits metadata.img, it formats a new writable ext4 metadata volume;
it does not invent system/vendor data or change AVB images. The separate CI
run 40910284394 acquired and independently verified the resulting GPT disk.
Its artifact includes the disk, source manifest and acquisition evidence.
Acquisition alone does not demonstrate AVB validation or Android boot.

`tools/guest_pci_boot.cpp` runs that real disk and the byte-pinned GKI and normal
ramdisk on the own board for a bounded interval. One immutable disk attachment
is retained across CPU chunks. Its fresh profile adds
`androidboot.boot_devices=30000000.pcie`, matching the root platform device;
AOSP init requires that selector to create global `/dev/block/by-name` links.
The existing profiles remain unchanged. It records an architectural stop honestly and
requires the actual 64-byte /init entry and following CPU transition before
claiming the first genuine Android instruction. Services, Android display and
native games are separate evidence fields.

Primary protocol contracts:

- https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/virtio_pci.h
- https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/virtio_blk.h
- https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/virtio_ring.h
- https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/virtio_gpu.h
- https://github.com/aosp-mirror/platform_system_core/blob/android-16.0.0_r1/init/devices.cpp
