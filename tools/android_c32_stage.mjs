// C32 host-only post-hoc stage proof from the authentic C31 -> C32 guest.
// Each call to run_boot_budget_cycles initializes previous_console to empty:
// report.console holds UART from THIS continuation, NOT C31+new concatenated.
// The source and output four-file contracts must be validated before calling.
import assert from 'node:assert/strict';
import {isLiveUserspaceTask} from './android_framework_evidence.mjs';
export const SOURCE_C31=Object.freeze({
 commit:'87c79bd86624268eac1f2b37a71d874355703e0b',
 sha256:'3011b7caaf537657d980b11ae6a1f3f84cc7a1cce180d9e808fe9c8a3d8864cc',
 instructions:13697998839
});
const shaPattern=/^[0-9a-f]{64}$/;
const serviceExited=(text,name,status)=>new RegExp("(?:^|\\n)[^\\n]*init: Service '"+name+"' \\(pid [1-9]\\d*\\) exited with status "+status+"\\b").test(text);
export function evaluateC32({sourceContract,sourceReport,sourceBoundary,report,result,host,boundary,kernel,frame,framework,sourceAuthenticated=false,outputAuthenticated=false}){
 assert.equal(sourceAuthenticated,true,'Full C31 four-file preflight required');
 assert.equal(outputAuthenticated,true,'Full C32 four-file preflight required');
 assert.equal(sourceContract?.source_commit,SOURCE_C31.commit);
 assert.equal(sourceContract?.checkpoint_sha256,SOURCE_C31.sha256);
 assert.match(sourceContract?.report_sha256??'',shaPattern);
 assert.equal(sourceReport?.checkpoint_sha256,SOURCE_C31.sha256);
 assert.equal(sourceReport?.guest?.instructions,SOURCE_C31.instructions);
 assert.equal(sourceBoundary?.schema,'reverse-c27-sequence-boundary-v1');
 assert.equal(sourceBoundary?.checkpoint_sha256,SOURCE_C31.sha256);
 assert.equal(sourceBoundary?.instructions,SOURCE_C31.instructions);
 assert.equal(sourceBoundary?.clean_completed,true);
 assert.match(report?.checkpoint_sha256??'',shaPattern);
 assert.notEqual(report.checkpoint_sha256,SOURCE_C31.sha256);
 assert.equal(report.initial_instructions,SOURCE_C31.instructions);
 assert.ok(Number.isSafeInteger(report.guest?.instructions)&&report.guest.instructions>SOURCE_C31.instructions);
 assert.equal(report.advanced_instructions,report.guest.instructions-SOURCE_C31.instructions);
 assert.equal(result?.checkpoint_sha256,report.checkpoint_sha256);
 assert.equal(result?.instructions,report.guest.instructions);
 assert.equal(result?.execution_progress_accepted,true);
 for(const key of ['guest_boot_failure','guest_reboot_requested','guest_kernel_panic','guest_terminal_failure'])assert.equal(result[key],false);
 assert.equal(host?.status,'process_completed');assert.equal(host.exit_code,0);
 assert.equal(host.trigger,null);assert.equal(host.forced_kill,false);
 assert.equal(boundary?.checkpoint_sha256,report.checkpoint_sha256);
 assert.equal(boundary?.instructions,report.guest.instructions);
 assert.equal(boundary?.clean_completed,true);
 const v=report.provenance;
 assert.equal(v?.experiment,'c27-profile-budget-continuation');
 assert.equal(v.input_checkpoint_sha256,sourceContract.checkpoint_sha256);
 assert.equal(v.input_report_sha256,sourceContract.report_sha256);
 assert.equal(v.snapshot_round_trip_verified,true);
 assert.equal(v.original_state_modified,false);
 assert.equal(kernel?.status,'read_only_kernel_observation');
 assert.equal(kernel.checkpoint_sha256,report.checkpoint_sha256);
 assert.equal(kernel.input_unchanged,true);
 assert.equal(kernel.instructions,report.guest.instructions);
 assert.equal(frame?.source_checkpoint_sha256,report.checkpoint_sha256);
 assert.equal(frame?.cpu_instructions_executed,0);
 assert.equal(frame?.original_snapshot_unchanged,true);
 assert.ok(['scanout_exported','no_scanout'].includes(frame.status));
 assert.equal(framework?.checkpoint_sha256,report.checkpoint_sha256);
 assert.equal(framework?.instructions,report.guest.instructions);
 assert.equal(framework?.scanout_exported,frame.status==='scanout_exported');
 assert.equal(framework?.android_display_content_verified,false);
 assert.equal(framework?.ready_for_visual_review,Boolean(framework.framework_runtime_verified&&framework.scanout_exported));
 assert.equal(typeof sourceReport.console,'string');
 assert.equal(typeof report.console,'string');
 // UART is scoped to this native budget (guest_budget_cycles.hpp), not
 // replayed from the prior snapshot. The direct input SHA attests continuity.
 const fresh=report.console;
 const classpathDone=serviceExited(fresh,'derive_classpath','0');
 const classpathFailed=serviceExited(fresh,'derive_classpath','(?!0\\b)\\d+');
 const artBootDone=serviceExited(fresh,'art_boot','0');
 const artBootFailed=serviceExited(fresh,'art_boot','(?!0\\b)\\d+');
 const tasks=Array.isArray(kernel.tasks)?kernel.tasks:[];
 const alive=name=>tasks.some(task=>task.name===name&&isLiveUserspaceTask(task));
 const liveProcessNames=['zygote64','system_server','surfaceflinger'].filter(alive);
 const odsignLive=alive('odsign');
 const current=kernel.current_task_details;
 const currentVerified=current&&kernel.current_pid===current.pid&&kernel.current_task===current.task&&
  ['process_list','process_thread_list'].includes(current.verification)&&Number.isSafeInteger(current.pid)&&current.pid>0;
 const stage=framework.framework_runtime_verified?'framework_live_verified':
  liveProcessNames.length?'framework_processes_observed':
  classpathFailed||artBootFailed?'post_fs_data_service_failed':
  odsignLive&&artBootDone?'odsign_running_after_art_boot':
  artBootDone?'art_boot_completed':
  classpathDone?'derive_classpath_completed':
  'post_fs_data_continuing';
 return {
  schema:'reverse-android-c32-stage-v2',
  original_c31_source_authenticated:true,
  original_c32_output_authenticated:true,
  source_sha256:SOURCE_C31.sha256,
  output_sha256:report.checkpoint_sha256,
  total_instructions:report.guest.instructions,
  added_instructions:report.guest.instructions-SOURCE_C31.instructions,
  uart_scope:'current_authenticated_continuation',
  derive_classpath_service_exit_zero_verified:classpathDone,
  derive_classpath_service_nonzero_exit_observed:classpathFailed,
  art_boot_service_exit_zero_verified:artBootDone,
  art_boot_service_nonzero_exit_observed:artBootFailed,
  odsign_live_userspace_verified:odsignLive,
  stage,
  process_count:kernel.task_count,
  live_framework_processes:liveProcessNames,
  current_task:currentVerified?{pid:current.pid,name:current.name,verification:current.verification}:null,
  live_framework_verified:framework.framework_runtime_verified,
  live_boot_completed:framework.sys_boot_completed_live,
  scanout_exported:framework.scanout_exported,
  ready_for_visual_review:framework.ready_for_visual_review,
  android_display_content_verified:false,
  readiness_reasons:framework.reasons,
  last_new_console_lines:fresh.split(/\r?\n/).filter(Boolean).slice(-40).map(s=>s.slice(0,280)),
  source_checkpoint_unchanged:true
 };
}
