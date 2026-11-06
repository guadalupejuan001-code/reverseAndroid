#!/usr/bin/env python3
"""Prepare explicit guest-file input; never edits RVG, policy or live properties.

The existing ABI2 guest executable is authenticated, copied to a new filename,
then updated through ordinary guest dd/xxd writes. The resulting whole ELF must
match the independently compiled ABI3 before execution is permitted.
"""
import argparse
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import tempfile
from difflib import SequenceMatcher

OLD_SHA = "46202a6e01e43449a9dce40aa88eb9509e5e41adf20e48c9a6c673aef8ab79da"
NEW_SHA = "548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b"
OLD_GUEST = "/data/local/tmp/reverseandroid-peer-abi2.elf"
NEW_GUEST = "/data/local/tmp/reverseandroid-peer-abi3.elf"
LOG_GUEST = "/data/local/tmp/reverseandroid-peer-abi3.log"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def root(body, marker, extra=""):
    return ("su 0 sh -c " + shlex.quote(body) + extra +
            "; r=$?; printf '\\nREVERSE_%s RC_%s\\n' " + marker + " \"$r\"\n")


def make_update(old, new):
    initial = old[:len(new)] + bytes(max(0, len(new)-len(old)))
    candidates = []
    for a, b, size in SequenceMatcher(None, old, new, autojunk=False).get_matching_blocks():
        if not size or a == b:
            continue
        count = min(len(old)-a, len(new)-b)
        base = bytearray(initial)
        base[b:b+count] = old[a:a+count]
        changed = [i for i, pair in enumerate(zip(base, new)) if pair[0] != pair[1]]
        spans = []
        for i in changed:
            if not spans or i != spans[-1][1]:
                spans.append([i, i+1])
            else:
                spans[-1][1] = i+1
        # A non-hex gutter terminates every short xxd row. Without it, the
        # actual Toybox reverse parser can consume the following line's offset.
        rows = [f"{at:x}: {new[at:min(at+16,end)].hex()}  |"
                for start, end in spans for at in range(start, end, 16)]
        wire = "\\n".join(rows) + "\\n"
        candidates.append((len(wire), a, b, count, wire))
    _, a, b, count, wire = min(candidates)
    body = (
        "set -e; umask 077; o=$1; n=$2; test -f \"$o\"; test ! -L \"$o\"; "
        "h=$(sha256sum \"$o\"); test \"${h%% *}\" = " + OLD_SHA + "; "
        "test ! -e \"$n\"; test ! -L \"$n\"; "
        "printf '\\nREVERSE_%s RC_0\\n' PTS_RX; IFS= read -r data || exit 21; "
        "cp \"$o\" \"$n\"; truncate -s " + str(len(new)) + " \"$n\"; "
        f"dd if=\"$o\" of=\"$n\" bs=1 skip={a} seek={b} count={count} conv=notrunc status=none; "
        "printf %b \"$data\" | xxd -r 1<>\"$n\"; "
        "h=$(sha256sum \"$n\"); test \"${h%% *}\" = " + NEW_SHA + "; "
        "sha256sum \"$n\""
    )
    return body, wire + "\n", {"old_offset": a, "new_offset": b, "bytes": count}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("old_build", type=pathlib.Path)
    parser.add_argument("new_build", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--host-xxd", required=True, type=pathlib.Path)
    args = parser.parse_args()
    old = (args.old_build / "cuttlefish-peripheral-peer-aarch64").read_bytes()
    new = (args.new_build / "cuttlefish-peripheral-peer-aarch64").read_bytes()
    assert sha(old) == OLD_SHA and sha(new) == NEW_SHA
    assert (args.old_build / "execution-commit.txt").read_text().strip() == "0d58e916564cdfb9dff416f1a9e5bbac13711738"
    assert (args.new_build / "execution-commit.txt").read_text().strip() == "c657a2395d8ebc9b2df2d3e0d7d9e5e28556ce32"
    body, wire, move = make_update(old, new)
    # Run the production root body, including blocking read, nontruncating
    # redirection and exact upstream Toybox reverse implementation, on temp files.
    with tempfile.TemporaryDirectory(prefix="reverseandroid-elf-update-") as temp:
        directory = pathlib.Path(temp)
        original, updated = directory / "old.elf", directory / "new.elf"
        original.write_bytes(old)
        env = dict(os.environ)
        env["PATH"] = str(args.host_xxd.resolve().parent) + os.pathsep + env["PATH"]
        tested = subprocess.run(["bash", "-c", body, "sh", str(original), str(updated)],
                                input=wire.encode("ascii"), stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, env=env, check=True)
        assert updated.read_bytes() == new
        assert original.read_bytes() == old
    first = root(body, "PTS_COPY", " sh " + shlex.quote(OLD_GUEST) + " " + shlex.quote(NEW_GUEST))
    mount = (
        "set -e; umask 077; IFS= read -r e < /sys/fs/selinux/enforce; test \"$e\" = 1; "
        "before=$(sha256sum /sys/fs/selinux/policy); before=${before%% *}; "
        "test ! -e /dev/ra-pts; mkdir -m 755 /dev/ra-pts; chcon u:object_r:device:s0 /dev/ra-pts; "
        "mount -t devpts -o newinstance,nosuid,noexec,mode=0600,ptmxmode=0600,fscontext=u:object_r:tmpfs:s0,rootcontext=u:object_r:device:s0 devpts /dev/ra-pts; "
        "grep -F ' /dev/ra-pts devpts ' /proc/mounts; ls -ldZ /dev/ra-pts; "
        "h=$(sha256sum " + NEW_GUEST + "); test \"${h%% *}\" = " + NEW_SHA + "; "
        "chmod 700 " + NEW_GUEST + "; " + NEW_GUEST + " --guest > " + LOG_GUEST + " 2>&1 < /dev/null & p=$!; "
        "ready=0; i=0; while test \"$i\" -lt 20; do if grep -q '^REVERSE_PERIPHERAL_PEER_READY$' " + LOG_GUEST +
        "; then ready=1; break; fi; kill -0 \"$p\" || break; sleep 1; i=$((i+1)); done; cat " + LOG_GUEST + "; "
        "test \"$ready\" = 1; after=$(sha256sum /sys/fs/selinux/policy); after=${after%% *}; "
        "test \"$before\" = \"$after\"; IFS= read -r e < /sys/fs/selinux/enforce; test \"$e\" = 1; "
        "printf '\\nREVERSE_%s SHA_%s\\n' PTS_POLICY_UNCHANGED \"$after\""
    )
    oem, sensors = "android.hardware.oemlock.IOemLock/default", "android.hardware.sensors.ISensors/default"
    inspection = (
        "id; ls -lZL /dev/hvc10 /dev/hvc18 /dev/hvc19; service check android.hardware.light.ILights/default; "
        "ok=0; i=0; while test \"$i\" -lt 20; do o=$(service check " + oem + "); s=$(service check " + sensors + "); "
        "printf '%s\\n%s\\n' \"$o\" \"$s\"; if test \"$o\" = 'Service " + oem + ": found' && test \"$s\" = 'Service " + sensors +
        ": found'; then ok=1; break; fi; sleep 1; i=$((i+1)); done; cat " + LOG_GUEST + "; test \"$ok\" = 1"
    )
    commands = [first, wire, root(mount, "PTS_ACTIVATION"), root(inspection, "PTS_INSPECT")]
    markers = ["REVERSE_PTS_RX", "REVERSE_PTS_COPY", "REVERSE_PTS_ACTIVATION", "REVERSE_PTS_INSPECT"]
    args.output.mkdir(parents=True, exist_ok=False)
    phases = []
    for i, (text, marker) in enumerate(zip(commands, markers)):
        data = text.encode("ascii")
        assert 0 < len(data) <= 4096
        if i != 1:
            subprocess.run(["bash", "-n"], input=data, check=True)
        path = args.output / f"input-{i}.txt"
        path.write_bytes(data)
        phases.append({"input_file": path.name, "input_bytes": len(data), "input_sha256": sha(data),
                       "response_marker": marker, "failure_markers": ["REVERSE_PTS_COPY"],
                       "data_for_waiting_guest_read": i == 1, "instruction_budget": 1000000000})
    proof = {"schema": "reverseandroid-private-PTY-controlled-input-proposal-v1", "phases": phases,
             "original_guest_ELF_SHA256": OLD_SHA, "ELF_SHA256": NEW_SHA,
             "guest_ELF_path": NEW_GUEST, "copy_from_authenticated_original_file": move,
             "host_exact_Toybox_reverse_and_original_root_body_test_passed": True,
             "host_source_old_ELF_unmodified": True, "host_reconstructed_whole_ELF_equals_compiled_target": True,
             "host_device_mount_or_SELinux_provisioning_attempted": False,
             "guest_integration_claimed": False, "RVG_or_policy_or_properties_modified": False}
    (args.output / "proposal.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps({"phase_sizes": [p["input_bytes"] for p in phases], "host_whole_ELF_reconstruction_passed": True,
                      "guest_integration_passed": False, "copy": move}))


if __name__ == "__main__":
    main()
