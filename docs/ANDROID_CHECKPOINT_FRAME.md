# Read-only scanout at a saved checkpoint

`guest_checkpoint_frame.cpp` restores an authenticated snapshot through the
original XI/Ether API, verifies its byte-identical round trip and reads
`MinimalGuest::display_rgb()`. It executes zero CPU instructions and verifies
the complete snapshot again after reading the pixels.

```sh
checkpoint-frame-native current.rvg android-c27.raw CHECKPOINT_SHA256 DISK_SHA256 DISK_BYTES new-frame.ppm
```

The explicit transport limit is 1 GiB. The whole disk identity is checked by
the native block-image loader. Use `-`, 64 zeroes and a disk size of `0` only
for an actual diskless snapshot. The output must be a new file; an existing
frame or source checkpoint is never overwritten.

Exit 0 exports a P6 RGB image and JSON binding its exact byte/pixel hashes,
dimensions and frame counter to the source checkpoint hash, PC and instruction
count. Exit 3 means that checkpoint has no active scanout and creates no image.
Other exits are errors. A partial output from a failed write is not evidence;
accept only a successful export whose complete hash matches its JSON.

A scanout is not sufficient evidence of Android boot. The JSON deliberately
keeps Android boot/display verification false. Corroborate the actual framework,
live PID 1 boot property and visual content separately.

The CLI regression uses the existing, explicitly non-Android PCI witness.
It checks every expected pixel after snapshot restore, original-state equality,
zero instruction execution, output exclusivity, invalid input pins before disk
open and the absence of a frame before any scanout. These fixture pixels must
never be presented as an Android screen.

`guest_native_benchmark.cpp` is a separate bounded build diagnostic. It retires
at most five million instructions from a copied source, reports the full result
hash and leaves the source file unchanged. It publishes no checkpoint and
cannot supply a healthy continuation result. The local O2/O3 comparison in
`measurements/native-build-20261030.json` produced identical complete states;
the measured difference was small and production build flags remain unchanged.
