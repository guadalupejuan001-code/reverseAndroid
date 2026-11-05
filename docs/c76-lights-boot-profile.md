# C76: correct the missing lights boot configuration

C75 completed six clean instruction budgets and retained every checkpoint. Its final fully admitted boundary is `7ecff7c2d460c582eeb3cbdf47fdd6686790cff2add75cd6c7437af7c9b9c3ec` at 88,895,950,532 instructions. SystemServer PID 3340 is alive. Actual Android boot is still false. Repeating budgets reaches the same missing `android.hardware.light.ILights/default` service.

A read-only examination of the original C75 stage-2 snapshot decoded the lights process's real debuggerd context: PID 4030, UID 9999, self-generated SIGABRT, and abort text `called Result::unwrap() on an Err value: Permission denied (os error 13)` (Rust backticks omitted here). The immutable init service runs as `nobody`. Its actual ramdisk bootconfig has no `androidboot.vsock_lights_port`. The Cuttlefish Rust implementation defaults that property to port 0; the Linux VSOCK bind path rejects reserved ports without `CAP_NET_BIND_SERVICE`. This identifies a configuration defect to test, rather than evidence of completed boot.

C76 adds only `androidboot.vsock_lights_port=6900` to the bootconfig trailer. Original kernel, compressed CPIO payload, existing bootconfig, boot arguments, AVB, SELinux policy, original immutable disk, 2 GiB logical RAM, timing seed, 64-instruction cadence, IRQ behavior, CPU, XI/Ether and all codecs remain unchanged. No live Android property is written.

Because this is boot configuration, C76 explicitly starts a new hardware boot epoch. It retains all completed drive writes from the **latest C75 final boundary**, rather than returning to an old blank disk or continuing a patched snapshot. The power-cycle helper uses public original `MinimalGuest` and XI APIs. It restores and byte-roundtrips the parent, initializes a new machine normally, copies only the persistent device domain via original XI transactions, and byte-roundtrips the resulting original snapshot. It does not patch RVG, MXIM or RFP bytes.

Both the emulator and an independent streamed Python comparison verified all 35,985 persistent drive pages, containing 142,384,297 nonzero XOR bytes, against the parent. Their canonical record SHA-256 is `35f3722982251f49602d18bfdf03b96c6a4cfe28ea1086445bd74d8c37f7e9ef`. Fresh RAM and GPU contain no inherited finite overrides. The new initialized root is `a39f886c126c9695024c29d5cef315d90739d39415da521e1f5e4f4e42228288`, with zero CPU instructions. This is **initialization, not boot acceptance**. All old checkpoints remain preserved.

The new epoch has a separate admission module because the historical C27 profile deliberately pins its old immutable image. That historical module and all 870 imported source files remain unchanged. C76 pins the new immutable image, ramdisk, unchanged original disk, seed and boot arguments; requires original restore/roundtrip, exact source/report binding, a clean supervised budget, original kernel/live-property observation and original scanout decoding; and calls the unchanged framework evaluator and C46 milestone. Original init must execute and be verified again within this epoch. A logo or SystemServer alone cannot pass boot acceptance.

Validation completed before execution: original 870-file integrity, the lights-profile preservation/checksum tests, a native persistent-drive fixture spanning XI batch boundaries and the last disk byte, actual parent admission, actual parent/fresh roundtrips, and the independent complete drive-record comparison. Functional boot, visible Android UI and an actual input response remain to be tested.

Reproduce after recovering the pinned C75 parent and original media:

```bash
bash scripts/build-reverseandroid.sh
bash scripts/build-reverseandroid-epoch.sh
node --expose-gc tools/reverseandroid-prepare-epoch.mjs build/source build/media/android-c27.raw build/reverseandroid/power-cycle evidence/c76-profile evidence/c76-root
node --expose-gc --max-old-space-size=5000 tools/reverseandroid-epoch.mjs run evidence/c76-root evidence/c76-1 build/reverseandroid/epoch build/media/android-c27.raw build/reverseandroid/frame COMMIT_SHA 2000000000
```

Primary references: [Cuttlefish lights implementation](https://android.googlesource.com/device/google/cuttlefish/+/93683578083eea380d2daca79f6015371dd6ac99/guest/hals/light/lights.rs), [Linux VSOCK bind implementation](https://android.googlesource.com/kernel/common/+/4da3a54f5a025846f9930354cfb80f075b9952e0/net/vmw_vsock/af_vsock.c). Actual guest evidence is in `docs/evidence/c76/`.

## First admitted execution boundary

The local new epoch completed exactly 2,000,000,000 instructions at checkpoint `8a898e4c42a35099e0b35b41417a98b970451f2853fcefbc080965f6c5bf7d01`. Original init executed and was verified again; the original decoder roundtripped the complete snapshot, the original kernel observer verified enforcing SELinux, and the unchanged C46 milestone accepted the source boundary. There is no scanout or completed framework at this early boundary. A supplemental read-only probe queried Bionic's structured property trie in PID1's verified live shared property mapping and observed `ro.boot.vsock_lights_port=6900`, serial 67108864. The lights service has not started at this boundary. Stage 2 continues the same new epoch; it does not reset the CPU again.

Independent run [42115913067](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42115913067) has passed the original engine build and new helper tests and is recovering the exact parent and media. Its initialized root and complete boundaries must be compared before claiming independent execution equality.
