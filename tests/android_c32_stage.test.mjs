// Synthetic host-only fixtures. They are not a boot result.
import test from 'node:test';
import assert from 'node:assert/strict';
import {SOURCE_C31,evaluateC32} from '../tools/android_c32_stage.mjs';
const reportSha='a'.repeat(64);
function fixture(consoleText=''){
 const sha='e'.repeat(64),n=SOURCE_C31.instructions+1000;
 const sourceContract={source_commit:SOURCE_C31.commit,checkpoint_sha256:SOURCE_C31.sha256,report_sha256:reportSha};
 const sourceReport={checkpoint_sha256:SOURCE_C31.sha256,guest:{instructions:SOURCE_C31.instructions},console:'init: starting derive_classpath on original C31\n'};
 const report={checkpoint_sha256:sha,initial_instructions:SOURCE_C31.instructions,advanced_instructions:1000,guest:{instructions:n},console:consoleText,
  provenance:{experiment:'c27-profile-budget-continuation',input_checkpoint_sha256:SOURCE_C31.sha256,input_report_sha256:reportSha,snapshot_round_trip_verified:true,original_state_modified:false}};
 const result={checkpoint_sha256:sha,instructions:n,execution_progress_accepted:true,guest_boot_failure:false,guest_reboot_requested:false,guest_kernel_panic:false,guest_terminal_failure:false};
 const host={status:'process_completed',exit_code:0,trigger:null,forced_kill:false};
 const boundary={checkpoint_sha256:sha,instructions:n,clean_completed:true};
 const tasks=[{pid:1,task:'0xffff01',name:'init',state:0,exit_state:0,has_user_mm:true}];
 const kernel={status:'read_only_kernel_observation',checkpoint_sha256:sha,input_unchanged:true,instructions:n,tasks,task_count:1,current_task_details:{pid:1,task:'0xffff01',name:'init',verification:'process_list'},current_task:'0xffff01',current_pid:1};
 const frame={source_checkpoint_sha256:sha,cpu_instructions_executed:0,original_snapshot_unchanged:true,status:'no_scanout'};
 const framework={checkpoint_sha256:sha,instructions:n,framework_runtime_verified:false,sys_boot_completed_live:false,scanout_exported:false,ready_for_visual_review:false,android_display_content_verified:false,reasons:['live_process_missing:zygote64']};
 return {sourceContract,sourceReport,sourceBoundary:{schema:'reverse-c27-sequence-boundary-v1',checkpoint_sha256:SOURCE_C31.sha256,instructions:SOURCE_C31.instructions,clean_completed:true},report,result,host,boundary,kernel,frame,framework,sourceAuthenticated:true,outputAuthenticated:true};
}
test('a launch line never equals process completion',()=>{
 const r=evaluateC32(fixture("init: Command 'exec_start derive_classpath' action=post-fs-data took 95ms and succeeded\n"));
 assert.equal(r.stage,'post_fs_data_continuing');assert.equal(r.derive_classpath_service_exit_zero_verified,false);
 assert.equal(r.uart_scope,'current_authenticated_continuation');
});
test('new native UART segment proves actual classpath completion despite missing old console prefix',()=>{
 const r=evaluateC32(fixture("[ 218.9] init: Service 'derive_classpath' (pid 552) exited with status 0 waiting took 1.38 seconds\r\n"));
 assert.equal(r.derive_classpath_service_exit_zero_verified,true);
 assert.equal(r.stage,'derive_classpath_completed');
});
test('ART and odsign next-stage evidence requires process exit plus live process',()=>{
 const x=fixture("[ 218.9] init: Service 'derive_classpath' (pid 552) exited with status 0\r\n[ 220.0] init: Service 'art_boot' (pid 553) exited with status 0\r\n");
 x.kernel.tasks.push({pid:554,task:'0xffff554',name:'odsign',state:0,exit_state:0,has_user_mm:true});x.kernel.task_count=2;
 const r=evaluateC32(x);
 assert.equal(r.derive_classpath_service_exit_zero_verified,true);
 assert.equal(r.art_boot_service_exit_zero_verified,true);
 assert.equal(r.odsign_live_userspace_verified,true);
 assert.equal(r.stage,'odsign_running_after_art_boot');
 assert.equal(r.last_new_console_lines.length,2,'UART CRLF must produce two independent new log lines');
});
test('nonzero exit remains failure and cannot be confused with zero',()=>{
 const r=evaluateC32(fixture("init: Service 'derive_classpath' (pid 552) exited with status 3\n"));
 assert.equal(r.derive_classpath_service_exit_zero_verified,false);
 assert.equal(r.derive_classpath_service_nonzero_exit_observed,true);
 assert.equal(r.stage,'post_fs_data_service_failed');
});
test('process absent, exited or kernel-only cannot prove odsign is live',()=>{
 for(const modifier of [{exit_state:32},{has_user_mm:false},{state:128}]){
  const x=fixture("init: Service 'art_boot' (pid 553) exited with status 0\n");
  x.kernel.tasks.push({pid:554,task:'0xffff554',name:'odsign',state:0,exit_state:0,has_user_mm:true,...modifier});
  assert.equal(evaluateC32(x).odsign_live_userspace_verified,false);
 }
});
test('unrelated live framework process is recorded but not treated as complete framework',()=>{
 const x=fixture();x.kernel.tasks.push({pid:23,task:'0xffff23',name:'zygote64',state:8193,exit_state:0,has_user_mm:true});x.kernel.task_count=2;
 const r=evaluateC32(x);assert.deepEqual(r.live_framework_processes,['zygote64']);
 assert.equal(r.live_framework_verified,false);
 assert.equal(r.stage,'framework_processes_observed');
});
for(const [name,mutate] of [
 ['no source full preflight',x=>x.sourceAuthenticated=false],
 ['no output full preflight',x=>x.outputAuthenticated=false],
 ['foreign source',x=>x.sourceContract.source_commit='0'.repeat(40)],
 ['wrong source report pin',x=>x.sourceContract.report_sha256='z'],
 ['mismatched report bytes',x=>x.report.provenance.input_report_sha256='f'.repeat(64)],
 ['crossed guest source',x=>x.report.provenance.input_checkpoint_sha256='f'.repeat(64)],
 ['no native snapshot roundtrip',x=>x.report.provenance.snapshot_round_trip_verified=false],
 ['guest state changed before resume',x=>x.report.provenance.original_state_modified=true],
 ['source never completed',x=>x.sourceBoundary.clean_completed=false],
 ['report instructions mismatch',x=>x.report.guest.instructions++],
 ['output boundary incomplete',x=>x.boundary.clean_completed=false],
 ['output not accepted',x=>x.result.execution_progress_accepted=false],
 ['reboot requested',x=>x.result.guest_reboot_requested=true],
 ['incorrect native process status',x=>x.host.status='process_failed'],
 ['injected observer checksum',x=>x.kernel.checkpoint_sha256='1'.repeat(64)],
 ['altered frame',x=>x.frame.cpu_instructions_executed=12],
 ['crossed framework evidence',x=>x.framework.checkpoint_sha256='1'.repeat(64)]
])test('reject '+name,()=>{const x=fixture();mutate(x);assert.throws(()=>evaluateC32(x));});
