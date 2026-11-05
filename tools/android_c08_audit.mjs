// Read-only host evidence. Never changes a guest state or supplies guest bytes.
import {readFileSync,writeFileSync,copyFileSync,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
export const PIN=Object.freeze({checkpoint:'845fb40da060147a23449ecfd4e7a3d8b11def8d66696093c6e6728798b82694',report:'5d426d1fec117b8ef955bbbccb0e944586f36c6fe8594d9c684575dd0067e574',entry:'ec12937b7c33a3aff439f3c388bbd32b1f538e08667d0f83e61736489666bdbf',entryCheckpoint:'4c20cd09e353f82b032c17636ae2d72cec6988df5a5b00afbd756464c4dcd575',seed:'3079c6396c3fdcdaea106572f5da7c1a26a81efb32f64e8d39de7521dd5a1c75',disk:'28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c',instructions:6350795356,pc:'0xffffffc0800dfa28'});
export const sha=b=>createHash('sha256').update(b).digest('hex');
export function header(b){
  assert.ok(b.length>=360&&/^RVG[23]$/.test(b.subarray(0,4).toString()),'Invalid RVG header');
  const at=8+(b[3]===51?44:43)*8,n=Number(b.readBigUInt64LE(8+39*8));
  assert.ok(Number.isSafeInteger(n)&&n>=128&&n<=b.length-at,'Invalid immutable extent');
  return {instructions:Number(b.readBigUInt64LE(40)),pc:'0x'+b.readBigUInt64LE(24).toString(16),image:sha(b.subarray(at,at+n))};
}
export function verifySource(state,report,entry){
  assert.equal(sha(state),PIN.checkpoint,'C07 checkpoint hash mismatch');
  assert.equal(sha(report),PIN.report,'C07 report hash mismatch');
  assert.equal(sha(entry),PIN.entry,'C06 ancestor proof hash mismatch');
  const r=JSON.parse(report),h=header(state);
  assert.equal(h.instructions,PIN.instructions);assert.equal(h.pc,PIN.pc);
  assert.equal(r.checkpoint_sha256,PIN.checkpoint);
  assert.equal(r.verified_entry_checkpoint_sha256,PIN.entryCheckpoint);
  assert.equal(r.prior_user_instruction_verified,true);
  assert.equal(r.guest.exception_level,1);assert.equal(r.guest.logical_ram_bytes,134217728);
  assert.ok(r.console.includes('init: init first stage started!'));
  assert.doesNotMatch(r.console,/Kernel panic - not syncing|Attempted to kill init!/);
  return {report:r,header:h};
}
const modules=text=>[...new Set([...text.matchAll(/init: Loaded kernel module (\/lib\/modules\/[A-Za-z0-9_.-]+)/g)].map(m=>m[1]))];
export function verifyResult(source,state,r,pointer,budget){
  assert.ok(Number.isSafeInteger(budget)&&budget>0&&budget<=1000000000,'Invalid result budget');
  const digest=sha(state),h=header(state);
  assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
  assert.equal(digest,pointer.checkpoint_sha256,'Journal pointer hash mismatch');
  assert.equal(digest,r.checkpoint_sha256,'Report hash mismatch');
  assert.equal(h.image,source.header.image,'Original immutable image changed');
  assert.equal(h.instructions,r.guest.instructions,'Native instruction count mismatch');
  assert.equal(h.pc,r.guest.pc,'Native PC mismatch');
  assert.equal(r.initial_instructions,PIN.instructions,'C07 was restarted');
  assert.equal(r.advanced_instructions,h.instructions-PIN.instructions);
  assert.ok(Number.isSafeInteger(r.advanced_instructions)&&r.advanced_instructions>=0&&r.advanced_instructions<=budget,'Invalid transition count');
  assert.equal(r.provenance.continuation_kind,'pinned-c07-descendant');
  assert.equal(r.provenance.input_checkpoint_sha256,PIN.checkpoint);
  assert.equal(r.provenance.input_report_sha256,PIN.report);
  assert.equal(r.provenance.ancestor_entry_checkpoint_sha256,PIN.entryCheckpoint);
  assert.equal(r.provenance.ancestor_entry_report_sha256,PIN.entry);
  assert.equal(r.provenance.ancestor_init_verified,true);
  assert.equal(r.provenance.timing_seed_origin,'checkpoint');
  assert.equal(r.provenance.timing_seed_sha256,PIN.seed);
  assert.equal(r.provenance.disk_sha256,PIN.disk);
  assert.equal(r.provenance.disk_bytes,8738832384);
  assert.equal(r.provenance.ram_profile_changed,false);
  assert.equal(r.guest.logical_ram_bytes,134217728);
  assert.equal(r.guest.block_image_bytes,8738832384);
  assert.equal(r.guest.counter_instructions_per_tick,64);
  assert.ok(r.guest.counter_samples>=source.report.guest.counter_samples);
  assert.equal(r.first_user_instruction_verified,false,'Do not claim to execute the entry again');
  assert.equal(r.prior_user_instruction_verified,false,'Ancestor evidence must remain separately labelled');
  for(const key of ['android_services_verified','android_display_verified','native_android_games_verified'])assert.equal(r.provenance[key],false);
  const combined=source.report.console+r.console,all=modules(combined),old=new Set(modules(source.report.console));
  const newModules=all.filter(x=>!old.has(x));
  const panic=/Kernel panic - not syncing|Attempted to kill init!/.test(combined);
  const normal=['instruction_budget','host_wall_budget'].includes(r.stop_reason)&&r.advanced_instructions>0&&!r.error&&!panic&&!r.guest.halted;
  return {source_c07_verified:true,ancestor_c06_verified:true,checkpoint_sha256:digest,instructions:h.instructions,advanced_instructions:r.advanced_instructions,pc:h.pc,stop_reason:r.stop_reason,error:r.error,execution_progress_accepted:normal,guest_kernel_panic:panic,new_loaded_modules:newModules,loaded_module_count:all.length,loaded_modules:all,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
}
export function run(input,out,budget){
  const state=readFileSync(resolve(input,'current.rvg')),report=readFileSync(resolve(input,'current.json')),entry=readFileSync(resolve(input,'entry-proof.json'));
  const source=verifySource(state,report,entry);
  if(!out)return {source_c07_verified:true,checkpoint_sha256:PIN.checkpoint,instructions:PIN.instructions};
  const pointer=JSON.parse(readFileSync(resolve(out,'journal/latest.json')));
  assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
  const dir=resolve(out,'journal',pointer.generation),next=readFileSync(resolve(dir,'checkpoint.rvg')),r=JSON.parse(readFileSync(resolve(dir,'report.json')));
  const milestone=verifyResult(source,next,r,pointer,budget);
  copyFileSync(resolve(dir,'checkpoint.rvg'),resolve(out,'current.rvg'));
  copyFileSync(resolve(dir,'report.json'),resolve(out,'current.json'));
  copyFileSync(resolve(input,'current.json'),resolve(out,'parent-c07.json'));
  copyFileSync(resolve(input,'entry-proof.json'),resolve(out,'entry-proof.json'));
  writeFileSync(resolve(out,'console-new.txt'),r.console);
  writeFileSync(resolve(out,'console-combined.txt'),source.report.console+r.console);
  writeFileSync(resolve(out,'milestone.json'),JSON.stringify(milestone,null,2)+'\n');
  return milestone;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{const [input,out,budget]=process.argv.slice(2);assert.ok(input,'Expected input directory [output directory budget]');console.log(JSON.stringify(run(input,out,Number(budget)),null,2));}
  catch(e){console.error('C08 audit:',e.message);process.exitCode=1;}
}
