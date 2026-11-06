"""Keep transient HAL observations separate from original native admission.

The full live checks remain mandatory for completion. An admitted boundary
with a restarting HAL is retained and can continue without another UART input.
"""
POLICY = "e7dc888c8a4c5ae6f48a175ace91e63ab262e64d4d6f6799a015fed5b4c32f97"


def verify_live_peripherals(actual, observed, checkpoint_sha256):
    if not isinstance(actual, dict) or actual.get("checkpoint_sha256") != checkpoint_sha256:
        return False
    if not all(actual.get(k) is True for k in ["real_AArch64_peer_alive", "original_oemlock_executable_alive",
        "original_sensors_executable_alive", "OEM_channel_connected_to_actual_peer",
        "two_separate_sensor_channels_connected_to_actual_peer"]):
        return False
    pairs = actual.get("paired_FDs", [])
    if not pairs or not all(x.get("actual_kernel_PTY_backlinks_verified") is True for x in pairs):
        return False
    if len({x.get("PTY_index") for x in pairs}) != 3 or len({x.get("peer_fd") for x in pairs}) != 3:
        return False
    services = observed.get("original_HAL_Binder_found")
    if not isinstance(services, dict):
        return False
    return (observed.get("actual_peer_READY_observed") is True
        and observed.get("actual_exact_guest_hash_and_path_observed") is True
        and services.get("android.hardware.oemlock.IOemLock/default") is True
        and services.get("android.hardware.sensors.ISensors/default") is True
        and bool(observed.get("actual_private_device_filesystem_mounts"))
        and observed.get("actual_loaded_policy_unchanged_hashes") == [POLICY])


def boot_complete(hardware_commands_completed, live_verified, gate):
    return (hardware_commands_completed is True and live_verified is True
        and gate.get("source_boundary_verified") is True
        and gate.get("android_boot_completed_verified") is True
        and gate.get("system_server_alive_verified") is True)
