import assert from 'node:assert/strict';import {readFileSync as read,writeFileSync as write,statSync,linkSync,copyFileSync as copy,existsSync,mkdtempSync,renameSync,rmSync}from'node:fs';import{join,resolve,dirname,basename}from'node:path';import{pathToFileURL}from'node:url';
import{verifyC27State,verifyC27Pins,CAPACITY}from'./android_c27_state.mjs';import{sha}from'./android_c13_avb.mjs';import{analyzeGuestConsole}from'./android_guest_outcome.mjs';import{acceptsProgress}from'./android_c15_result.mjs';
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function c27Result(state,r,digest){
 const binary=verifyC27State(state,r,digest),outcome=analyzeGuestConsole(r.console);
 return{schema:'reverse-c27-fresh-result-v1',checkpoint_sha256:digest,immutable_image_sha256:binary.immutable_image_sha256,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,fresh_machine:true,old_cpu_or_init_proof_reused:false,source_initrd_unchanged:true,first_user_instruction_verified:r.first_user_instruction_verified,second_stage_init_observed:r.console.includes('init: init second stage started!'),selinux_enforcing_observed:/audit: type=1404[^\r\n]*enforcing=1/.test(r.console),execution_progress_accepted:acceptsProgress(r,outcome)&&!outcome.guest_boot_failure,...outcome,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
}
export function pinC27(journal,output,sourceCommit){
 assert.match(sourceCommit,/^[0-9a-f]{40}$/);assert.ok(!existsSync(output));let temporary;
 try{
  const p=json(join(journal,'latest.json'));assert.match(p.generation,/^generation-[1-9][0-9]*$/);assert.match(p.checkpoint_sha256,/^[0-9a-f]{64}$/);const dir=join(journal,p.generation),report=bounded(join(dir,'report.json'),8*1024*1024),r=JSON.parse(report);
  if(!r.guest.instructions)return{status:'not_ready'};
  temporary=mkdtempSync(join(dirname(resolve(output)),'.'+basename(output)+'-pending-'));linkSync(join(dir,'checkpoint.rvg'),join(temporary,'current.rvg'));write(join(temporary,'current.json'),report);
  const state=bounded(join(temporary,'current.rvg'),CAPACITY),digest=sha(state);assert.equal(digest,p.checkpoint_sha256);const result=c27Result(state,r,digest);
  const manifest={...result,source_commit:sourceCommit,report_sha256:sha(report),observation_only:true,original_snapshot_modified:false,journal_generation:p.generation};write(join(temporary,'observation.json'),JSON.stringify(manifest,null,2)+'\n');renameSync(temporary,output);temporary=undefined;return{status:'pinned',...manifest};
 }catch(e){if(e.code==='ENOENT')return{status:'not_ready'};throw e;}finally{if(temporary)rmSync(temporary,{recursive:true,force:true});}
}
export function auditC27(root){
 const p=json(join(root,'journal/latest.json'));assert.match(p.generation,/^generation-[1-9][0-9]*$/);const dir=join(root,'journal',p.generation),state=bounded(join(dir,'checkpoint.rvg'),CAPACITY),r=json(join(dir,'report.json')),digest=sha(state);assert.equal(digest,p.checkpoint_sha256);const result=c27Result(state,r,digest);
 linkSync(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{const[mode,root,output,commit]=process.argv.slice(2);let r;if(mode==='pins')r=verifyC27Pins(json(root));else if(mode==='pin')r=pinC27(root,output,commit);else{assert.equal(mode,'audit');r=auditC27(root);}console.log(JSON.stringify(r,null,2));if(mode==='audit'&&!r.execution_progress_accepted)process.exitCode=2;}catch(e){console.error(e.stack);process.exitCode=1;}}
