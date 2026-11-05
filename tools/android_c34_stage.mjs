// C34: independently authenticated C33 -> C34 Android guest continuation.
// This classifies real read-only checkpoint observations, not an Android boot mock.
// Both four-file contracts MUST pass preflight before calling this function.
// UART belongs only to the current C34 budget, not to C32 or C33.
import assert from 'node:assert/strict';
import {isLiveUserspaceTask} from './android_framework_evidence.mjs';

export const C33_SOURCE=Object.freeze({
  source_commit:'b6d6240292a462a849a80e8e7cb66e70b197de80',
  checkpoint_sha256:'740a564577c991b24d131f4335806ad491413bfa00d8bdeb6942973d46fa348c',
  instructions:14220348839,
  workflow_run:41342890275,
  artifact_id:13098439110
});
const SHA=/^[0-9a-f]{64}$/,COMMIT=/^[0-9a-f]{40}$/;
const EXIT=/init: Service 'odsign' \(pid [1-9]\d*\) exited with status (\d+)\b/g;
function odsignExits(consoleText){
  return [...consoleText.matchAll(EXIT)].map(x=>Number(x[1]));
}
function liveTasks(tasks,name){
  return tasks.some(t=>t.name===name&&isLiveUserspaceTask(t));
}
export function classifyC34({
  sourceContract,sourceReport,sourceBoundary,
  outputContract,report,result,host,boundary,kernel,frame,framework,
  sourceAuthenticated=false,outputAuthenticated=false
}){
  assert.equal(sourceAuthenticated,true,'Full original C33 four-file preflight required');
  assert.equal(outputAuthenticated,true,'Full C34 four-file preflight required');
  assert.equal(sourceContract?.schema,'reverse-c27-profile-resume-contract-v1');
  assert.equal(sourceContract.source_commit,C33_SOURCE.source_commit);
  assert.equal(sourceContract.checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.match(sourceContract.report_sha256,SHA);
  assert.equal(sourceReport?.checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(sourceReport.guest?.instructions,C33_SOURCE.instructions);
  assert.equal(sourceReport.error,'');
  assert.equal(sourceBoundary?.schema,'reverse-c27-sequence-boundary-v1');
  assert.equal(sourceBoundary.clean_completed,true);
  assert.equal(sourceBoundary.checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(sourceBoundary.instructions,C33_SOURCE.instructions);

  assert.equal(outputContract?.schema,'reverse-c27-profile-resume-contract-v1');
  assert.match(outputContract.source_commit,COMMIT);
  assert.match(outputContract.checkpoint_sha256,SHA);
  assert.notEqual(outputContract.checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(report?.checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(report.initial_instructions,C33_SOURCE.instructions);
  assert.ok(Number.isSafeInteger(report.guest?.instructions));
  assert.ok(report.guest.instructions>C33_SOURCE.instructions,'CPU must advance beyond C33');
  assert.equal(report.advanced_instructions,report.guest.instructions-C33_SOURCE.instructions);
  assert.equal(report.error,'');
  assert.ok(['host_wall_budget','instruction_budget'].includes(report.stop_reason));
  assert.equal(report.guest.halted,false);
  assert.equal(report.guest.waiting,false);
  assert.equal(typeof report.console,'string');

  const v=report.provenance;
  assert.equal(v?.experiment,'c27-profile-budget-continuation');
  assert.equal(v.input_checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(v.input_report_sha256,sourceContract.report_sha256);
  assert.equal(v.snapshot_round_trip_verified,true);
  assert.equal(v.source_profile_verified,true);
  assert.equal(v.original_state_modified,false);
  assert.equal(v.ram_profile_changed,false);
  assert.equal(v.host_transport_policy_serialized,false);

  assert.equal(result?.checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(result.instructions,report.guest.instructions);
  assert.equal(result.execution_progress_accepted,true);
  for(const flag of ['guest_boot_failure','guest_reboot_requested','guest_kernel_panic','guest_terminal_failure'])assert.equal(result[flag],false);
  assert.equal(host?.status,'process_completed');
  assert.equal(host.exit_code,0);
  assert.equal(host.trigger,null);
  assert.equal(host.forced_kill,false);
  assert.equal(host.checkpoint_bytes_modified_by_supervisor,false);
  assert.equal(boundary?.schema,'reverse-c27-sequence-boundary-v1');
  assert.equal(boundary.clean_completed,true);
  assert.equal(boundary.checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(boundary.instructions,report.guest.instructions);

  assert.equal(kernel?.status,'read_only_kernel_observation');
  assert.equal(kernel.checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(kernel.instructions,report.guest.instructions);
  assert.equal(kernel.input_unchanged,true);
  assert.equal(frame?.schema,'reverse-snapshot-scanout-v1');
  assert.equal(frame.source_checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(frame.original_snapshot_unchanged,true);
  assert.equal(frame.cpu_instructions_executed,0);
  assert.ok(['scanout_exported','no_scanout'].includes(frame.status));
  assert.equal(framework?.schema,'reverse-android-runtime-evidence-v1');
  assert.equal(framework.checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(framework.instructions,report.guest.instructions);
  assert.equal(framework.original_snapshot_unchanged,true);
  assert.equal(framework.scanout_exported,frame.status==='scanout_exported');
  assert.equal(framework.ready_for_visual_review,Boolean(framework.framework_runtime_verified&&framework.scanout_exported));
  assert.equal(framework.android_display_content_verified,false);

  const tasks=Array.isArray(kernel.tasks)?kernel.tasks:[];
  const names=['odsign','servicemanager','hwservicemanager','zygote64','zygote','system_server','surfaceflinger'];
  const live=names.filter(name=>liveTasks(tasks,name));
  const consoleLines=report.console.split(/\r?\n/).filter(Boolean);
  const exits=odsignExits(report.console);
  const started=['zygote64','zygote','surfaceflinger','hwservicemanager'].filter(name=>
    consoleLines.some(line=>line.includes("init: starting service '"+name+"'")));
  const processesLive=live.some(name=>['zygote64','zygote','system_server','surfaceflinger'].includes(name));
  const stage=framework.framework_runtime_verified?'framework_verified':
    processesLive?'framework_processes_running':
    started.includes('zygote64')||started.includes('zygote')?'zygote_start_requested':
    exits.some(x=>x!==0)?'odsign_exit_nonzero':
    exits.includes(0)?'odsign_exited_clean':
    live.includes('odsign')?'odsign_running':
    'post_odsign_unresolved';
  const current=kernel.current_task_details;
  const currentVerified=current&&kernel.current_pid===current.pid&&kernel.current_task===current.task&&
    Number.isSafeInteger(current.pid)&&current.pid>0&&['process_list','process_thread_list'].includes(current.verification);
  return {
    schema:'reverse-android-c34-stage-v1',
    original_c33_source_authenticated:true,
    c34_output_authenticated:true,
    source_sha256:C33_SOURCE.checkpoint_sha256,
    output_sha256:outputContract.checkpoint_sha256,
    instructions:report.guest.instructions,
    added_instructions:report.guest.instructions-C33_SOURCE.instructions,
    stage,
    uart_scope:'current_authenticated_c34_continuation',
    odsign_exit_zero_verified:exits.includes(0),
    odsign_exit_nonzero_observed:exits.some(x=>x!==0),
    odsign_live:live.includes('odsign'),
    live_processes:live,
    started_services:started,
    process_count:kernel.task_count,
    current_task:currentVerified?{pid:current.pid,name:current.name}:null,
    live_framework_verified:framework.framework_runtime_verified,
    sys_boot_completed_live:framework.sys_boot_completed_live,
    selinux_enforcing_live:framework.selinux_enforcing_live,
    scanout_exported:framework.scanout_exported,
    ready_for_visual_review:framework.ready_for_visual_review,
    android_display_content_verified:false,
    reasons:framework.reasons,
    last_uart_lines:consoleLines.slice(-40).map(s=>s.slice(0,300)),
    source_checkpoint_unchanged:true
  };
}
