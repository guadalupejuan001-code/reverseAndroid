// C34 stage-policy unit fixtures. These are NOT Android boot tests.
import test from 'node:test';
import assert from 'node:assert/strict';
import {C33_SOURCE,classifyC34} from '../tools/android_c34_stage.mjs';

const inputReportSha='d'.repeat(64),outSha='e'.repeat(64),producer='4'.repeat(40);
const task=(pid,name,state=1)=>({pid,name,task:'0xffff'+pid,state,exit_state:0,has_user_mm:true});
function fixture(consoleText=''){
  const instructions=C33_SOURCE.instructions+2500000;
  const sourceContract={schema:'reverse-c27-profile-resume-contract-v1',
    source_commit:C33_SOURCE.source_commit,checkpoint_sha256:C33_SOURCE.checkpoint_sha256,report_sha256:inputReportSha};
  const sourceReport={checkpoint_sha256:C33_SOURCE.checkpoint_sha256,
    guest:{instructions:C33_SOURCE.instructions},error:'',console:'init: starting service odsign'};
  const sourceBoundary={schema:'reverse-c27-sequence-boundary-v1',clean_completed:true,
    checkpoint_sha256:C33_SOURCE.checkpoint_sha256,instructions:C33_SOURCE.instructions};
  const outputContract={schema:'reverse-c27-profile-resume-contract-v1',source_commit:producer,checkpoint_sha256:outSha};
  const report={checkpoint_sha256:outSha,initial_instructions:C33_SOURCE.instructions,
    advanced_instructions:2500000,guest:{instructions,halted:false,waiting:false},error:'',
    stop_reason:'host_wall_budget',console:consoleText,provenance:{
      experiment:'c27-profile-budget-continuation',input_checkpoint_sha256:C33_SOURCE.checkpoint_sha256,
      input_report_sha256:inputReportSha,snapshot_round_trip_verified:true,
      source_profile_verified:true,original_state_modified:false,
      ram_profile_changed:false,host_transport_policy_serialized:false
    }};
  const result={checkpoint_sha256:outSha,instructions,execution_progress_accepted:true,
    guest_boot_failure:false,guest_reboot_requested:false,guest_kernel_panic:false,guest_terminal_failure:false};
  const host={status:'process_completed',exit_code:0,trigger:null,forced_kill:false,
    checkpoint_bytes_modified_by_supervisor:false};
  const boundary={schema:'reverse-c27-sequence-boundary-v1',clean_completed:true,
    checkpoint_sha256:outSha,instructions};
  const tasks=[task(1,'init'),task(554,'odsign',0),task(55,'servicemanager',0)];
  const kernel={status:'read_only_kernel_observation',checkpoint_sha256:outSha,
    instructions,input_unchanged:true,tasks,task_count:tasks.length,
    current_task_details:{pid:554,task:'0xffff554',name:'odsign',verification:'process_list'},
    current_task:'0xffff554',current_pid:554};
  const frame={schema:'reverse-snapshot-scanout-v1',status:'no_scanout',
    source_checkpoint_sha256:outSha,original_snapshot_unchanged:true,cpu_instructions_executed:0};
  const framework={schema:'reverse-android-runtime-evidence-v1',checkpoint_sha256:outSha,instructions,
    original_snapshot_unchanged:true,framework_runtime_verified:false,sys_boot_completed_live:false,
    selinux_enforcing_live:true,scanout_exported:false,ready_for_visual_review:false,
    android_display_content_verified:false,reasons:['live_process_missing:zygote64']};
  return {sourceContract,sourceReport,sourceBoundary,outputContract,report,result,host,boundary,
    kernel,frame,framework,sourceAuthenticated:true,outputAuthenticated:true};
}
test('authenticated C33 advances CPU but does not invent zygote or display',()=>{
  const r=classifyC34(fixture());
  assert.equal(r.source_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(r.added_instructions,2500000);
  assert.equal(r.odsign_live,true);
  assert.equal(r.stage,'odsign_running');
  assert.equal(r.live_framework_verified,false);
  assert.equal(r.android_display_content_verified,false);
});
test('start command is not an odsign exit',()=>{
  const r=classifyC34(fixture("init: Command 'start odsign' action=post-fs-data took 10ms and succeeded\n"));
  assert.equal(r.odsign_exit_zero_verified,false);
});
test('an actual zero exit is classified without inventing a framework',()=>{
  const x=fixture("init: Service 'odsign' (pid 554) exited with status 0 waiting took 12 seconds\r\n");
  x.kernel.tasks.splice(1,1);x.kernel.task_count=x.kernel.tasks.length;
  const r=classifyC34(x);
  assert.equal(r.stage,'odsign_exited_clean');
  assert.equal(r.odsign_live,false);
  assert.equal(r.odsign_exit_zero_verified,true);
});
test('nonzero odsign exit is not clean completion',()=>{
  const x=fixture("init: Service 'odsign' (pid 554) exited with status 1 waiting took 12 seconds\n");
  x.kernel.tasks.splice(1,1);x.kernel.task_count=x.kernel.tasks.length;
  const r=classifyC34(x);
  assert.equal(r.stage,'odsign_exit_nonzero');
  assert.equal(r.odsign_exit_nonzero_observed,true);
});
test('zygote start request is weaker than a live zygote',()=>{
  const r=classifyC34(fixture("init: starting service 'zygote64'...\n"));
  assert.equal(r.stage,'zygote_start_requested');
  assert.equal(r.live_framework_verified,false);
});
test('real live zygote upgrades process status without inventing boot',()=>{
  const x=fixture();x.kernel.tasks.push(task(612,'zygote64',0));
  const r=classifyC34(x);assert.equal(r.stage,'framework_processes_running');
  assert.equal(r.live_processes.includes('zygote64'),true);
  assert.equal(r.sys_boot_completed_live,false);
});
test('zombies and kernel threads never count as live zygote',()=>{
  for(const mutation of [{exit_state:32},{has_user_mm:false},{state:128},{pid:0}]){
    const x=fixture();x.kernel.tasks.push({...task(612,'zygote64'),...mutation});
    const r=classifyC34(x);
    assert.equal(r.live_processes.includes('zygote64'),false);
  }
});
const invalid=[
  ['unauthenticated C33 source',x=>x.sourceAuthenticated=false],
  ['unauthenticated C34 output',x=>x.outputAuthenticated=false],
  ['wrong C33 checkpoint hash',x=>x.sourceContract.checkpoint_sha256='0'.repeat(64)],
  ['wrong producer',x=>x.sourceContract.source_commit='0'.repeat(40)],
  ['wrong C33 instruction count',x=>x.sourceReport.guest.instructions++],
  ['unclean C33',x=>x.sourceBoundary.clean_completed=false],
  ['wrong C33 source report digest',x=>x.sourceContract.report_sha256='invalid'],
  ['bad input CPU count',x=>x.report.initial_instructions++],
  ['zero progress',x=>{x.report.guest.instructions=C33_SOURCE.instructions;x.report.advanced_instructions=0;}],
  ['invalid delta',x=>x.report.advanced_instructions++],
  ['wrong input checkpoint provenance',x=>x.report.provenance.input_checkpoint_sha256='0'.repeat(64)],
  ['wrong input report provenance',x=>x.report.provenance.input_report_sha256='0'.repeat(64)],
  ['failed source snapshot proof',x=>x.report.provenance.snapshot_round_trip_verified=false],
  ['modified input state',x=>x.report.provenance.original_state_modified=true],
  ['native error',x=>x.report.error='architectural_stop'],
  ['host error',x=>x.host.exit_code=2],
  ['forced kill',x=>x.host.forced_kill=true],
  ['guest reboot',x=>x.result.guest_reboot_requested=true],
  ['guest panic',x=>x.result.guest_kernel_panic=true],
  ['unclean output',x=>x.boundary.clean_completed=false],
  ['mixed source/output checkpoint',x=>x.outputContract.checkpoint_sha256=C33_SOURCE.checkpoint_sha256],
  ['mixed result digest',x=>x.result.checkpoint_sha256='0'.repeat(64)],
  ['mixed kernel',x=>x.kernel.checkpoint_sha256='0'.repeat(64)],
  ['mutating observer',x=>x.kernel.input_unchanged=false],
  ['mixed scanout',x=>x.frame.source_checkpoint_sha256='0'.repeat(64)],
  ['active scanout runs CPU',x=>x.frame.cpu_instructions_executed=1],
  ['mixed framework',x=>x.framework.checkpoint_sha256='0'.repeat(64)],
  ['fabricated display proof',x=>x.framework.android_display_content_verified=true],
  ['mismatched scanout',x=>x.framework.scanout_exported=true]
];
for(const [label,change] of invalid)test('reject '+label,()=>{
  const x=fixture();change(x);assert.throws(()=>classifyC34(x));
});
