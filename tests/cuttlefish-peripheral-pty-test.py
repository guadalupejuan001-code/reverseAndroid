#!/usr/bin/env python3
"""Exercise the actual broker I/O against real PTYs, without host /dev aliases.

Never runs --guest. The Android adapter, labels and actual HALs require separate
guest validation and are not claimed by these transport tests.
"""
import json
import os
import pathlib
import select
import struct
import subprocess
import sys
import tempfile
import time
import tty


def exact(fd, count, seconds=4):
    out = bytearray()
    end = time.monotonic() + seconds
    while len(out) < count:
        remaining = end - time.monotonic()
        assert remaining > 0 and select.select([fd], [], [], remaining)[0], "PTY reply timed out"
        chunk = os.read(fd, count-len(out))
        assert chunk, "Unexpected PTY EOF"
        out += chunk
    return bytes(out)


def frame(fd):
    command, size = struct.unpack("<II", exact(fd, 8))
    assert size <= 256
    return command, exact(fd, size)


def send(fd, command, body=b"", fragments=False):
    message = struct.pack("<II", command, len(body)) + body
    if fragments:
        for byte in message:
            assert os.write(fd, bytes([byte])) == 1
    else:
        assert os.write(fd, message) == len(message)


def launch(executable, state, logs):
    pairs = [os.openpty() for _ in range(3)]
    for _, slave in pairs:
        tty.setraw(slave)
    masters = [m for m, _ in pairs]
    process = subprocess.Popen(
        [executable, "--host-fds", *map(str, masters), str(state)],
        pass_fds=masters, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=logs,
    )
    for master in masters:
        os.close(master)
    return process, [s for _, s in pairs]


def stop(process, fds):
    process.terminate()
    process.wait(timeout=5)
    for fd in fds:
        os.close(fd)


def main():
    executable = str(pathlib.Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="ra-peripheral-") as directory:
        directory = pathlib.Path(directory)
        state = directory / "oemlock-state"
        with (directory / "peer.log").open("wb") as logs:
            process, fds = launch(executable, state, logs)
            oem, control, data = fds
            try:
                for command, expected in [(0, 1), (1, 1), (2, 1), (3, 0)]:
                    send(oem, command, fragments=True)
                    assert frame(oem) == (0x80000000 | command, bytes([expected]))
                for command, value, state_value in [(0, 0, 2), (3, 1, 6), (0, 1, 7)]:
                    send(oem, command, bytes([value]))
                    assert frame(oem) == (0x80000000 | command, bytes([value]))
                    assert state.read_bytes() == bytes([state_value]), "Response preceded persistence"
                send(oem, 2)
                assert frame(oem) == (0x80000002, b"\x01"), "Locked must not override carrier/device AND"
                send(control, 0, b"list-sensors", fragments=True)
                assert frame(control) == (0x80000002, b"503\n")
                initial = [frame(data) for _ in range(8)]
                expected_names = [b"acceleration", b"gyroscope", b"magnetic", b"temperature", b"proximity", b"light", b"pressure", b"humidity"]
                assert [body.split(b":")[0] for command, body in initial] == expected_names
                assert all(command == 0x80000002 and body.endswith(b"\n") for command, body in initial)
                assert initial[0][1] == b"acceleration:0:0:9.80665\n"
                periodic = [frame(data) for _ in range(5)]
                assert [body.split(b":")[0] for _, body in periodic] == [expected_names[i] for i in (0, 1, 2, 5, 6)]
                assert not select.select([control], [], [], 0)[0], "Sensor data leaked into control channel"
                send(oem, 0, b"\x00\x00")  # invalid size must terminate, not write state
                assert process.wait(timeout=4) == 1
                assert state.read_bytes() == b"\x07"
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
                for fd in fds:
                    os.close(fd)
            process, fds = launch(executable, state, logs)
            try:
                send(fds[0], 3)
                assert frame(fds[0]) == (0x80000003, b"\x01")
                assert state.read_bytes() == b"\x07"
            finally:
                stop(process, fds)
        assert state.stat().st_mode & 0o777 == 0o600
    print(json.dumps({"real_PTY_binary_roundtrip": True, "OEM_state_persisted_before_response": True,
                      "OEM_state_survives_process_restart": True, "separate_sensor_control_and_data": True,
                      "initial_and_periodic_stationary_sensor_reports": True, "invalid_request_rejected_without_state_change": True,
                      "host_device_provisioning_attempted": False, "Android_HAL_integration_claimed": False}))


if __name__ == "__main__":
    main()
