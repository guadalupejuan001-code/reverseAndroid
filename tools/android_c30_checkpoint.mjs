// C30: source-authenticated, read-only diagnosis of the completed C29 guest.
// This does not execute instructions, interpret a green CI run as a boot, or mutate a checkpoint.
import assert from 'node:assert/strict';
import {readFileSync, statSync, existsSync} from 'node:fs';
import {join, resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {preflight} from './android_c27_profile_resume.mjs';
import {evaluateFrameworkEvidence} from './android_framework_evidence.mjs';

export const C29_FINAL = Object.freeze({
 source_commit: '16d670048a2072862dbdb732c21444c9dc35575d',
 checkpoint_sha256: '454e29a90cc1af34df95b2c60f1fc90d29927096c72be962e95b2e9fca16598d',
 instructions: 13615798839,
 c28_instructions: 4896648839
});
export const C29_ROUND_COUNTS = Object.freeze([
 7294248839, 9654948839, 11888198839, C29_FINAL.instructions
]);

function safeTaskName(kernel) {
 const d = kernel?.current_task_details;
 if (!d || !['process_list','process_thread_list'].includes(d.verification) ||
     !Number.isSafeInteger(d.pid) || d.pid <= 0 ||
     typeof d.name !== 'string' || d.name.length > 16 ||
     kernel.current_pid !== d.pid || !kernel.current_task ||
     kernel.current_task !== d.task) return null;
 return {pid:d.pid, name:d.name, source:d.verification};
}

export function diagnoseCompletedC29({contract, report, result, host, boundary, kernel=null, frame=null, ppm, authenticated=false}) {
 assert.equal(authenticated,true,'Full four-file C27 source preflight required');
 assert.equal(contract.source_commit,C29_FINAL.source_commit,'Unexpected producer commit');
 assert.equal(contract.checkpoint_sha256,C29_FINAL.checkpoint_sha256,'Unexpected final C29 checkpoint');
 assert.equal(report.checkpoint_sha256,C29_FINAL.checkpoint_sha256);
 assert.equal(report.guest.instructions,C29_FINAL.instructions);
 assert.equal(result.checkpoint_sha256,C29_FINAL.checkpoint_sha256);
 assert.equal(result.instructions,C29_FINAL.instructions);
 assert.equal(boundary.schema,'reverse-c27-sequence-boundary-v1');
 assert.equal(boundary.checkpoint_sha256,C29_FINAL.checkpoint_sha256);
 assert.equal(boundary.instructions,C29_FINAL.instructions);
 assert.equal(boundary.clean_completed,true);
 assert.equal(host.status,'process_completed');
 assert.equal(host.exit_code,0);
 assert.equal(result.execution_progress_accepted,true);
 assert.ok(C29_ROUND_COUNTS.every((n,i)=>i===0||n>C29_ROUND_COUNTS[i-1]));
 assert.equal(C29_ROUND_COUNTS.at(-1),C29_FINAL.instructions);

 // Runtime evidence is recomputed from bound observer outputs. A missing
 // observer is a diagnostic gap, never an implicit successful boot.
 assert.equal(Boolean(kernel),Boolean(frame),'Partial read-only observer evidence');
 let observation=null;
 if(kernel)observation=evaluateFrameworkEvidence({
  checkpointSha:C29_FINAL.checkpoint_sha256,report,result,host,kernel,frame,ppm
 });
 const current=observation?safeTaskName(kernel):null;
 const processes=observation?kernel.tasks.filter(t=>t&&Number.isSafeInteger(t.pid)&&t.pid>0)
   .map(t=>({pid:t.pid,name:t.name,state:t.state,exit_state:t.exit_state??null,has_user_mm:t.has_user_mm??null})):[];
 const has=name=>processes.some(p=>p.name===name&&p.exit_state===0&&p.has_user_mm===true);
 const stage=observation?.framework_runtime_verified?'framework_live_verified':
   current?.name==='mke2fs'?'userdata_formatter_executing':
   has('mke2fs')?'userdata_formatter_present':
   has('keystore2')?'userspace_services_observed':
   observation?'stage_not_established':'observation_unavailable';
 return {
  schema:'reverse-android-c30-diagnosis-v1',
  original_source_authenticated:true,
  source_commit:C29_FINAL.source_commit,
  checkpoint_sha256:C29_FINAL.checkpoint_sha256,
  instructions:C29_FINAL.instructions,
  cpu_instructions_since_c28:C29_FINAL.instructions-C29_FINAL.c28_instructions,
  c29_clean_round_instruction_counts:[...C29_ROUND_COUNTS],
  cpu_execution_confirmed:true,
  filesystem_formatting_completion_verified:false,
  stage,
  current_task:current,
  guest_kernel_pc_symbol:observation?kernel.pc_symbol??null:null,
  process_count:processes.length,
  observed_key_processes:processes.filter(t=>['init','mke2fs','keystore2','zygote64','system_server','surfaceflinger'].includes(t.name)),
  read_only_observation_available:Boolean(observation),
  live_framework_verified:observation?.framework_runtime_verified??false,
  live_boot_completed_verified:observation?.sys_boot_completed_live??false,
  scanout_exported:observation?.scanout_exported??false,
  ready_for_visual_review:observation?.ready_for_visual_review??false,
  android_display_content_verified:false,
  framework_readiness_reasons:observation?.reasons??['read_only_observation_unavailable'],
  instruction_progress_does_not_prove_disk_progress:true,
  next_diagnostic:'Compare real live task/console and userdata state across authenticated completed checkpoints before extending long budgets.',
  source_checkpoint_unchanged:true
 };
}

function boundedJson(path,maxBytes=8*1024*1024){
 assert.ok(statSync(path).size<=maxBytes,'Evidence JSON exceeds bound');
 return JSON.parse(readFileSync(path,'utf8'));
}
export function diagnoseSourceFolder(folder) {
 const contract=boundedJson(join(folder,'output-contract.json'),131072);
 // This independently verifies all four hashes, authentic CPU, RAM, disk,
 // timer, original /init proof and clean-budget result before ANY stage claim.
 preflight(contract,folder);
 const load=name=>boundedJson(join(folder,name));
 const kernelPath=join(folder,'kernel-observation.json');
 const framePath=join(folder,'checkpoint-frame.json');
 assert.equal(existsSync(kernelPath),existsSync(framePath),'Observer pair incomplete');
 const kernel=existsSync(kernelPath)?load('kernel-observation.json'):null;
 const frame=existsSync(framePath)?load('checkpoint-frame.json'):null;
 const ppm=frame?.status==='scanout_exported'?readFileSync(join(folder,'checkpoint-frame.ppm')):undefined;
 return diagnoseCompletedC29({
  contract,report:load('current.json'),result:load('result.json'),
  host:load('process-outcome.json'),boundary:load('boundary-summary.json'),
  kernel,frame,ppm,authenticated:true
 });
}
if(process.argv[1] && import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 try{
  assert.equal(process.argv.length,3,'usage: node tools/android_c30_checkpoint.mjs <completed-C29-artifact-directory>');
  process.stdout.write(JSON.stringify(diagnoseSourceFolder(resolve(process.argv[2])),null,2)+'\n');
 }catch(error){console.error(error.stack);process.exitCode=1;}
}
