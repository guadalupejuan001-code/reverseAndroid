// Zero-step fixtures validate evidence gates; they are not Android execution.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {PIN,verifySource,verifyResult} from '../tools/android_c10_audit.mjs';
import {C07_PIN} from '../tools/android_verified_c07_source.mjs';
assert.ok(process.env.C09_INPUT,'C09_INPUT is required');
const source=verifySource(process.env.C09_INPUT),state=readFileSync(resolve(process.env.C09_INPUT,'current.rvg'));
function fixture(){
  const r=structuredClone(source.report);r.initial_instructions=PIN.instructions;r.advanced_instructions=0;
  r.stop_reason='running';r.error='';r.console='';r.prior_user_instruction_verified=true;
  r.verified_entry_checkpoint_sha256=C07_PIN.original_entry_checkpoint_sha256;
  Object.assign(r.provenance,{continuation_kind:'pinned-c09-descendant',input_checkpoint_sha256:PIN.state,input_report_sha256:PIN.report});
  return r;
}
const verify=r=>verifyResult(source,state,r,{checkpoint_sha256:PIN.state},100000000);
test('original C09 source and R2 binder accept the real state',()=>assert.equal(source.report.guest.instructions,PIN.instructions));
test('zero-step fixture stays unverified',()=>assert.equal(verify(fixture()).execution_progress_accepted,false));
for(const [name,mutate] of [
  ['restart',r=>r.initial_instructions--],['invented progress',r=>r.advanced_instructions=1],
  ['different input',r=>r.provenance.input_checkpoint_sha256='0'.repeat(64)],
  ['different report',r=>r.provenance.input_report_sha256='0'.repeat(64)],
  ['lost entry root',r=>r.verified_entry_checkpoint_sha256=PIN.state],
  ['replayed first entry',r=>r.first_user_instruction_verified=true],
  ['changed timing',r=>r.provenance.timing_seed_origin='host'],
  ['changed phase',r=>r.guest.counter_subtick=0],
  ['false screen',r=>r.provenance.android_display_verified=true],
  ['false games',r=>r.provenance.native_android_games_verified=true],
  ['changed RAM',r=>r.provenance.ram_profile_changed=true]
])test('rejects '+name,()=>{const r=fixture();mutate(r);assert.throws(()=>verify(r));});
test('kernel panic is not a successful checkpoint',()=>{const r=fixture();r.console='Kernel panic - not syncing: test fixture';const v=verify(r);assert.equal(v.guest_kernel_panic,true);assert.equal(v.execution_progress_accepted,false);});
