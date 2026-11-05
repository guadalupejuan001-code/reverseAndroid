// C35: genuine C34 -> C35 continuation stage. No guest boot is simulated.
// C34's checkpoint and its independent diagnosis are both authenticated before
// admitting the source. C35 output is independently authenticated by preflight.
import assert from 'node:assert/strict';
import {admitC34Source} from './android_c35_source_gate.mjs';
import {isLiveUserspaceTask} from './android_framework_evidence.mjs';
const SHA=/^[0-9a-f]{64}$/,COMMIT=/^[0-9a-f]{40}$/;
function odsignExitCodes(s){
  return [...s.matchAll(/init: Service 'odsign' \(pid [1-9]\d*\) exited with status (\d+)\b/g)].map(m=>Number(m[1]));
}
export function classifyC35({
  sourceContract,sourceReport,sourceBoundary,sourceStage,
  outputContract,report,result,host,boundary,kernel,frame,framework,
  sourceAuthenticated=false,outputAuthenticated=false
}){
  const input=admitC34Source({contract:sourceContract,report:sourceReport,
    boundary:sourceBoundary,stage:sourceStage,authenticated:sourceAuthenticated});
  assert.equal(outputAuthenticated,true,'C35 output requires four-file preflight');
  assert.equal(outputContract?.schema,'reverse-c27-profile-resume-contract-v1');
  assert.match(outputContract.source_commit,COMMIT);
  assert.match(outputContract.checkpoint_sha256,SHA);
  assert.notEqual(outputContract.checkpoint_sha256,input.source_checkpoint_sha256);
  assert.equal(report?.checkpoint_sha256,outputContract.checkpoint_sha256);
  assert.equal(report.initial_instructions,input.source_instructions);
  assert.ok(Number.isSafeInteger(report.guest?.instructions)&&report.guest.instructions>input.source_instructions);
  assert.equal(report.advanced_instructions,report.guest.instructions-input.source_instructions);
  assert.equal(report.error,'');
  assert.ok(['host_wall_budget','instruction_budget'].includes(report.stop_reason));
  assert.equal(report.guest.halted,false);
  assert.equal(report.guest.waiting,false);
  const v=report.provenance;
  assert.equal(v?.experiment,'c27-profile-budget-continuation');
  assert.equal(v.input_checkpoint_sha256,input.source_checkpoint_sha256);
  assert.equal(v.input_report_sha256,input.source_report_sha256);
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
  assert.equal(typeof report.console,'string');
  const lines=report.console.split(/\r?\n/).filter(Boolean);
  const tasks=Array.isArray(kernel.tasks)?kernel.tasks:[];
  const names=['odsign','servicemanager','hwservicemanager','zygote64','zygote','system_server','surfaceflinger'];
  const live=names.filter(name=>tasks.some(t=>t.name===name&&isLiveUserspaceTask(t)));
  const exits=odsignExitCodes(report.console);
  const started=['zygote64','zygote','surfaceflinger','hwservicemanager'].filter(name=>
    lines.some(line=>line.includes("init: starting service '"+name+"'")));
  const processes=live.some(x=>['zygote64','zygote','system_server','surfaceflinger'].includes(x));
  const stage=framework.framework_runtime_verified?'framework_verified':
    processes?'framework_processes_running':
    started.includes('zygote64')||started.includes('zygote')?'zygote_start_requested':
    exits.some(x=>x!==0)?'odsign_exit_nonzero':
    exits.includes(0)?'odsign_exited_clean':
    live.includes('odsign')?'odsign_running':'post_odsign_unresolved';
  const current=kernel.current_task_details;
  const currentVerified=current&&kernel.current_pid===current.pid&&kernel.current_task===current.task&&
    Number.isSafeInteger(current.pid)&&current.pid>0&&['process_list','process_thread_list'].includes(current.verification);
  return {
    schema:'reverse-android-c35-stage-v1',
    source_c34_authenticated:true,output_c35_authenticated:true,
    source_sha256:input.source_checkpoint_sha256,
    output_sha256:outputContract.checkpoint_sha256,
    instructions:report.guest.instructions,
    added_instructions:report.guest.instructions-input.source_instructions,
    source_stage:input.source_stage,
    stage,
    uart_scope:'current_authenticated_c35_continuation',
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
    last_uart_lines:lines.slice(-40).map(s=>s.slice(0,300)),
    source_checkpoint_unchanged:true
  };
}
