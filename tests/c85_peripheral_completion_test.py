import copy
import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from c85_peripheral_completion import verify_live_peripherals, boot_complete, POLICY


def fixture():
    actual = {"checkpoint_sha256": "a"*64}
    for k in ["real_AArch64_peer_alive", "original_oemlock_executable_alive", "original_sensors_executable_alive",
        "OEM_channel_connected_to_actual_peer", "two_separate_sensor_channels_connected_to_actual_peer"]:
        actual[k] = True
    actual["paired_FDs"] = [{"PTY_index": i, "peer_fd": fd, "actual_kernel_PTY_backlinks_verified": True}
        for i, fd in [(0,3),(1,6),(2,8)]]
    observed = {"actual_peer_READY_observed": True, "actual_exact_guest_hash_and_path_observed": True,
        "actual_private_device_filesystem_mounts": ["actual mount"], "actual_loaded_policy_unchanged_hashes": [POLICY],
        "original_HAL_Binder_found": {"android.hardware.oemlock.IOemLock/default": True,
            "android.hardware.sensors.ISensors/default": True}}
    return actual, observed


class RequiredLiveChecks(unittest.TestCase):
    def test_all_original_conditions_still_required(self):
        a,o=fixture()
        self.assertTrue(verify_live_peripherals(a,o,"a"*64))
        for key in [k for k,v in a.items() if v is True]:
            b=copy.deepcopy(a);b[key]=False
            self.assertFalse(verify_live_peripherals(b,o,"a"*64),key)
        self.assertFalse(verify_live_peripherals(a,o,"b"*64))

    def test_alias_or_unverified_backlinks_never_complete(self):
        a,o=fixture();a["paired_FDs"][2]["PTY_index"]=1
        self.assertFalse(verify_live_peripherals(a,o,"a"*64))
        a,o=fixture();a["paired_FDs"][2]["peer_fd"]=6
        self.assertFalse(verify_live_peripherals(a,o,"a"*64))
        a,o=fixture();a["paired_FDs"][2]["actual_kernel_PTY_backlinks_verified"]=False
        self.assertFalse(verify_live_peripherals(a,o,"a"*64))

    def test_console_and_original_policy_requirements_unchanged(self):
        a,o=fixture()
        for key in o:
            b=copy.deepcopy(o);b[key]=None
            self.assertFalse(verify_live_peripherals(a,b,"a"*64),key)

    def test_live_probe_failure_or_restart_is_pending_not_boot_success(self):
        a,o=fixture();a["original_oemlock_executable_alive"]=False
        live=verify_live_peripherals(a,o,"a"*64)
        gate={"source_boundary_verified":True,"android_boot_completed_verified":True,"system_server_alive_verified":True}
        self.assertFalse(boot_complete(True,live,gate))
        self.assertFalse(verify_live_peripherals(None,o,"a"*64))
        self.assertTrue(boot_complete(True,True,gate))
        for key in gate:
            g=dict(gate);g[key]=False
            self.assertFalse(boot_complete(True,True,g))
        self.assertFalse(boot_complete(False,True,gate))


if __name__ == "__main__":unittest.main()
