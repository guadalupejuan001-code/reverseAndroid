import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,linkSync as link,statSync} from 'node:fs';
import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';
import {verifyProfileState,DISK_SHA,SEED_SHA} from './android_profile_state.mjs';
import {acceptsProgress} from './android_c15_result.mjs';import {analyzeGuestConsole} from './android_guest_outcome.mjs';import {sha} from './android_c13_avb.mjs';
export const INPUT_SHA='bf73c3de585cfed346ddd5dfd8756f0b99ce1ccb4f2419cb637c247afbf7ac27',REPORT_SHA='efd0c35eb8f233a14006ef4588672c0417fdf5946538a3f5016bfdf3294d8327';
const RESULT_SHA='2ef10220e5ad431b7785bc0145acf95f87c223dae0cd253626cad8946a051478',PROCESS_SHA='cc706ca287ce91c85bd84f36c8b16bcce99e6a514037583e9049c5ea8566d0a5',initial=6562538801,capacity=1024*1024*1024;
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function verifyNativePins(p){
 assert.equal(p.schema,'reverse-c26-native-pins-v1');assert.equal(p.state_sha256,INPUT_SHA,'compiled state pin');assert.equal(p.report_sha256,REPORT_SHA,'compiled report pin');assert.equal(p.disk_sha256,DISK_SHA,'compiled disk pin');assert.equal(p.counter_seed_sha256,SEED_SHA,'compiled counter seed pin');assert.equal(p.host_snapshot_transport_bytes,capacity,'compiled transport policy');return {compiled_native_pins_verified:true};
}
export function verifyInstructionProbe(p){
 assert.equal(p.opcode,'0x4e238c67');assert.equal(p.pc,'0x74680de1e0');assert.equal(p.input_checkpoint_sha256,INPUT_SHA);assert.equal(p.instructions_retired,1);
 for(const key of ['fpcr_preserved','fpsr_preserved','unrelated_registers_preserved','original_snapshot_unchanged'])assert.equal(p[key],true);
 const vector=(lo,hi)=>{for(const x of [lo,hi])assert.match(x,/^0x[0-9a-f]{1,16}$/);return BigInt(lo)|(BigInt(hi)<<64n);};
 const a=vector(p.v3_low,p.v3_high),actual=vector(p.result_low,p.result_high);let expected=0n;
 for(let lane=0n;lane<16n;lane++)if((a>>(8n*lane))&255n)expected|=255n<<(8n*lane);
 assert.equal(actual,expected,'Independent BigInt CMTST self-test result');return {authenticated_instruction_probe_verified:true,opcode:p.opcode};
}
export function preflight(root,stateOverride){
 const state=stateOverride??bounded(join(root,'current.rvg'),capacity),report=bounded(join(root,'current.json'),8*1024*1024),result=bounded(join(root,'result.json'),131072),process=bounded(join(root,'process-outcome.json'),131072);
 assert.equal(sha(state),INPUT_SHA);assert.equal(sha(report),REPORT_SHA);assert.equal(sha(result),RESULT_SHA);assert.equal(sha(process),PROCESS_SHA);
 const r=JSON.parse(report),prior=JSON.parse(result),host=JSON.parse(process),a=verifyProfileState(state,r,INPUT_SHA,{maxSnapshotBytes:capacity});
 assert.equal(a.immutable_image_sha256,'f4708c601115d0c37b1dc0c73b5efb5337fd14decc2eb5de9d593dc2ae86356d');
 assert.equal(r.guest.instructions,initial);assert.equal(r.guest.pc,'0x74680de1e0');assert.equal(r.guest.sp,'0x7ff25146c0');assert.equal(r.guest.counter_subtick,13);assert.equal(r.guest.exception_level,0);assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);assert.equal(r.guest.fpcr,0);assert.equal(r.guest.fpsr,0);
 assert.equal(r.stop_reason,'architectural_stop');assert.equal(r.error,'Unsupported AArch64 instruction 0x4e238c67 at 0x74680de1e0');assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);
 const v=r.provenance;assert.equal(v.experiment,'c25-c24-lane-transfer-continuation');assert.equal(v.input_checkpoint_sha256,'e75a8c99db489a69b0e2c9f7cc067f31535ad638ffbc14a85a924dc300d42a7a');assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.snapshot_round_trip_verified,true);assert.equal(v.source_original_init_verified_in_c21,true);assert.equal(v.source_c22_apex_bootstrap_done,true);assert.equal(v.timing_seed_sha256,SEED_SHA);assert.equal(v.host_transport_policy_serialized,false);
 assert.equal(host.status,'process_failed');assert.equal(host.exit_code,2);assert.equal(prior.checkpoint_sha256,INPUT_SHA);assert.equal(prior.execution_progress_accepted,false);
 for(const key of ['source_c21_original_init_verified','source_c21_second_stage_verified','source_c21_selinux_enforcing_verified','source_c22_apex_bootstrap_verified'])assert.equal(prior[key],true);
 for(const key of ['guest_terminal_failure','guest_reboot_requested','guest_boot_failure','guest_kernel_panic','android_framework_verified'])assert.equal(prior[key],false);
 const outcome=analyzeGuestConsole(r.console);assert.equal(outcome.guest_boot_failure,false);assert.equal(outcome.guest_reboot_requested,false);assert.equal(prior.source_c23_linkerconfig_complete,true);assert.equal(prior.authenticated_instruction_probe_verified,true);assert.equal(prior.opcode,'0x0d9f8186');
 assert.match(r.console,/Service 'system_aconfigd_platform_init'.*exited with status 0/);assert.match(r.console,/starting service 'keystore2'/);
 return {binary:a,report:r,source_c21_init_entry_verified:true,source_c22_apex_bootstrap_verified:true,source_c23_linkerconfig_complete:true,source_guest_state_unmodified:true,source_isa_stop_exactly_pinned:true};
}
export function audit(source,root){
 const input=preflight(source),probe=verifyInstructionProbe(json(join(root,'instruction-probe.json'))),pointer=json(join(root,'journal/latest.json'));
 assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),capacity),r=json(join(dir,'report.json')),v=r.provenance,digest=sha(state);assert.equal(digest,pointer.checkpoint_sha256);
 const a=verifyProfileState(state,r,digest,{maxSnapshotBytes:capacity});assert.equal(a.immutable_image_sha256,input.binary.immutable_image_sha256);assert.equal(r.initial_instructions,initial);
 assert.equal(v.experiment,'c26-c25-cmtst-continuation');assert.equal(v.input_checkpoint_sha256,INPUT_SHA);assert.equal(v.input_report_sha256,REPORT_SHA);
 for(const key of ['snapshot_round_trip_verified','source_original_init_verified_in_c21','source_c22_apex_bootstrap_done','source_c23_linkerconfig_complete','offending_instruction_verified_on_copy'])assert.equal(v[key],true);
 assert.equal(v.source_c25_st1_verified,true);assert.equal(v.source_c25_post_fs_verified,true);assert.equal(v.offending_opcode,'0x4e238c67');assert.equal(v.host_snapshot_transport_bytes,capacity);assert.equal(v.host_transport_policy_serialized,false);assert.equal(v.cycle_limit,3);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_sha256,SEED_SHA);assert.equal(v.disk_sha256,DISK_SHA);assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const outcome=analyzeGuestConsole(r.console),result={schema:'reverse-c26-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:INPUT_SHA,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,source_c21_original_init_verified:true,source_c21_second_stage_verified:true,source_c21_selinux_enforcing_verified:true,source_c22_apex_bootstrap_verified:true,source_c23_linkerconfig_complete:true,source_c24_uaddw_verified:true,source_c25_st1_verified:true,source_c25_post_fs_verified:true,...probe,host_snapshot_transport_bytes:capacity,host_transport_policy_serialized:false,execution_progress_accepted:acceptsProgress(r,outcome)&&!outcome.guest_boot_failure,...outcome,new_disk_sha256:DISK_SHA,ram_bytes:2147483648,guest_state_modified_before_resume:false,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 link(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [mode,source,output]=process.argv.slice(2);assert.ok(source&&['preflight','preflight-stdin','audit','native-pins','instruction-probe'].includes(mode));
 if(mode==='native-pins')console.log(JSON.stringify(verifyNativePins(json(source)),null,2));
 else if(mode==='instruction-probe')console.log(JSON.stringify(verifyInstructionProbe(json(source)),null,2));
 else if(mode!=='audit'){const {binary,report,...r}=preflight(source,mode==='preflight-stdin'?read(0):undefined);console.log(JSON.stringify(r,null,2));}
 else{assert.ok(output);const r=audit(source,output);console.log(JSON.stringify(r,null,2));if(!r.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
