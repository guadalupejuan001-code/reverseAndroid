// Console-based failure evidence, separate from successful host process termination.
// Absence of these signatures is NOT evidence that Android has booted.
import assert from 'node:assert/strict';
export function analyzeGuestConsole(consoleText) {
  assert.equal(typeof consoleText, 'string', 'Console must be text');
  const lines = consoleText.replace(/\x1b\[[0-9;]*[A-Za-z]/g, '').split(/\r\n|\n|\r/)
    .map(line => line.replace(/^(?:\s*\[[^\]\r\n]*\])+\s*/, '').trim());
  const rules = [
    ['kernel_panic', /^(?:Kernel panic - not syncing|Attempted to kill init!)/],
    ['init_fatal_reboot', /^init:\s*InitFatalReboot:\s*signal\s+\d+/],
    ['required_mount_failure', /^init:\s*Failed to mount required partitions early/],
    ['system_mount_failure', /^init:\s*Failed to mount \/system:/],
    ['late_mount_failure', /^init:\s*Command 'mount_all(?: [^'\r\n]*)? --late' action=late-fs[^\r\n]* and failed: queue_fs_event\(\) failed:/],
    ['data_read_only', /^init:\s*Command '[^'\r\n]* \/data(?:\/[^'\r\n]*)?[^'\r\n]*' action=post-fs-data[^\r\n]* and failed: [^\r\n]*Read-only file system/],
    ['avb_invalid_size', /^init:\s*\[libfs_avb\]\s*Invalid hash size:/],
    ['avb_digest_failure', /^init:\s*\[libfs_avb\]\s*(?:Failed to verify vbmeta digest|vbmeta digest error isn't allowed)/],
    ['bootstrap_apex_ambiguity', /^apexd-bootstrap:\s*Found two APEX packages for module/],
    ['critical_service_failure', /^init:\s*Service [A-Za-z0-9_.-]+ has 'reboot_on_failure' option and failed, shutting down system\./],
    ['reboot_requested', /^reboot:\s*Restarting system(?:\s+with command\b|\s*$)/],
    ['reboot_failed_halt', /^Reboot failed -- System halt(?:ed)?(?:\s|$)/],
  ];
  const signals = rules.filter(([, expression]) => lines.some(line => expression.test(line))).map(([name]) => name);
  const has = name => signals.includes(name);
  const terminal = has('kernel_panic') || has('init_fatal_reboot') || has('reboot_failed_halt') || has('critical_service_failure');
  const failure = terminal || ['required_mount_failure', 'system_mount_failure', 'late_mount_failure', 'data_read_only', 'avb_invalid_size', 'avb_digest_failure', 'bootstrap_apex_ambiguity'].some(has);
  return {guest_kernel_panic:has('kernel_panic'), guest_boot_failure:failure,
    guest_terminal_failure:terminal, guest_reboot_requested:has('reboot_requested'),
    guest_failure_signals:signals, android_boot_verified:false};
}
