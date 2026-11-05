// Independent read-only C09 proof. No guest bytes or execution are fabricated.
import {readFileSync as read,writeFileSync as write,copyFileSync as copy} from 'node:fs';
import {resolve} from 'node:path';
import assert from 'node:assert/strict';
import {sha,header} from './android_c08_audit.mjs';
const PIN={state:'547b7245def6e27a60201744a5837e8bbb0a4c2e9bf6eab4b2ecefa61dde9397',report:'3dc63288cb53ad4485e4b2511af2fb22a13825fc81d0aace3efe25e71becbd38',parent:'5d426d1fec117b8ef955bbbccb0e944586f36c6fe8594d9c684575dd0067e574',entry:'ec12937b7c33a3aff439f3c388bbd32b1f538e08667d0f83e61736489666bdbf',disk:'28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c',seed:'3079c6396c3fdcdaea106572f5da7c1a26a81efb32f64e8d39de7521dd5a1c75',instructions:6450570741,pc:'0x4cc358'};
const [input,out,budgetText]=process.argv.slice(2),budget=Number(budgetText);
assert.ok(input,'Expected input [output budget]');
const source=read(resolve(input,'current.rvg')),report=read(resolve(input,'current.json'));
assert.equal(sha(source),PIN.state,'C08 checkpoint hash mismatch');
assert.equal(sha(report),PIN.report,'C08 report hash mismatch');
assert.equal(sha(read(resolve(input,'parent-c07.json'))),PIN.parent,'C07 parent report mismatch');
assert.equal(sha(read(resolve(input,'entry-proof.json'))),PIN.entry,'C06 ancestor proof mismatch');
const h0=header(source),r0=JSON.parse(report);
assert.equal(h0.instructions,PIN.instructions);assert.equal(h0.pc,PIN.pc);
assert.equal(r0.error,'Unsupported AArch64 instruction 0x2f10a442 at 0x4cc358');
if(!out){console.log(JSON.stringify({source_c08_verified:true,checkpoint_sha256:PIN.state}));}
else{
  assert.ok(Number.isSafeInteger(budget)&&budget>0&&budget<=1000000000);
  const pointer=JSON.parse(read(resolve(out,'journal/latest.json')));
  assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
  const dir=resolve(out,'journal',pointer.generation),state=read(resolve(dir,'checkpoint.rvg')),r=JSON.parse(read(resolve(dir,'report.json'))),h=header(state),digest=sha(state);
  assert.equal(digest,pointer.checkpoint_sha256);assert.equal(digest,r.checkpoint_sha256);
  assert.equal(h.image,h0.image,'Immutable kernel/ramdisk changed');
  assert.equal(h.instructions,r.guest.instructions);assert.equal(h.pc,r.guest.pc);
  assert.equal(r.initial_instructions,PIN.instructions);assert.equal(r.advanced_instructions,h.instructions-PIN.instructions);
  assert.ok(r.advanced_instructions>=0&&r.advanced_instructions<=budget);
  assert.equal(r.provenance.continuation_kind,'pinned-c08-descendant');
  assert.equal(r.provenance.input_checkpoint_sha256,PIN.state);assert.equal(r.provenance.input_report_sha256,PIN.report);
  assert.equal(r.provenance.ancestor_entry_report_sha256,PIN.entry);assert.equal(r.provenance.ancestor_init_verified,true);
  assert.equal(r.provenance.disk_sha256,PIN.disk);assert.equal(r.provenance.disk_bytes,8738832384);
  assert.equal(r.provenance.timing_seed_sha256,PIN.seed);assert.equal(r.provenance.timing_seed_origin,'checkpoint');
  assert.equal(r.provenance.ram_profile_changed,false);assert.equal(r.guest.logical_ram_bytes,134217728);
  assert.equal(r.guest.block_image_bytes,8738832384);assert.equal(r.guest.counter_instructions_per_tick,64);
  assert.ok(r.guest.counter_samples>=r0.guest.counter_samples);
  assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);
  for(const key of ['android_services_verified','android_display_verified','native_android_games_verified'])assert.equal(r.provenance[key],false);
  const prior=JSON.parse(read(resolve(input,'parent-c07.json'))).console+r0.console;
  const consoleText=prior+r.console,panic=/Kernel panic - not syncing|Attempted to kill init!/.test(consoleText);
  const progress=['instruction_budget','host_wall_budget'].includes(r.stop_reason)&&r.advanced_instructions>0&&!r.error&&!panic&&!r.guest.halted;
  const proof={source_c08_verified:true,checkpoint_sha256:digest,instructions:h.instructions,advanced_instructions:r.advanced_instructions,pc:h.pc,stop_reason:r.stop_reason,error:r.error,execution_progress_accepted:progress,guest_kernel_panic:panic,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
  copy(resolve(dir,'checkpoint.rvg'),resolve(out,'current.rvg'));copy(resolve(dir,'report.json'),resolve(out,'current.json'));
  copy(resolve(input,'current.json'),resolve(out,'parent-c08.json'));copy(resolve(input,'parent-c07.json'),resolve(out,'parent-c07.json'));copy(resolve(input,'entry-proof.json'),resolve(out,'entry-proof.json'));
  write(resolve(out,'console-new.txt'),r.console);write(resolve(out,'console-combined.txt'),consoleText);
  write(resolve(out,'milestone.json'),JSON.stringify(proof,null,2)+'\n');console.log(JSON.stringify(proof,null,2));
}
