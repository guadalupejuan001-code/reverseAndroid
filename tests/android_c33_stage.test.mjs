// C33 host-only policy fixtures, NEVER an Android boot.
import test from 'node:test';
import assert from 'node:assert/strict';
import {C32_SOURCE,classifyC33} from '../tools/android_c33_stage.mjs';

const inputReportSha='d'.repeat(64);
const outputSha='e'.repeat(64);
const sourceName='4'.repeat(40);
const makeTask=(pid,name,state=1)=>({pid,name,task:'0xffff'+pid,state,exit_state:0,has_user_mm:true});
function fixture(consoleText=''){
 const instructions=C32_SOURCE.instructions+2000000;
 const sourceContract={schema:'reverse-c27-profile-resume-contract-v1',source_commit:C32_SOURCE.source_commit,checkpoint_sha256:C32_SOURCE.checkpoint_sha256,report_sha256:inputReportSha};
 const sourceReport={checkpoint_sha256:C32_SOURCE.checkpoint_sha256,guest:{instructions:C32_SOURCE.instructions},console:'init: starting service odsign'};
 const sourceBoundary={schema:'reverse-c27-sequence-boundary-v1',checkpoint_sha256:C32_SOURCE.checkpoint_sha256,instructions:C32_SOURCE.instructions,clean_completed:true};
 const outputContract={schema:'reverse-c27-profile-resume-contract-v1',source_commit:sourceName,checkpoint_sha256:outputSha};
 const report={checkpoint_sha256:outputSha,initial_instructions:C32_SOURCE.instructions,advanced_instructions:2000000,
  guest:{instructions,halted:false,waiting:false},error:'',stop_reason:'host_wall_budget',console:consoleText,
  provenance:{experiment:'c27-profile-budget-continuation',input_checkpoint_sha256:C32_SOURCE.checkpoint_sha256,
   input_report_sha256:inputReportSha,snapshot_round_trip_verified:true,source_profile_verified:true,
   original_state_modified:false,ram_profile_changed:false,host_transport_policy_serialized:false}};
 const result={checkpoint_sha256:outputSha,instructions,execution_progress_accepted:true,
  guest_boot_failure:false,guest_reboot_requested:false,guest_kernel_panic:false,guest_terminal_failure:false};
 const host={status:'process_completed',exit_code:0,trigger:null,forced_kill:false,checkpoint_bytes_modified_by_supervisor:false};
 const boundary={schema:'reverse-c27-sequence-boundary-v1',clean_completed:true,checkpoint_sha256:outputSha,instructions};
 const tasks=[makeTask(1,'init'),makeTask(554,'odsign',0)];
 const kernel={status:'read_only_kernel_observation',checkpoint_sha256:outputSha,input_unchanged:true,instructions,
  tasks,task_count:tasks.length,current_task_details:{pid:554,task:'0xffff554',name:'odsign',verification:'process_list'},
  current_task:'0xffff554',current_pid:554};
 const frame={schema:'reverse-snapshot-scanout-v1',status:'no_scanout',source_checkpoint_sha256:outputSha,original_snapshot_unchanged:true,cpu_instructions_executed:0};
 const framework={schema:'reverse-android-runtime-evidence-v1',checkpoint_sha256:outputSha,instructions,
  original_snapshot_unchanged:true,framework_runtime_verified:false,sys_boot_completed_live:false,
  selinux_enforcing_live:true,scanout_exported:false,ready_for_visual_review:false,
  android_display_content_verified:false,reasons:['live_process_missing:zygote64']};
 return {sourceContract,sourceReport,sourceBoundary,outputContract,report,result,host,boundary,
  kernel,frame,framework,sourceAuthenticated:true,outputAuthenticated:true};
}
test('C32 provenance, CPU progress and live odsign observed without inventing Zygote',()=>{
 const r=classifyC33(fixture());
 assert.equal(r.source_sha256,C32_SOURCE.checkpoint_sha256);
 assert.equal(r.stage,'odsign_running');
 assert.equal(r.odsign_exit_zero_verified,false);
 assert.equal(r.odsign_live,true);
 assert.equal(r.added_instructions,2000000);
 assert.equal(r.live_framework_verified,false);
 assert.equal(r.android_display_content_verified,false);
});
test('init command to start odsign is not evidence of its exit',()=>{
 const r=classifyC33(fixture("[ 223.4] init: Command 'start odsign' action=post-fs-data took 10ms and succeeded\r\n"));
 assert.equal(r.odsign_exit_zero_verified,false);
});
test('CRLF UART completion confirms odsign only when actual process exits zero',()=>{
 const x=fixture("[ 230.2] init: Service 'odsign' (pid 554) exited with status 0 waiting took 10 seconds\r\n");
 x.kernel.tasks.pop();x.kernel.task_count=1;
 const r=classifyC33(x);assert.equal(r.stage,'odsign_exited_clean');
 assert.equal(r.odsign_exit_zero_verified,true);assert.equal(r.odsign_live,false);
 assert.equal(r.last_uart_lines.length,1);
});
test('nonzero odsign exit is a failure, not completion',()=>{
 const x=fixture("init: Service 'odsign' (pid 554) exited with status 1 waiting took 10 seconds\n");
 x.kernel.tasks.pop();x.kernel.task_count=1;
 const r=classifyC33(x);assert.equal(r.odsign_exit_nonzero_observed,true);
 assert.equal(r.odsign_exit_zero_verified,false);
 assert.equal(r.stage,'odsign_exited_error');
});
test('zygote start is weaker than living zygote process',()=>{
 const x=fixture("[ 232] init: starting service 'zygote'...\n");
 x.kernel.tasks.pop();x.kernel.task_count=1;
 const r=classifyC33(x);
 assert.equal(r.stage,'zygote_start_requested');assert.equal(r.live_framework_verified,false);
});
test('running live zygote is independently observed',()=>{
 const x=fixture("init: starting service 'zygote'...\n");
 x.kernel.tasks.push(makeTask(612,'zygote64',0));x.kernel.task_count=3;
 const r=classifyC33(x);assert.equal(r.stage,'framework_processes_running');
 assert.deepEqual(r.live_processes,['odsign','zygote64']);
 assert.equal(r.live_framework_verified,false);
});
test('zombie, kernel thread, stopped and malformed process cannot count as zygote',()=>{
 for(const mutation of [{exit_state:32},{has_user_mm:false},{state:128},{state:0x8000},{pid:0}]){
  const x=fixture();x.kernel.tasks.push({...makeTask(612,'zygote64',1),...mutation});x.kernel.task_count=3;
  const r=classifyC33(x);
  assert.equal(r.stage,'odsign_running');assert.equal(r.live_processes.includes('zygote64'),false);
 }
});
const invalid=[
 ['missing source authentication',x=>x.sourceAuthenticated=false],
 ['missing output authentication',x=>x.outputAuthenticated=false],
 ['wrong C32 source SHA',x=>x.sourceContract.checkpoint_sha256='0'.repeat(64)],
 ['wrong C32 source commit',x=>x.sourceContract.source_commit='0'.repeat(40)],
 ['wrong source instruction count',x=>x.sourceReport.guest.instructions++],
 ['source not clean',x=>x.sourceBoundary.clean_completed=false],
 ['bad source report pin',x=>x.sourceContract.report_sha256='x'],
 ['changed native CPU first count',x=>x.report.initial_instructions++],
 ['zero CPU progress',x=>{x.report.guest.instructions=C32_SOURCE.instructions;x.report.advanced_instructions=0;}],
 ['invalid CPU progress delta',x=>x.report.advanced_instructions++],
 ['changed CPU input checkpoint',x=>x.report.provenance.input_checkpoint_sha256='0'.repeat(64)],
 ['changed CPU input report',x=>x.report.provenance.input_report_sha256='0'.repeat(64)],
 ['lost byte identical restore',x=>x.report.provenance.snapshot_round_trip_verified=false],
 ['mutated guest before resume',x=>x.report.provenance.original_state_modified=true],
 ['invalid host status',x=>x.host.status='process_failed'],
 ['invalid host exit',x=>x.host.exit_code=9],
 ['guest terminal',x=>x.result.guest_boot_failure=true],
 ['guest reboot',x=>x.result.guest_reboot_requested=true],
 ['missing completed budget',x=>x.boundary.clean_completed=false],
 ['wrong result SHA',x=>x.result.checkpoint_sha256='0'.repeat(64)],
 ['cross source output',x=>x.outputContract.checkpoint_sha256=C32_SOURCE.checkpoint_sha256],
 ['mixed live kernel observation',x=>x.kernel.checkpoint_sha256='0'.repeat(64)],
 ['altered snapshot evidence',x=>x.kernel.input_unchanged=false],
 ['mixed frame',x=>x.frame.source_checkpoint_sha256='0'.repeat(64)],
 ['frame executes instructions',x=>x.frame.cpu_instructions_executed=1],
 ['mixed framework',x=>x.framework.checkpoint_sha256='0'.repeat(64)],
 ['unsupported boot visual claim',x=>x.framework.android_display_content_verified=true],
 ['disagreement frame/framework',x=>x.framework.scanout_exported=true],
];
for(const [label,change] of invalid)test('reject '+label,()=>{
 const x=fixture();change(x);assert.throws(()=>classifyC33(x));
});
