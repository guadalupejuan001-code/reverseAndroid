// Read-only C10 -> C11 evidence; reuses the unmodified R2 full-state/media binder.
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,statSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
import {bindC08Snapshot} from './android_c08_result.mjs';
import {C07_PIN,hash} from './android_verified_c07_source.mjs';
export const PIN=Object.freeze({state:'dc1683ae2cf0f469852785f83d12d18e99cb9631a3b1d4ba4600c60993353307',report:'3f7e28bb92a652ac6a791566cdb2c120ddc83dbd2469c9b6126df6758d57188d',parent09:'2f50ba79ef1bcfd0838a3bbd344b92d749576671cd6f76e357159c91c73a6d0c',parent08:'3dc63288cb53ad4485e4b2511af2fb22a13825fc81d0aace3efe25e71becbd38',parent07:'5d426d1fec117b8ef955bbbccb0e944586f36c6fe8594d9c684575dd0067e574',entry:'ec12937b7c33a3aff439f3c388bbd32b1f538e08667d0f83e61736489666bdbf',instructions:6456253347,pc:'0x4ce568',subtick:3,opcode:'0x4c9f0192'});
const boundedRead=(path,max)=>{assert.ok(statSync(path).size<=max,'Oversized evidence');return read(path);};
export function verifySource(input){
  const report=boundedRead(resolve(input,'current.json'),65536);assert.equal(hash(report),PIN.report,'C10 report mismatch');
  const state=boundedRead(resolve(input,'current.rvg'),256*1024*1024);assert.equal(hash(state),PIN.state,'C10 state mismatch');
  const r=JSON.parse(report);bindC08Snapshot(state,r,{checkpoint_sha256:PIN.state});
  assert.equal(r.guest.instructions,PIN.instructions);assert.equal(r.guest.pc,PIN.pc);assert.equal(r.guest.exception_level,0);
  assert.equal(r.guest.counter_subtick,PIN.subtick);assert.equal(r.stop_reason,'architectural_stop');
  assert.equal(r.error,'Unsupported AArch64 instruction 0x4c9f0192 at 0x4ce568');
  const parents={};
  for(const [name,pin] of [['parent-c09.json',PIN.parent09],['parent-c08.json',PIN.parent08],['parent-c07.json',PIN.parent07],['entry-proof.json',PIN.entry]]){
    const b=boundedRead(resolve(input,name),65536);assert.equal(hash(b),pin,'Ancestor mismatch: '+name);parents[name]=JSON.parse(b);
  }
  assert.equal(r.provenance.input_report_sha256,PIN.parent09);
  assert.equal(parents['parent-c09.json'].provenance.input_report_sha256,PIN.parent08);
  assert.equal(parents['parent-c08.json'].provenance.input_report_sha256,PIN.parent07);
  assert.equal(parents['parent-c07.json'].verified_entry_checkpoint_sha256,C07_PIN.original_entry_checkpoint_sha256);
  return {report:r,parents};
}
export function verifyResult(source,state,r,pointer,budget){
  assert.ok(Number.isSafeInteger(budget)&&budget>0&&budget<=1000000000,'Invalid execution budget');
  bindC08Snapshot(state,r,pointer); // Internal FNV, original media, RAM, EL, clock and exact state/report binding.
  const v=r.provenance,g=r.guest;
  assert.equal(r.initial_instructions,PIN.instructions);
  assert.ok(Number.isSafeInteger(r.advanced_instructions)&&r.advanced_instructions>=0&&r.advanced_instructions<=budget);
  assert.equal(g.instructions,PIN.instructions+r.advanced_instructions);
  assert.equal(v.continuation_kind,'pinned-c10-descendant');assert.equal(v.input_checkpoint_sha256,PIN.state);
  assert.equal(v.input_report_sha256,PIN.report);assert.equal(v.ancestor_entry_report_sha256,PIN.entry);
  assert.equal(v.ancestor_entry_checkpoint_sha256,C07_PIN.original_entry_checkpoint_sha256);assert.equal(v.ancestor_init_verified,true);
  assert.equal(r.prior_user_instruction_verified,true);assert.equal(r.first_user_instruction_verified,false);
  assert.equal(r.verified_entry_checkpoint_sha256,C07_PIN.original_entry_checkpoint_sha256);
  assert.equal(v.disk_sha256,C07_PIN.disk_sha256);assert.equal(v.disk_bytes,C07_PIN.disk_bytes);
  assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_origin,'checkpoint');
  assert.equal(v.timing_seed_sha256,C07_PIN.counter_seed_sha256);assert.equal(v.counter_instructions_per_tick,64);
  assert.equal(g.counter_instructions_per_tick,64);assert.equal(g.system_mode,true);assert.equal(g.pci_bus,true);
  assert.ok([0,1].includes(g.exception_level));assert.ok(g.counter_samples>=source.report.guest.counter_samples);
  assert.equal(g.counter_subtick,(PIN.subtick+r.advanced_instructions)%64,'Counter phase is not continuous');
  for(const name of ['android_services_verified','android_display_verified','native_android_games_verified'])assert.equal(v[name],false,'Unproved capability: '+name);
  assert.ok(typeof r.console==='string'&&Buffer.byteLength(r.console)<=4*1024*1024);
  assert.equal(typeof r.error,'string');assert.equal(typeof r.stop_reason,'string');
  const previous=source.parents['parent-c07.json'].console+source.parents['parent-c08.json'].console+source.parents['parent-c09.json'].console+source.report.console;
  const combined=previous+r.console,panic=/Kernel panic - not syncing|Attempted to kill init!/.test(combined);
  const clean=['instruction_budget','host_wall_budget'].includes(r.stop_reason)&&r.advanced_instructions>0&&!r.error&&!panic&&!g.halted&&!g.waiting;
  return {source_c10_verified:true,r2_integrity_verified:true,checkpoint_sha256:r.checkpoint_sha256,instructions:g.instructions,advanced_instructions:r.advanced_instructions,pc:g.pc,stop_reason:r.stop_reason,error:r.error,execution_progress_accepted:clean,guest_kernel_panic:panic,new_uart_bytes:Buffer.byteLength(r.console),android_framework_verified:false,android_display_verified:false,native_android_games_verified:false,combined};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const [input,out,budgetText]=process.argv.slice(2);assert.ok(input,'Expected input-directory [output-directory budget]');
    const source=verifySource(input);
    if(!out)console.log(JSON.stringify({source_c10_verified:true,checkpoint_sha256:PIN.state,internal_integrity_verified:true}));
    else{
      const pointer=JSON.parse(boundedRead(resolve(out,'journal/latest.json'),65536));
      assert.match(pointer.generation,/^generation-[1-9][0-9]*$/,'Unsafe journal generation');
      const dir=resolve(out,'journal',pointer.generation),state=boundedRead(resolve(dir,'checkpoint.rvg'),256*1024*1024),r=JSON.parse(boundedRead(resolve(dir,'report.json'),8*1024*1024));
      const result=verifyResult(source,state,r,pointer,Number(budgetText));
      // Promote authentic stopped states too, but never before all integrity checks.
      copy(resolve(dir,'checkpoint.rvg'),resolve(out,'current.rvg'));copy(resolve(dir,'report.json'),resolve(out,'current.json'));
      copy(resolve(input,'current.json'),resolve(out,'parent-c10.json'));
      for(const name of ['parent-c09.json','parent-c08.json','parent-c07.json','entry-proof.json'])copy(resolve(input,name),resolve(out,name));
      write(resolve(out,'console-combined.txt'),result.combined);delete result.combined;
      write(resolve(out,'console-new.txt'),r.console);
      write(resolve(out,'milestone.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result,null,2));
      if(!result.execution_progress_accepted)process.exitCode=2;
    }
  }catch(error){console.error(error.message);process.exitCode=1;}
}
