import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {PIN,sha,verifySource,verifyResult} from '../tools/android_c08_audit.mjs';
const dir=process.env.C07_INPUT;
assert.ok(dir,'C07_INPUT must identify the downloaded immutable C07 artifact');
const original=readFileSync(resolve(dir,'current.rvg'));
const report=readFileSync(resolve(dir,'current.json'));
const entry=readFileSync(resolve(dir,'entry-proof.json'));
const source=verifySource(original,report,entry);
test('accepts the exact genuine C07 input and C06 ancestry',()=>assert.equal(source.report.guest.instructions,PIN.instructions));
for(const [label,position] of [['header',40],['payload',original.length-1]])test(`rejects changed source ${label}`,()=>{
  const bad=Buffer.from(original);bad[position]^=1;
  assert.throws(()=>verifySource(bad,report,entry),/checkpoint hash mismatch/);
});
test('rejects an altered source report',()=>assert.throws(()=>verifySource(original,Buffer.concat([report,Buffer.from(' ')]),entry),/report hash mismatch/));
test('rejects an altered ancestor proof',()=>assert.throws(()=>verifySource(original,report,Buffer.concat([entry,Buffer.from(' ')])),/ancestor proof hash mismatch/));
// Synthetic descendant ONLY for validator tests. Never an emulator result.
const synthetic=Buffer.from(original);synthetic.writeBigUInt64LE(BigInt(PIN.instructions+25),40);
const digest=sha(synthetic),pointer={generation:'generation-2',checkpoint_sha256:digest};
const result={initial_instructions:PIN.instructions,advanced_instructions:25,guest:{...source.report.guest,instructions:PIN.instructions+25},checkpoint_sha256:digest,stop_reason:'instruction_budget',error:'',console:'',first_user_instruction_verified:false,prior_user_instruction_verified:false,provenance:{continuation_kind:'pinned-c07-descendant',input_checkpoint_sha256:PIN.checkpoint,input_report_sha256:PIN.report,ancestor_entry_checkpoint_sha256:PIN.entryCheckpoint,ancestor_entry_report_sha256:PIN.entry,ancestor_init_verified:true,timing_seed_origin:'checkpoint',timing_seed_sha256:PIN.seed,disk_sha256:PIN.disk,disk_bytes:8738832384,ram_profile_changed:false,android_services_verified:false,android_display_verified:false,native_android_games_verified:false}};
const check=r=>verifyResult(source,synthetic,r,pointer,100);
test('accepts a consistent synthetic tuple (not execution evidence)',()=>assert.equal(check(result).execution_progress_accepted,true));
const mutations=[
 ['restarted counter',r=>r.initial_instructions--],['wrong delta',r=>r.advanced_instructions++],
 ['wrong native PC',r=>r.guest.pc='0x0'],['wrong native count',r=>r.guest.instructions++],
 ['wrong report hash',r=>r.checkpoint_sha256='0'.repeat(64)],
 ['wrong parent',r=>r.provenance.input_checkpoint_sha256='0'.repeat(64)],
 ['wrong parent report',r=>r.provenance.input_report_sha256='0'.repeat(64)],
 ['wrong ancestor',r=>r.provenance.ancestor_entry_checkpoint_sha256='0'.repeat(64)],
 ['wrong ancestor proof',r=>r.provenance.ancestor_entry_report_sha256='0'.repeat(64)],
 ['reseeded clock',r=>r.provenance.timing_seed_origin='host'],
 ['wrong seed',r=>r.provenance.timing_seed_sha256='0'.repeat(64)],
 ['wrong disk',r=>r.provenance.disk_sha256='0'.repeat(64)],
 ['wrong disk size',r=>r.guest.block_image_bytes--],['wrong RAM',r=>r.guest.logical_ram_bytes*=2],
 ['wrong counter rate',r=>r.guest.counter_instructions_per_tick=1],['regressed clock samples',r=>r.guest.counter_samples=0],
 ['replayed entry claim',r=>r.first_user_instruction_verified=true],
 ['unlabelled ancestor claim',r=>r.prior_user_instruction_verified=true],
 ['unverified display claim',r=>r.provenance.android_display_verified=true]
];
for(const [name,mutate] of mutations)test(`rejects ${name}`,()=>{const r=structuredClone(result);mutate(r);assert.throws(()=>check(r));});
test('rejects a budget overrun',()=>assert.throws(()=>verifyResult(source,synthetic,result,pointer,24)));
test('rejects a pointer path escape',()=>assert.throws(()=>verifyResult(source,synthetic,result,{...pointer,generation:'../escape'},100)));
test('rejects a pointer hash mismatch',()=>assert.throws(()=>verifyResult(source,synthetic,result,{...pointer,checkpoint_sha256:'0'.repeat(64)},100)));
for(const reason of ['architectural_stop','no_instruction_progress','host_interrupted','waiting_for_device_or_interrupt','guest_kernel_panic'])test(`does not accept ${reason} as completed progress`,()=>assert.equal(check({...result,stop_reason:reason}).execution_progress_accepted,false));
test('does not accept a hidden kernel panic',()=>assert.equal(check({...result,console:'Kernel panic - not syncing: test'}).execution_progress_accepted,false));
test('counts only new distinct loaded module messages',()=>{const r={...result,console:'init: Loaded kernel module /lib/modules/virtio_pci.ko\ninit: Loaded kernel module /lib/modules/virtio_pci.ko\n'};assert.deepEqual(check(r).new_loaded_modules,['/lib/modules/virtio_pci.ko']);});
