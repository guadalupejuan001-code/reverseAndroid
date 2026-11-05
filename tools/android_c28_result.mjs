import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,linkSync as link,statSync} from 'node:fs';
import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';
import {verifyC28State,DISK_SHA,SEED_SHA} from './android_c28_state.mjs';
import {verifyC27State} from './android_c27_state.mjs';
import {acceptsProgress} from './android_c15_result.mjs';import {analyzeGuestConsole} from './android_guest_outcome.mjs';import {sha} from './android_c13_avb.mjs';
export const INPUT_SHA='9db40b80214344583f855ccc777eb74963ece41ca5a1afab8de0737507f0e939',REPORT_SHA='7ff064cd92bb1fffa7de798e971abc456dd8032b7cebc0bb7c6cc44aee34e903';
export const BOUNDARY=Object.freeze({instructions:2833048839,pc:'0x5568b464ec',phase:23});
const RESULT_SHA='dc1f9d47ed847c5f298ec93e8bc70c963740720408b3886b2b36996e471c7e61',PROCESS_SHA='4670e6d2de854298419f66e5658b241a9ea5e3ccc2ab8eadce4e9c4055c949c9',initial=2833048839,capacity=1024*1024*1024;
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function verifyNativePins(p){
 assert.equal(p.schema,'reverse-c28-native-pins-v1');assert.equal(p.state_sha256,INPUT_SHA,'compiled state pin');assert.equal(p.report_sha256,REPORT_SHA,'compiled report pin');assert.equal(p.disk_sha256,DISK_SHA,'compiled disk pin');assert.equal(p.counter_seed_sha256,SEED_SHA,'compiled counter seed pin');assert.equal(p.host_snapshot_transport_bytes,capacity,'compiled transport policy');return {compiled_native_pins_verified:true};
}
export function verifyCleanSourceBoundary(r,prior,host){
 assert.equal(r.guest.instructions,initial);assert.equal(r.guest.pc,'0x5568b464ec');assert.equal(r.guest.counter_subtick,23);assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);
 assert.ok(['host_wall_budget','instruction_budget'].includes(r.stop_reason));assert.equal(r.error,'');assert.equal(r.first_user_instruction_verified,true);assert.equal(r.prior_user_instruction_verified,false);
 assert.equal(host.status,'process_completed');assert.equal(host.exit_code,0);assert.equal(prior.schema,'reverse-c27-fresh-result-v1');assert.equal(prior.checkpoint_sha256,INPUT_SHA);assert.equal(prior.instructions,initial);assert.equal(prior.stop_reason,r.stop_reason);assert.equal(prior.error,r.error);assert.equal(prior.execution_progress_accepted,true);
 for(const key of ['first_user_instruction_verified','second_stage_init_observed','selinux_enforcing_observed','fresh_machine','source_initrd_unchanged'])assert.equal(prior[key],true);
 for(const key of ['old_cpu_or_init_proof_reused','guest_kernel_panic','guest_boot_failure','guest_terminal_failure','guest_reboot_requested','android_framework_verified','android_display_verified'])assert.equal(prior[key],false);
 const outcome=analyzeGuestConsole(r.console);assert.equal(outcome.guest_boot_failure,false);assert.equal(outcome.guest_reboot_requested,false);
 assert.match(r.console,/init: init second stage started!/);assert.match(r.console,/audit: type=1404[^\r\n]*enforcing=1/);
 return {clean_source_boundary_verified:true};
}
export function preflight(root,stateOverride){
 const state=stateOverride??bounded(join(root,'current.rvg'),capacity),report=bounded(join(root,'current.json'),8*1024*1024),result=bounded(join(root,'result.json'),131072),process=bounded(join(root,'process-outcome.json'),131072);
 assert.equal(bounded(join(root,'source-commit.txt'),128).toString('utf8').trim(),'6a944cc2f9b244b224a409ccc1938ad603524e2e','Wrong exact C27 source commit');
 assert.equal(sha(state),INPUT_SHA);assert.equal(sha(report),REPORT_SHA);assert.equal(sha(result),RESULT_SHA);assert.equal(sha(process),PROCESS_SHA);
 const r=JSON.parse(report),prior=JSON.parse(result),host=JSON.parse(process),a=verifyC27State(state,r,INPUT_SHA);verifyC28State(state,r,INPUT_SHA,{maxSnapshotBytes:capacity});
 verifyCleanSourceBoundary(r,prior,host);
 return {binary:a,report:r,source_c27_init_entry_verified:true,source_c27_second_stage_verified:true,source_c27_selinux_enforcing_verified:true,clean_source_boundary_verified:true};
}
export function audit(source,root){
 const input=preflight(source),pointer=json(join(root,'journal/latest.json'));
 assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),capacity),r=json(join(dir,'report.json')),v=r.provenance,digest=sha(state);assert.equal(digest,pointer.checkpoint_sha256);
 const a=verifyC28State(state,r,digest,{maxSnapshotBytes:capacity});assert.equal(a.immutable_image_sha256,input.binary.immutable_image_sha256);assert.equal(r.initial_instructions,initial);
 assert.equal(v.experiment,'c28-c27-clean-continuation');assert.equal(v.input_checkpoint_sha256,INPUT_SHA);assert.equal(v.input_report_sha256,REPORT_SHA);
 for(const key of ['snapshot_round_trip_verified','source_c27_original_init_verified','source_c27_second_stage_verified','source_c27_selinux_enforcing_verified'])assert.equal(v[key],true);
 assert.equal(v.host_snapshot_transport_bytes,capacity);assert.equal(v.host_transport_policy_serialized,false);assert.equal(v.cycle_limit,3);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_sha256,SEED_SHA);assert.equal(v.disk_sha256,DISK_SHA);assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const outcome=analyzeGuestConsole(r.console),result={schema:'reverse-c28-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:INPUT_SHA,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,source_c27_original_init_verified:true,source_c27_second_stage_verified:true,source_c27_selinux_enforcing_verified:true,clean_source_boundary_verified:true,host_snapshot_transport_bytes:capacity,host_transport_policy_serialized:false,execution_progress_accepted:acceptsProgress(r,outcome)&&!outcome.guest_boot_failure,...outcome,new_disk_sha256:DISK_SHA,ram_bytes:2147483648,guest_state_modified_before_resume:false,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 link(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [mode,source,output]=process.argv.slice(2);assert.ok(source&&['preflight','preflight-stdin','audit','native-pins'].includes(mode));
 if(mode==='native-pins')console.log(JSON.stringify(verifyNativePins(json(source)),null,2));
 else if(mode!=='audit'){const {binary,report,...r}=preflight(source,mode==='preflight-stdin'?read(0):undefined);console.log(JSON.stringify(r,null,2));}
 else{assert.ok(output);const r=audit(source,output);console.log(JSON.stringify(r,null,2));if(!r.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
