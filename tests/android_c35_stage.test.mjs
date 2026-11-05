// Host-only C35 stage fixtures. Real Android is exercised by the separate CI workflow.
import test from 'node:test';
import assert from 'node:assert/strict';
import {C33_SOURCE} from '../tools/android_c34_stage.mjs';
import {C34_PRODUCER} from '../tools/android_c35_source_gate.mjs';
import {classifyC35} from '../tools/android_c35_stage.mjs';
const sourceSha='a'.repeat(64),outSha='f'.repeat(64),sourceCount=C33_SOURCE.instructions+100000;
const makeTask=(pid,name,state=1)=>({pid,name,task:'0xffff'+pid,state,exit_state:0,has_user_mm:true});
function fixture(uart=''){
  const instructions=sourceCount+200000;
  const sourceContract={schema:'reverse-c27-profile-resume-contract-v1',
    source_commit:C34_PRODUCER.commit,checkpoint_sha256:sourceSha,
    report_sha256:'b'.repeat(64),result_sha256:'c'.repeat(64),process_sha256:'d'.repeat(64)};
  const sourceReport={checkpoint_sha256:sourceSha,initial_instructions:C33_SOURCE.instructions,
    guest:{instructions:sourceCount,halted:false,waiting:false},advanced_instructions:100000,
    stop_reason:'host_wall_budget',error:'',provenance:{
      experiment:'c27-profile-budget-continuation',input_checkpoint_sha256:C33_SOURCE.checkpoint_sha256,
      snapshot_round_trip_verified:true,original_state_modified:false,ram_profile_changed:false}};
  const sourceBoundary={schema:'reverse-c27-sequence-boundary-v1',
    clean_completed:true,checkpoint_sha256:sourceSha,instructions:sourceCount};
  const sourceStage={schema:'reverse-android-c34-stage-v1',
    original_c33_source_authenticated:true,c34_output_authenticated:true,
    source_sha256:C33_SOURCE.checkpoint_sha256,output_sha256:sourceSha,
    instructions:sourceCount,added_instructions:100000,stage:'odsign_running',
    source_checkpoint_unchanged:true,android_display_content_verified:false,
    selinux_enforcing_live:true,live_framework_verified:false,sys_boot_completed_live:false,
    scanout_exported:false,ready_for_visual_review:false};
  const outputContract={schema:'reverse-c27-profile-resume-contract-v1',source_commit:'4'.repeat(40),checkpoint_sha256:outSha};
  const report={checkpoint_sha256:outSha,initial_instructions:sourceCount,
    advanced_instructions:200000,guest:{instructions,halted:false,waiting:false},
    stop_reason:'host_wall_budget',error:'',console:uart,provenance:{
      experiment:'c27-profile-budget-continuation',
      input_checkpoint_sha256:sourceSha,input_report_sha256:sourceContract.report_sha256,
      snapshot_round_trip_verified:true,source_profile_verified:true,
      original_state_modified:false,ram_profile_changed:false,host_transport_policy_serialized:false}};
  const result={checkpoint_sha256:outSha,instructions,execution_progress_accepted:true,
    guest_boot_failure:false,guest_reboot_requested:false,guest_kernel_panic:false,guest_terminal_failure:false};
  const host={status:'process_completed',exit_code:0,trigger:null,forced_kill:false,
    checkpoint_bytes_modified_by_supervisor:false};
  const boundary={schema:'reverse-c27-sequence-boundary-v1',clean_completed:true,
    checkpoint_sha256:outSha,instructions};
  const tasks=[makeTask(1,'init'),makeTask(554,'odsign',0),makeTask(56,'servicemanager',0)];
  const kernel={status:'read_only_kernel_observation',checkpoint_sha256:outSha,
    instructions,input_unchanged:true,tasks,task_count:tasks.length,
    current_task_details:{pid:554,task:'0xffff554',name:'odsign',verification:'process_list'},
    current_task:'0xffff554',current_pid:554};
  const frame={schema:'reverse-snapshot-scanout-v1',status:'no_scanout',
    source_checkpoint_sha256:outSha,original_snapshot_unchanged:true,cpu_instructions_executed:0};
  const framework={schema:'reverse-android-runtime-evidence-v1',checkpoint_sha256:outSha,
    instructions,original_snapshot_unchanged:true,framework_runtime_verified:false,
    sys_boot_completed_live:false,selinux_enforcing_live:true,scanout_exported:false,
    ready_for_visual_review:false,android_display_content_verified:false,reasons:['live_process_missing:zygote64']};
  return {sourceContract,sourceReport,sourceBoundary,sourceStage,outputContract,report,result,host,boundary,
    kernel,frame,framework,sourceAuthenticated:true,outputAuthenticated:true};
}
test('C35 checkpoint continuation valid but odsign remains running',()=>{
  const s=classifyC35(fixture());
  assert.equal(s.schema,'reverse-android-c35-stage-v1');
  assert.equal(s.source_sha256,sourceSha);
  assert.equal(s.added_instructions,200000);
  assert.equal(s.odsign_live,true);
  assert.equal(s.stage,'odsign_running');
  assert.equal(s.android_display_content_verified,false);
});
test('zygote start event is not a verified live zygote',()=>{
  const s=classifyC35(fixture("init: starting service 'zygote64'...\n"));
  assert.equal(s.stage,'zygote_start_requested');
  assert.equal(s.live_framework_verified,false);
});
test('live zygote observed without claiming sys.boot_completed',()=>{
  const x=fixture();x.kernel.tasks.push(makeTask(610,'zygote64'));
  const s=classifyC35(x);
  assert.equal(s.stage,'framework_processes_running');
  assert.equal(s.live_processes.includes('zygote64'),true);
  assert.equal(s.sys_boot_completed_live,false);
});
test('odsign service exit is distinct from mere init command',()=>{
  const start=classifyC35(fixture("init: Command 'start odsign' succeeded\n"));
  assert.equal(start.odsign_exit_zero_verified,false);
  const x=fixture("init: Service 'odsign' (pid 554) exited with status 0 waiting took 10 s\n");
  x.kernel.tasks.splice(1,1);
  const done=classifyC35(x);assert.equal(done.odsign_exit_zero_verified,true);
  assert.equal(done.stage,'odsign_exited_clean');
});
const invalid=[
  ['unauthenticated source',x=>x.sourceAuthenticated=false],
  ['unauthenticated output',x=>x.outputAuthenticated=false],
  ['untrusted C34 producer',x=>x.sourceContract.source_commit='0'.repeat(40)],
  ['mixed C34 stage digest',x=>x.sourceStage.output_sha256='0'.repeat(64)],
  ['unclean C34 source',x=>x.sourceBoundary.clean_completed=false],
  ['mixed input provenance',x=>x.report.provenance.input_checkpoint_sha256='0'.repeat(64)],
  ['wrong source count',x=>x.report.initial_instructions++],
  ['no new progress',x=>{x.report.guest.instructions=sourceCount;x.report.advanced_instructions=0}],
  ['wrong instruction delta',x=>x.report.advanced_instructions++],
  ['native execution failure',x=>x.report.error='architectural_stop'],
  ['host process failed',x=>x.host.exit_code=2],
  ['forced kill',x=>x.host.forced_kill=true],
  ['guest reboot',x=>x.result.guest_reboot_requested=true],
  ['result digest not source',x=>x.result.checkpoint_sha256='0'.repeat(64)],
  ['unclean boundary',x=>x.boundary.clean_completed=false],
  ['mismatched kernel',x=>x.kernel.checkpoint_sha256='0'.repeat(64)],
  ['mismatched scanout',x=>x.frame.source_checkpoint_sha256='0'.repeat(64)],
  ['frame executes CPU',x=>x.frame.cpu_instructions_executed=1],
  ['mismatched framework',x=>x.framework.checkpoint_sha256='0'.repeat(64)],
  ['fabricated display',x=>x.framework.android_display_content_verified=true]
];
for(const [name,change] of invalid)test('reject '+name,()=>{
  const x=fixture();change(x);assert.throws(()=>classifyC35(x));
});
