import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {C07_PIN as pin,hash} from '../tools/android_verified_c07_source.mjs';
import {classifyC08Report,bindC08Snapshot} from '../tools/android_c08_result.mjs';

assert.ok(process.env.C07_REPORT&&process.env.C07_STATE,'Set C07_REPORT and C07_STATE to the original artifact');
const source=JSON.parse(readFileSync(process.env.C07_REPORT));
// Synthetic report fixtures test classification only; they are NOT Android runs.
const fixture=()=>{
  const p=structuredClone(source);
  p.initial_instructions=pin.instructions;p.advanced_instructions=100;
  p.guest.instructions=pin.instructions+100;
  p.provenance.input_checkpoint_sha256=pin.checkpoint_sha256;
  p.checkpoint_sha256='1'.repeat(64);p.console='';
  return p;
};
test('clean continuation does not need to replay first-stage UART',()=>{
  const result=classifyC08Report(fixture(),source);
  assert.equal(result.execution_progress_verified,true);
  assert.equal(result.original_entry_checkpoint_sha256,pin.original_entry_checkpoint_sha256);
  assert.equal(result.previously_loaded_modules.length,16);
  assert.deepEqual(result.newly_loaded_modules,[]);
  assert.equal(result.android_display_verified,false);
  assert.equal(result.native_android_games_verified,false);
});
test('only newly observed loaded modules establish the next module milestone',()=>{
  const p=fixture();p.console='init: Loaded kernel module /lib/modules/virtio_pci.ko\n'.repeat(2);
  const result=classifyC08Report(p,source);
  assert.deepEqual(result.newly_loaded_modules,['/lib/modules/virtio_pci.ko']);
  assert.equal(result.all_observed_loaded_modules.length,17);
});
test('a module being loaded is not already loaded',()=>{
  const p=fixture();p.console="init: Loading module /lib/modules/virtio_pci.ko with args ''\n";
  assert.deepEqual(classifyC08Report(p,source).newly_loaded_modules,[]);
});
for(const reason of ['architectural_stop','guest_kernel_panic','no_instruction_progress','waiting_for_device_or_interrupt','host_interrupted','halted'])
  test('preserves but does not certify '+reason,()=>{
    const p=fixture();p.stop_reason=reason;
    assert.equal(classifyC08Report(p,source).execution_progress_verified,false);
  });
test('no-progress budget cannot pass',()=>{
  const p=fixture();p.advanced_instructions=0;p.guest.instructions=pin.instructions;
  assert.equal(classifyC08Report(p,source).execution_progress_verified,false);
});
test('panic text overrides a nominally clean budget',()=>{
  for(const text of ['Kernel panic - not syncing','Attempted to kill init!']){
    const p=fixture();p.console=text;
    assert.equal(classifyC08Report(p,source).execution_progress_verified,false);
  }
});
test('nonempty error overrides a nominally clean budget',()=>{
  const p=fixture();p.error='unsupported operation';
  assert.equal(classifyC08Report(p,source).execution_progress_verified,false);
});
const corruptions=[
  ['C06 replay',p=>p.initial_instructions=6250795356],
  ['negative count',p=>p.advanced_instructions=-1],
  ['budget exceeded',p=>p.advanced_instructions=pin.instructions_budget+1],
  ['counter mismatch',p=>p.guest.instructions--],
  ['root overwritten by C07',p=>p.verified_entry_checkpoint_sha256=pin.checkpoint_sha256],
  ['new entry callback',p=>p.first_user_instruction_verified=true],
  ['wrong source',p=>p.provenance.input_checkpoint_sha256=pin.original_entry_checkpoint_sha256],
  ['RAM changed',p=>p.guest.logical_ram_bytes*=2],
  ['EL2',p=>p.guest.exception_level=2],
  ['clock reset',p=>p.guest.counter_samples=0],
  ['clock phase',p=>p.guest.counter_subtick=64],
  ['kernel changed',p=>p.provenance.kernel_sha256='0'.repeat(64)],
  ['display asserted',p=>p.provenance.android_display_verified=true],
];
for(const [name,mutate] of corruptions)test('rejects '+name,()=>{
  const p=fixture();mutate(p);assert.throws(()=>classifyC08Report(p,source),/C08 result:/);
});
const state=readFileSync(process.env.C07_STATE),pointer={checkpoint_sha256:pin.checkpoint_sha256};
test('native snapshot binder checks the real C07 bytes without executing them',()=>{
  assert.equal(bindC08Snapshot(state,source,pointer),pin.checkpoint_sha256);
});
for(const [name,mutate] of [
  ['count',p=>p.guest.instructions++],['PC',p=>p.guest.pc='0x2a4004'],
  ['EL',p=>p.guest.exception_level=0],['clock samples',p=>p.guest.counter_samples++],
  ['clock phase',p=>p.guest.counter_subtick=0],['clock cadence',p=>p.guest.counter_instructions_per_tick=1]])
  test('native binding rejects false report '+name,()=>{
    const p=structuredClone(source);mutate(p);assert.throws(()=>bindC08Snapshot(state,p,pointer),/native/);
  });
test('modified pointer cannot select the snapshot',()=>{
  assert.throws(()=>bindC08Snapshot(state,source,{checkpoint_sha256:'0'.repeat(64)}),/mismatch/);
});
test('modified snapshot bytes cannot reuse the original report',()=>{
  const b=Buffer.from(state);b[b.length-1]^=1;
  assert.throws(()=>bindC08Snapshot(b,source,pointer),/mismatch/);
});
test('forged section sizes fail even after recomputing the external hash',()=>{
  const b=Buffer.from(state);b.writeBigUInt64LE(1n,8+42*8);
  const sha=hash(b),p=structuredClone(source);p.checkpoint_sha256=sha;
  assert.throws(()=>bindC08Snapshot(b,p,{checkpoint_sha256:sha}),/section extents/);
});
test('unsafe integer section size is rejected',()=>{
  const b=Buffer.from(state);b.writeBigUInt64LE((1n<<63n),8+39*8);
  const sha=hash(b),p=structuredClone(source);p.checkpoint_sha256=sha;
  assert.throws(()=>bindC08Snapshot(b,p,{checkpoint_sha256:sha}),/overflow/);
});
