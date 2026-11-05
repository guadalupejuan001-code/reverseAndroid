// C33 host-only diagnosis of a genuine C32 -> C33 Android guest continuation.
// Both source and output are independently authenticated with the existing
// four-file C27-profile preflight BEFORE this function can be invoked.
// C33's native UART is NEW for this execution, not cumulative across C32.
import assert from 'node:assert/strict';
import {isLiveUserspaceTask} from './android_framework_evidence.mjs';

export const C32_SOURCE=Object.freeze({
 source_commit:'48f1e18f30e9072d81cbb73ab4724f317d5613c6',
 checkpoint_sha256:'721a228423e55810f11dc4295c64e41feca1fb2121d6db8a0666a6e219ad1a53',
 instructions:13867398839
});
const isSha=s=>typeof s==='string'&&/^[0-9a-f]{64}$/.test(s);
function statusFromLines(lines,service){
 // Only fixed, code-defined service names; never use external input as a regexp.
 const re=new RegExp("init: Service '"+service+"' \\(pid [1-9]\\d*\\) exited with status (\\d+)\\b");
 const statuses=lines.map(line=>line.match(re)?.[1]).filter(v=>v!==undefined).map(Number);
 return {exit_zero:statuses.includes(0),exit_nonzero:statuses.some(x=>x!==0),exit_observed:statuses.length>0};
}
export function classifyC33({
 sourceContract,sourceReport,sourceBoundary,
 outputContract,report,result,host,boundary,
 kernel,frame,framework,sourceAuthenticated=false,outputAuthenticated=false
}){
 assert.equal(sourceAuthenticated,true,'Full source C32 four-file preflight required');
 assert.equal(outputAuthenticated,true,'Full output C33 four-file preflight required');
 assert.equal(sourceContract?.schema,'reverse-c27-profile-resume-contract-v1');
 assert.equal(sourceContract.source_commit,C32_SOURCE.source_commit);
 assert.equal(sourceContract.checkpoint_sha256,C32_SOURCE.checkpoint_sha256);
 assert.ok(isSha(sourceContract.report_sha256));
 assert.equal(sourceReport?.checkpoint_sha256,C32_SOURCE.checkpoint_sha256);
 assert.equal(sourceReport?.guest?.instructions,C32_SOURCE.instructions);
 assert.equal(sourceBoundary?.schema,'reverse-c27-sequence-boundary-v1');
 assert.equal(sourceBoundary?.clean_completed,true);
 assert.equal(sourceBoundary?.checkpoint_sha256,C32_SOURCE.checkpoint_sha256);
 assert.equal(sourceBoundary?.instructions,C32_SOURCE.instructions);

 assert.equal(outputContract?.schema,'reverse-c27-profile-resume-contract-v1');
 assert.match(outputContract.source_commit,/^[0-9a-f]{40}$/);
 assert.ok(isSha(outputContract.checkpoint_sha256));
 assert.notEqual(outputContract.checkpoint_sha256,C32_SOURCE.checkpoint_sha256);
 assert.equal(report?.checkpoint_sha256,outputContract.checkpoint_sha256);
 assert.equal(report.initial_instructions,C32_SOURCE.instructions);
 assert.ok(Number.isSafeInteger(report.guest?.instructions));
 assert.ok(report.guest.instructions>C32_SOURCE.instructions);
 assert.equal(report.advanced_instructions,report.guest.instructions-C32_SOURCE.instructions);
 assert.equal(report.error,'');
 assert.ok(['host_wall_budget','instruction_budget'].includes(report.stop_reason));
 assert.equal(report.guest.halted,false);
 assert.equal(report.guest.waiting,false);

 assert.equal(report.provenance?.experiment,'c27-profile-budget-continuation');
 assert.equal(report.provenance.input_checkpoint_sha256,C32_SOURCE.checkpoint_sha256);
 assert.equal(report.provenance.input_report_sha256,sourceContract.report_sha256);
 assert.equal(report.provenance.snapshot_round_trip_verified,true);
 assert.equal(report.provenance.source_profile_verified,true);
 assert.equal(report.provenance.original_state_modified,false);
 assert.equal(report.provenance.ram_profile_changed,false);
 assert.equal(report.provenance.host_transport_policy_serialized,false);
 assert.equal(result?.checkpoint_sha256,outputContract.checkpoint_sha256);
 assert.equal(result.instructions,report.guest.instructions);
 assert.equal(result.execution_progress_accepted,true);
 for(const k of ['guest_boot_failure','guest_reboot_requested','guest_kernel_panic','guest_terminal_failure'])assert.equal(result[k],false);

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
 const serviceNames=['odsign','zygote','zygote64','surfaceflinger','servicemanager','hwservicemanager'];
 const services=Object.fromEntries(serviceNames.map(name=>[name,statusFromLines(lines,name)]));
 const liveTasks=(Array.isArray(kernel.tasks)?kernel.tasks:[]).filter(isLiveUserspaceTask);
 const liveNamed=new Set(liveTasks.map(t=>t.name));
 const processNames=['odsign','zygote64','zygote','app_process64','system_server','surfaceflinger','servicemanager','hwservicemanager'];
 const liveProcesses=processNames.filter(name=>liveNamed.has(name));
 const startedServices=['zygote','zygote64','surfaceflinger','servicemanager','hwservicemanager'].filter(name=>
  lines.some(line=>line.includes("init: starting service '"+name+"'"))
 );
 const running=kernel.current_task_details;
 const currentValid=running&&kernel.current_task===running.task&&kernel.current_pid===running.pid&&
  Number.isSafeInteger(running.pid)&&running.pid>0&&['process_list','process_thread_list'].includes(running.verification);
 const stage=framework.framework_runtime_verified?'framework_verified':
  liveProcesses.some(n=>['zygote64','zygote','system_server','surfaceflinger'].includes(n))?'framework_processes_running':
  startedServices.some(n=>n==='zygote'||n==='zygote64')?'zygote_start_requested':
  services.odsign.exit_nonzero?'odsign_exited_error':
  services.odsign.exit_zero?'odsign_exited_clean':
  liveProcesses.includes('odsign')?'odsign_running':
  'post_art_boot_unresolved';

 return {
  schema:'reverse-android-c33-stage-v1',
  c32_source_authenticated:true,c33_output_authenticated:true,
  source_sha256:C32_SOURCE.checkpoint_sha256,
  output_sha256:outputContract.checkpoint_sha256,
  instructions:report.guest.instructions,
  added_instructions:report.guest.instructions-C32_SOURCE.instructions,
  uart_scope:'current_authenticated_c33_continuation',
  odsign_exit_zero_verified:services.odsign.exit_zero,
  odsign_exit_nonzero_observed:services.odsign.exit_nonzero,
  odsign_live:liveProcesses.includes('odsign'),
  process_count:kernel.task_count,
  live_processes:liveProcesses,
  started_services:startedServices,
  stage, current_task:currentValid?{pid:running.pid,name:running.name}:null,
  live_framework_verified:framework.framework_runtime_verified,
  boot_completed_live:framework.sys_boot_completed_live,
  selinux_enforcing_live:framework.selinux_enforcing_live,
  scanout_exported:framework.scanout_exported,
  ready_for_visual_review:framework.ready_for_visual_review,
  android_display_content_verified:false,
  reasons:framework.reasons,
  last_uart_lines:lines.slice(-40).map(line=>line.slice(0,300)),
  source_checkpoint_unchanged:true
 };
}
