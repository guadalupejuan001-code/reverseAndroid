// Reclassify the authentic C12 output; no guest instructions are executed here.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync as read} from 'node:fs';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {verifySource,verifyResult} from '../tools/android_c12_audit.mjs';
const source=verifySource(process.env.C11_INPUT),out=process.env.C12_OUTPUT;
const state=read(resolve(out,'current.rvg')),text=read(resolve(out,'current.json'));
const sha=b=>createHash('sha256').update(b).digest('hex');
assert.equal(sha(state),'88f890e0237a3723cadf308ee5fcd23778d1f101e36c87e40e6ca13eb8aa0117');
const report=JSON.parse(text),pointer={checkpoint_sha256:sha(state)};
const verify=r=>verifyResult(source,state,r,pointer,300000000);
test('authentic C12 instruction budget is not Android progress after fatal reboot',()=>{
 const r=verify(report);assert.equal(r.advanced_instructions,300000000);
 assert.equal(r.stop_reason,'instruction_budget');assert.equal(r.error,'');
 assert.equal(r.guest_kernel_panic,false);assert.equal(r.guest_boot_failure,true);
 assert.equal(r.guest_terminal_failure,true);assert.equal(r.guest_reboot_requested,true);
 assert.equal(r.execution_progress_accepted,false);assert.equal(r.new_uart_bytes,1716);
 assert.equal(r.android_display_verified,false);
});
for(const message of ['init: InitFatalReboot: signal 6','Reboot failed -- System halted',
 'init: Failed to mount required partitions early ...',"reboot: Restarting system with command 'bootloader'"])
 test('positive instruction delta must not override '+message,()=>{
  const fixture=structuredClone(report);fixture.console=message; // Synthetic report variant, not a guest run.
  assert.equal(verify(fixture).execution_progress_accepted,false);
 });
test('synthetic clean budget is only execution progress, never Android screen proof',()=>{
 const fixture=structuredClone(report);fixture.console='';
 const r=verify(fixture);assert.equal(r.execution_progress_accepted,true);
 assert.equal(r.android_display_verified,false);assert.equal(r.android_framework_verified,false);
});
test('reclassification does not alter the checkpoint or raw report',()=>{
 verify(report);assert.equal(sha(read(resolve(out,'current.rvg'))),sha(state));
 assert.equal(sha(read(resolve(out,'current.json'))),sha(text));
});
