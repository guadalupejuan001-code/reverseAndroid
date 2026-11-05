// Bind the original UMIN stop to a new, unmodified native continuation.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,statSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {verifyBinary,acceptsProgress} from './android_c15_result.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {sha,PIN} from './android_c13_avb.mjs';
export const INPUT_SHA='d2ef1f650948453e902122a7bcf6ddda577effb469c5c101a0eaec5f25394df5';
export const REPORT_SHA='c11ef7f66b4e2138e8b6d450116ebc838d9f19514e9c1f8173fb17207e5a3f7f';
const RESULT_SHA='63f35be7e51dd12e7c85ec0c19598602da424e36db5b1415eb18eb04d34671a3';
const initial=1254508639,seed='78b0aeb569a7e87dc9e970bd1a46c57dfe3364c2f49f41cf688a57f0afa1b12f';
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};
const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function checkInputHashes(state,report,result){assert.equal(sha(state),INPUT_SHA);assert.equal(sha(report),REPORT_SHA);assert.equal(sha(result),RESULT_SHA);}
export function preflight(root){
 const state=bounded(join(root,'current.rvg'),256*1024*1024),report=bounded(join(root,'current.json'),131072),result=bounded(join(root,'result.json'),131072);
 checkInputHashes(state,report,result);const r=JSON.parse(report),prior=JSON.parse(result),a=verifyBinary(state,r,INPUT_SHA);
 assert.equal(r.guest.instructions,initial);assert.equal(r.guest.pc,'0x7f8335e058');assert.equal(r.guest.exception_level,0);assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);
 assert.equal(r.stop_reason,'architectural_stop');assert.equal(r.error,'Unsupported AArch64 instruction 0x2ea16c00 at 0x7f8335e058');
 assert.equal(r.provenance.experiment,'c16-c15-polynomial-and-journal-continuation');assert.equal(r.provenance.input_checkpoint_sha256,'851ad6cfaad290f0cf130d8730bf500c3993b14dcb46b61d7ccb2b3abb600c9f');assert.equal(r.provenance.original_state_modified,false);
 assert.equal(prior.checkpoint_sha256,INPUT_SHA);assert.equal(prior.c13_system_root_switched,true);assert.equal(prior.guest_terminal_failure,false);
 for(const target of ['/system','/vendor','/product','/system_ext','/odm','/vendor_dlkm','/odm_dlkm','/system_dlkm'])assert.ok(prior.c13_mounted_targets.includes(target));
 assert.ok(/audit: type=1403[^\r\n]*lsm=selinux res=1/.test(r.console),'SELinux policy was not accepted');assert.ok(r.console.includes('/system/etc/selinux/plat_file_contexts'),'Source contexts missing');
 return {report:r,binary:a,selinux_policy_loaded_in_source:true,mounted_targets:prior.c13_mounted_targets,system_mount_evidence_source:'C13 run 41009073891 through pinned C14/C15/C16 proof chain',source_fault:'UMIN V0.2S,V0.2S,V1.2S',android_display_verified:false};
}
export function audit(source,root){
 const input=preflight(source),pointer=json(join(root,'journal/latest.json'));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
 const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),256*1024*1024),r=json(join(dir,'report.json')),v=r.provenance,digest=sha(state);
 assert.equal(digest,pointer.checkpoint_sha256);const a=verifyBinary(state,r,digest);
 assert.equal(a.immutable_image_sha256,input.binary.immutable_image_sha256);assert.equal(r.initial_instructions,initial);
 assert.equal(v.experiment,'c17-c16-minmax-continuation');assert.equal(v.input_checkpoint_sha256,INPUT_SHA);assert.equal(v.input_report_sha256,REPORT_SHA);
 assert.equal(v.fault_instruction_replayed_on_copy,true);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_sha256,seed);assert.equal(v.disk_sha256,PIN.diskSha);
 assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const outcome=analyzeGuestConsole(r.console);
 const result={schema:'reverse-c17-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:INPUT_SHA,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,total_advanced_since_c14:r.guest.instructions-910037410,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,source_fault_instruction_replayed:true,execution_progress_accepted:acceptsProgress(r,outcome),...outcome,selinux_policy_loaded_in_source:input.selinux_policy_loaded_in_source,c13_mounted_targets:input.mounted_targets,c13_system_root_switched:true,system_mount_evidence_source:input.system_mount_evidence_source,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 copy(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [mode,source,output]=process.argv.slice(2);assert.ok(source&&['preflight','audit'].includes(mode));
 if(mode==='preflight'){const {report,binary,...result}=preflight(source);console.log(JSON.stringify(result,null,2));}
 else{assert.ok(output);const result=audit(source,output);console.log(JSON.stringify(result,null,2));if(!result.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
