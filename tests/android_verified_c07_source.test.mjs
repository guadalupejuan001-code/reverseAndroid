import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {C07_PIN,validateC07Report,verifyC07Source} from '../tools/android_verified_c07_source.mjs';

// A real, already downloaded report is required: absence is an error, not a skip.
const reportPath=process.env.C07_REPORT;
assert.ok(reportPath,'Set C07_REPORT to the pinned original C07 current.json');
const bytes=readFileSync(reportPath),original=JSON.parse(bytes);

test('real C07 report retains its 16 unique loaded modules',()=>{
  assert.equal(validateC07Report(original).length,16);
});
const changes=[
  ['missing machine',p=>delete p.guest],
  ['missing provenance',p=>delete p.provenance],
  ['snapshot hash',p=>p.checkpoint_sha256='0'.repeat(64)],
  ['restart at an earlier count',p=>p.initial_instructions--],
  ['invented progress',p=>p.advanced_instructions++],
  ['changed terminal count',p=>p.guest.instructions++],
  ['changed PC',p=>p.guest.pc='0x2a4004'],
  ['changed EL',p=>p.guest.exception_level=0],
  ['system mode disabled',p=>p.guest.system_mode=false],
  ['halted guest',p=>p.guest.halted=true],
  ['waiting guest',p=>p.guest.waiting=true],
  ['missing PCI',p=>p.guest.pci_bus=false],
  ['changed RAM',p=>p.guest.logical_ram_bytes*=2],
  ['architectural error',p=>p.stop_reason='architectural_stop'],
  ['nonempty error',p=>p.error='unsupported instruction'],
  ['reexecuted entry',p=>p.first_user_instruction_verified=true],
  ['lost prior proof',p=>p.prior_user_instruction_verified=false],
  ['wrong root',p=>p.verified_entry_checkpoint_sha256=C07_PIN.checkpoint_sha256],
  ['wrong parent',p=>p.provenance.input_checkpoint_sha256=C07_PIN.checkpoint_sha256],
  ['stop at first init again',p=>p.provenance.stop_after_init=true],
  ['kernel replacement',p=>p.provenance.kernel_sha256='0'.repeat(64)],
  ['ramdisk replacement',p=>p.provenance.ramdisk_sha256='0'.repeat(64)],
  ['disk replacement',p=>p.provenance.disk_sha256='0'.repeat(64)],
  ['disk size provenance',p=>p.provenance.disk_bytes--],
  ['disk size machine',p=>p.guest.block_image_bytes--],
  ['disabled original clock',p=>p.provenance.counter_latency=false],
  ['reseeded clock',p=>p.provenance.timing_seed_origin='host_os_getrandom'],
  ['reconfigured cadence',p=>p.provenance.counter_rate_origin='configured'],
  ['different seed',p=>p.provenance.timing_seed_sha256='0'.repeat(64)],
  ['different samples',p=>p.guest.counter_samples--],
  ['different phase',p=>p.guest.counter_subtick=0],
  ['different rate',p=>p.guest.counter_instructions_per_tick=1],
  ['different reported rate',p=>p.provenance.counter_instructions_per_tick=1],
  ['lost first-stage UART',p=>p.console=''],
  ['panic',p=>p.console+='Kernel panic - not syncing'],
  ['kill init',p=>p.console+='Attempted to kill init!'],
  ...['android_services_verified','android_display_verified','native_android_games_verified'].map(k=>[
    'unproved '+k,p=>p.provenance[k]=true])
];
for(const [name,mutate] of changes)test('rejects '+name,()=>{
  const p=structuredClone(original);mutate(p);
  assert.throws(()=>validateC07Report(p),/Verified C07 source:/);
});
test('semantic success alone cannot replace the raw report proof',()=>{
  // Even whitespace-only edits must fail the separately pinned report hash.
  assert.throws(()=>verifyC07Source(Buffer.alloc(8),Buffer.concat([bytes,Buffer.from('\n')])),/report hash differs/);
});
test('correct report cannot bless a fabricated checkpoint',()=>{
  assert.throws(()=>verifyC07Source(Buffer.alloc(8),bytes),/checkpoint hash differs/);
});
test('report type and length are bounded',()=>{
  assert.throws(()=>verifyC07Source(Buffer.alloc(8),{}),/report size\/type/);
  assert.throws(()=>verifyC07Source(Buffer.alloc(8),Buffer.alloc(65537)),/report size\/type/);
});
test('checkpoint must contain bytes',()=>{
  assert.throws(()=>verifyC07Source({},bytes),/state size\/type/);
});
