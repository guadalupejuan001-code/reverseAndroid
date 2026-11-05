// Synthetic console tests only; no Android boot is performed here.
import test from 'node:test';
import assert from 'node:assert/strict';
import {analyzeGuestConsole} from '../tools/android_guest_outcome.mjs';
const signatures = [
 ['Kernel panic - not syncing: fixture', 'kernel_panic'],
 ['Attempted to kill init! exitcode=0x0000000b', 'kernel_panic'],
 ['init: InitFatalReboot: signal 6', 'init_fatal_reboot'],
 ['init: Failed to mount required partitions early ...', 'required_mount_failure'],
 ['init: Failed to mount /system: No such file or directory', 'system_mount_failure'],
 ["init: Command 'mount_all --late' action=late-fs (/vendor/etc/init/init.cutf_cvm.rc:73) took 14366ms and failed: queue_fs_event() failed: Invalid code: -1", 'late_mount_failure'],
 ["init: Command 'mkdir /data/property 0700 root root encryption=Require' action=post-fs-data (/system/etc/init/hw/init.rc:721) took 22ms and failed: mkdir() failed on /data/property: Read-only file system", 'data_read_only'],
 ['init: [libfs_avb] Invalid hash size: ', 'avb_invalid_size'],
 ['init: [libfs_avb] Failed to verify vbmeta digest', 'avb_digest_failure'],
 ["init: [libfs_avb] vbmeta digest error isn't allowed", 'avb_digest_failure'],
 ['Reboot failed -- System halted', 'reboot_failed_halt'],
 ['Reboot failed -- System halt', 'reboot_failed_halt'],
];
for(const [message,signal] of signatures) {
 test('recognizes fatal/boot error '+signal+': '+message, () => {
   for(const prefix of ['', '[ 6176.388701][    T1] ', '\x1b[31m[ 6176.388701][    T1] ']) {
     const r=analyzeGuestConsole(prefix+message+'\r\n'+prefix+message+'\x1b[0m\r\n');
     assert.equal(r.guest_boot_failure,true);assert.ok(r.guest_failure_signals.includes(signal));
     assert.equal(new Set(r.guest_failure_signals).size,r.guest_failure_signals.length);
     assert.equal(r.android_boot_verified,false);
   }
 });
}
for(const message of ["reboot: Restarting system with command 'bootloader'",'reboot: Restarting system'])
 test('restart is interruption, not boot success: '+message, () => {
   const r=analyzeGuestConsole('[ 6.1][ T1] '+message);assert.equal(r.guest_reboot_requested,true);
   assert.equal(r.guest_boot_failure,false);assert.equal(r.android_boot_verified,false);
 });
for(const message of ['', 'init: starting service foo', 'init: __mount(...)=0: Success',
 'init: No InitFatalReboot observed', 'init: Reboot failed diagnostic handler installed',
 'init: [libfs_avb] verification passed'])
 test('does not invent failure or success from '+JSON.stringify(message), () => {
   const r=analyzeGuestConsole(message);assert.equal(r.guest_boot_failure,false);
   assert.equal(r.guest_reboot_requested,false);assert.equal(r.android_boot_verified,false);
 });
test('rejects non-text evidence',()=>{for(const value of [null,undefined,1,{},[]])assert.throws(()=>analyzeGuestConsole(value));});

test('a successful late mount, optional service exit and quoted diagnostic do not prove a boot failure',()=>{
 for(const text of ["init: Command 'mount_all --late' action=late-fs (init.rc:73) took 10ms and succeeded", "init: Service 'recovery-refresh' (pid 395) exited with status 254", "log: init: Command 'mount_all --late' action=late-fs failed: queue_fs_event() failed:"]){assert.equal(analyzeGuestConsole(text).guest_boot_failure,false);}
});

test('APEX variant conflict is a boot failure and a mandatory-service abort is terminal',()=>{
 const conflict=analyzeGuestConsole("[ 39.7][ T92] apexd-bootstrap: Found two APEX packages for module 'com.android.hardware.gatekeeper': first.apex and second.apex.");assert.equal(conflict.guest_boot_failure,true);assert.ok(conflict.guest_failure_signals.includes('bootstrap_apex_ambiguity'));
 const fatal=analyzeGuestConsole("[ 39.8][ T1] init: Service apexd-bootstrap has 'reboot_on_failure' option and failed, shutting down system.");assert.equal(fatal.guest_terminal_failure,true);assert.equal(fatal.guest_boot_failure,true);assert.equal(fatal.android_boot_verified,false);
 for(const text of ['app: init: Service apexd-bootstrap has failure diagnostics','apexd-bootstrap: Found pre-installed APEX /vendor/apex/valid.apex'])assert.equal(analyzeGuestConsole(text).guest_boot_failure,false);
});
