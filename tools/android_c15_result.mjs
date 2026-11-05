// Bind authentic C14 input to actual C15 output. This auditor never executes the guest.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,statSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {sha,PIN} from './android_c13_avb.mjs';
export const INPUT_SHA='33a8436f87b1426f6fb8af0e979627694957d0ce01e6c8972b8f79580ff2d28b';
export const REPORT_SHA='cba168210b8b8dfd31e2a887ea80607345e21f6d87b33121bc94b3199a02310f';
const RESULT_SHA='85ce5f3afec336cf4ca1f3841a30fef9f5ffe711a08e6ca0e731af93d338612e';
const kernel='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8';
const ramdisk='c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da';
const seed='78b0aeb569a7e87dc9e970bd1a46c57dfe3364c2f49f41cf688a57f0afa1b12f';
const initial=910037410;
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};
const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function checkInputHashes(state,report,result){assert.equal(sha(state),INPUT_SHA);assert.equal(sha(report),REPORT_SHA);assert.equal(sha(result),RESULT_SHA);}
export function verifyBinary(state,r,digest){
 const a=auditGkiRvg(state,kernel,digest),g=r.guest;
 assert.equal(a.logical_ram_bytes,134217728);assert.equal(a.rvi_version,'RVI3');
 assert.equal(a.external_disk_sha256,PIN.diskSha);assert.equal(a.external_disk_bytes,PIN.diskBytes);
 assert.equal(a.instructions,g.instructions);assert.equal(a.native_pc,g.pc);assert.equal(a.native_sp,g.sp);
 assert.equal(a.native_halted,g.halted?1:0);assert.equal(a.native_exit_code,g.exit_code);
 assert.equal(g.system_mode,true);assert.equal(g.logical_ram_bytes,a.logical_ram_bytes);assert.equal(g.block_image_bytes,PIN.diskBytes);assert.equal(g.pci_bus,true);
 assert.equal(digest,r.checkpoint_sha256);assert.equal(r.advanced_instructions,g.instructions-r.initial_instructions);
 assert.equal(typeof r.console,'string');assert.ok(Buffer.byteLength(r.console)<=4*1024*1024);
 const field=i=>Number(state.readBigUInt64LE(8+i*8)),head=8+(state[3]===51?44:43)*8;
 const kn=Number(state.readBigUInt64LE(head+8)),dt=Number(state.readBigUInt64LE(head+16)),rd=Number(state.readBigUInt64LE(head+24));
 assert.equal(sha(state.subarray(head+48+kn+dt,head+48+kn+dt+rd)),ramdisk);
 const machine=head+field(39)+field(40)+field(41),clock=machine+field(42)-64;
 assert.equal(state.readBigUInt64LE(machine),7n);assert.equal(state.readBigUInt64LE(machine+8),1n);
 assert.equal(Number(state.readBigUInt64LE(machine+16)),g.waiting?1:0);assert.equal(Number(state.readBigUInt64LE(machine+32)),g.exception_level);
 assert.equal(state.readBigUInt64LE(clock),2n);assert.equal(sha(state.subarray(clock+8,clock+40)),seed);
 for(const [offset,key] of [[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])assert.equal(Number(state.readBigUInt64LE(clock+offset)),g[key]);
 assert.equal(g.counter_instructions_per_tick,64);assert.ok(Number.isInteger(g.counter_subtick)&&g.counter_subtick>=0&&g.counter_subtick<64);
 return a;
}
export function preflight(root){
 const state=bounded(join(root,'current.rvg'),256*1024*1024),report=bounded(join(root,'current.json'),131072),result=bounded(join(root,'result.json'),131072);
 checkInputHashes(state,report,result);const r=JSON.parse(report),prior=JSON.parse(result),a=verifyBinary(state,r,INPUT_SHA);
 assert.equal(r.guest.instructions,initial);assert.equal(r.guest.pc,'0x7f855d33d0');assert.equal(r.guest.exception_level,0);assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);
 assert.equal(r.stop_reason,'architectural_stop');assert.equal(r.error,'Unsupported AArch64 instruction 0x2e65c022 at 0x7f855d33d0');
 assert.equal(r.provenance.input_checkpoint_sha256,'2a67dd8683dc179fb98effee1c1ff7d10b251e9626d9f6a37698fabe3d335caf');assert.equal(r.provenance.original_state_modified,false);
 assert.equal(prior.checkpoint_sha256,INPUT_SHA);assert.equal(prior.c13_system_root_switched,true);assert.equal(prior.guest_terminal_failure,false);
 for(const target of ['/system','/vendor','/product','/system_ext','/odm','/vendor_dlkm','/odm_dlkm','/system_dlkm'])assert.ok(prior.c13_mounted_targets.includes(target));
 return {report:r,binary:a,mounted_targets:prior.c13_mounted_targets,system_mount_evidence_source:'C13 run 41009073891 via pinned C14 artifact 12957658228',source_fault:'UMULL V2.4S,V1.4H,V5.4H',android_display_verified:false};
}
export function acceptsProgress(r,outcome){return ['host_wall_budget','instruction_budget'].includes(r.stop_reason)&&!r.error&&!outcome.guest_terminal_failure&&!outcome.guest_reboot_requested&&!r.guest.halted&&!r.guest.waiting&&r.advanced_instructions>0;}
export function audit(source,root){
 const input=preflight(source),pointer=json(join(root,'journal/latest.json'));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
 const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),256*1024*1024),r=json(join(dir,'report.json')),v=r.provenance,digest=sha(state);
 assert.equal(digest,pointer.checkpoint_sha256);const a=verifyBinary(state,r,digest);
 assert.equal(a.immutable_image_sha256,input.binary.immutable_image_sha256);assert.equal(r.initial_instructions,initial);
 assert.equal(v.experiment,'c15-c14-multiply-long-continuation');assert.equal(v.input_checkpoint_sha256,INPUT_SHA);assert.equal(v.input_report_sha256,REPORT_SHA);
 assert.equal(v.fault_instruction_replayed_on_copy,true);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_sha256,seed);assert.equal(v.disk_sha256,PIN.diskSha);
 assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const outcome=analyzeGuestConsole(r.console);
 const result={schema:'reverse-c15-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:INPUT_SHA,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,source_fault_instruction_replayed:true,execution_progress_accepted:acceptsProgress(r,outcome),...outcome,c13_mounted_targets:input.mounted_targets,c13_system_root_switched:true,system_mount_evidence_source:input.system_mount_evidence_source,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 copy(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [mode,source,output]=process.argv.slice(2);assert.ok(source&&['preflight','audit'].includes(mode));
 if(mode==='preflight'){const {report,binary,...result}=preflight(source);console.log(JSON.stringify(result,null,2));}
 else{assert.ok(output);const result=audit(source,output);console.log(JSON.stringify(result,null,2));if(!result.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
