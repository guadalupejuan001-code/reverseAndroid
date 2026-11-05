// Read-only binding to exact C13 input and C14 output, not an Android UI claim.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,statSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {sha,PIN,deriveHandoff,readDirectory} from './android_c13_avb.mjs';
import {verifyC13} from './android_c13_result.mjs';
export const INPUT_SHA='2a67dd8683dc179fb98effee1c1ff7d10b251e9626d9f6a37698fabe3d335caf';
export const REPORT_SHA='acbddc19e78c97f03817cd2f031435b8624be70ed81ed280ca810f229f4e5d37';
const kernel='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8';
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};
const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function checkInputHashes(state,report){assert.equal(sha(state),INPUT_SHA);assert.equal(sha(report),REPORT_SHA);}
export async function preflight(root){
 const state=bounded(join(root,'current.rvg'),256*1024*1024),report=bounded(join(root,'current.json'),131072);checkInputHashes(state,report);
 const r=JSON.parse(report),handoff=deriveHandoff(await readDirectory(join(root,'metadata')));
 const a=verifyC13(state,r,{checkpoint_sha256:INPUT_SHA},handoff);
 assert.equal(a.stop_reason,'architectural_stop');assert.equal(a.error,'Unsupported AArch64 instruction 0x2e30a862 at 0x7f855d33a0');
 assert.equal(a.instructions,910037398);assert.equal(a.first_original_init_instruction_verified,true);assert.equal(a.guest_terminal_failure,false);
 assert.equal(r.guest.exception_level,0);assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);
 const mounts=[...r.console.matchAll(/__mount\(source=[^,\r\n]+,target=(\/[^,\r\n]+),type=[^)\r\n]+\)=0: Success/g)].map(x=>x[1]);
 for(const target of ['/system','/vendor','/product','/system_ext','/odm','/vendor_dlkm','/odm_dlkm','/system_dlkm'])assert.ok(mounts.includes(target),'C13 mount missing: '+target);
 assert.ok(r.console.includes("Switching root to '/system'"));
 return {report:r,checkpoint_sha256:INPUT_SHA,mounted_targets:[...new Set(mounts)],system_root_switched:true,source_fault:'UMAXV B2,V3.8B',android_ui_verified:false};
}
export async function audit(source,root){
 const input=await preflight(source),pointer=json(join(root,'journal/latest.json'));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
 const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),256*1024*1024),r=json(join(dir,'report.json')),g=r.guest,v=r.provenance;
 const digest=sha(state);assert.equal(digest,pointer.checkpoint_sha256);assert.equal(digest,r.checkpoint_sha256);
 const a=auditGkiRvg(state,kernel,digest);assert.equal(a.bootargs,input.report.provenance.bootargs);assert.equal(a.logical_ram_bytes,134217728);
 assert.equal(a.external_disk_sha256,PIN.diskSha);assert.equal(a.external_disk_bytes,PIN.diskBytes);assert.equal(a.rvi_version,'RVI3');
 assert.equal(a.instructions,g.instructions);assert.equal(a.native_pc,g.pc);assert.equal(a.native_halted,g.halted?1:0);assert.equal(g.system_mode,true);assert.equal(g.logical_ram_bytes,a.logical_ram_bytes);assert.equal(g.block_image_bytes,PIN.diskBytes);assert.equal(g.pci_bus,true);assert.equal(r.initial_instructions,910037398);assert.equal(r.advanced_instructions,g.instructions-910037398);
 assert.equal(v.experiment,'c14-c13-minmax-continuation');assert.equal(v.input_checkpoint_sha256,INPUT_SHA);assert.equal(v.input_report_sha256,REPORT_SHA);
 assert.equal(v.fault_instruction_replayed_on_copy,true);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);
 assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const field=i=>Number(state.readBigUInt64LE(8+i*8)),head=8+(state[3]===51?44:43)*8;
 const kn=Number(state.readBigUInt64LE(head+8)),dt=Number(state.readBigUInt64LE(head+16)),rd=Number(state.readBigUInt64LE(head+24));
 assert.equal(sha(state.subarray(head+48+kn+dt,head+48+kn+dt+rd)),input.report.provenance.ramdisk_sha256);
 const machine=head+field(39)+field(40)+field(41),clock=machine+field(42)-64;
 assert.equal(state.readBigUInt64LE(machine),7n);assert.equal(state.readBigUInt64LE(machine+8),1n);assert.equal(Number(state.readBigUInt64LE(machine+16)),g.waiting?1:0);assert.equal(Number(state.readBigUInt64LE(machine+32)),g.exception_level);
 assert.equal(state.readBigUInt64LE(clock),2n);assert.equal(sha(state.subarray(clock+8,clock+40)),input.report.provenance.timing_seed_sha256);
 assert.equal(v.timing_seed_sha256,input.report.provenance.timing_seed_sha256);
 for(const [offset,key] of [[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])assert.equal(Number(state.readBigUInt64LE(clock+offset)),g[key]);
 assert.equal(g.counter_instructions_per_tick,64);assert.ok(Number.isInteger(g.counter_subtick)&&g.counter_subtick>=0&&g.counter_subtick<64); // Persisted phase may reset on WFI.
 const outcome=analyzeGuestConsole(r.console);const clean=['host_wall_budget','instruction_budget'].includes(r.stop_reason)&&!r.error&&!outcome.guest_terminal_failure&&!outcome.guest_reboot_requested&&!g.halted&&!g.waiting&&r.advanced_instructions>0;
 const result={schema:'reverse-c14-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:INPUT_SHA,instructions:g.instructions,advanced_instructions:r.advanced_instructions,pc:g.pc,stop_reason:r.stop_reason,error:r.error,source_fault_instruction_replayed:true,execution_progress_accepted:clean,...outcome,c13_mounted_targets:input.mounted_targets,c13_system_root_switched:true,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 copy(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [mode,source,output]=process.argv.slice(2);assert.ok(source&&['preflight','audit'].includes(mode));
 if(mode==='preflight'){const {report,...result}=await preflight(source);console.log(JSON.stringify(result,null,2));}
 else{assert.ok(output);const result=await audit(source,output);console.log(JSON.stringify(result,null,2));if(!result.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
