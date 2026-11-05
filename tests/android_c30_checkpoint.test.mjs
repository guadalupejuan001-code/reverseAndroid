// Host-policy fixtures. The test inputs are not evidence of an Android boot.
import test from 'node:test';
import assert from 'node:assert/strict';
import {C29_FINAL,C29_ROUND_COUNTS,diagnoseCompletedC29} from '../tools/android_c30_checkpoint.mjs';
function fixture(){
 const sha=C29_FINAL.checkpoint_sha256,instructions=C29_FINAL.instructions;
 const report={checkpoint_sha256:sha,error:'',stop_reason:'host_wall_budget',console:'',guest:{
  instructions,pc:'0xabc0',virtual_ticks:22,exceptions:4,halted:false,waiting:false,display_frame:0,display_width:0,display_height:0
 }};
 const result={checkpoint_sha256:sha,instructions,execution_progress_accepted:true,guest_kernel_panic:false,guest_terminal_failure:false,guest_boot_failure:false,guest_reboot_requested:false};
 const host={status:'process_completed',exit_code:0,trigger:null,forced_kill:false,checkpoint_bytes_modified_by_supervisor:false};
 const task=(pid,name)=>({pid,name,state:1,exit_state:0,has_user_mm:true,task:'0xffff'+pid});
 const tasks=[task(1,'init'),task(396,'keystore2'),task(400,'mke2fs')];
 const kernel={status:'read_only_kernel_observation',checkpoint_sha256:sha,
  kernel_sha256:'f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8',
  input_unchanged:true,instructions,pc:'0xabc0',ticks:'22',exceptions:'4',pc_symbol:'crypto_aes_encrypt+0xc',
  tasks,current_task:tasks[2].task,current_pid:400,current_task_details:{pid:400,name:'mke2fs',task:tasks[2].task,verification:'process_list'},
  live_kernel_security:{read_only:true,btf_layout_verified:true,selinux_initialized:true,selinux_enforcing:true,policy_present:true},
  live_boot_property:{schema:'reverse-live-pid1-property-v1',read_only:true,pid:1,live_property_mapping_verified:false,boot_completed_property_live:false,ambiguous_property_values:false,observations:[]}
 };
 const frame={schema:'reverse-snapshot-scanout-v1',status:'no_scanout',source_checkpoint_sha256:sha,cpu_instructions_executed:0,original_snapshot_unchanged:true};
 return {contract:{source_commit:C29_FINAL.source_commit,checkpoint_sha256:sha},report,result,host,
 boundary:{schema:'reverse-c27-sequence-boundary-v1',clean_completed:true,checkpoint_sha256:sha,instructions},
 kernel,frame,authenticated:true};
}
test('real C29 final pins and four ordered instruction counts are preserved',()=>{
 const x=fixture(),r=diagnoseCompletedC29(x);
 assert.deepEqual(r.c29_clean_round_instruction_counts,C29_ROUND_COUNTS);
 assert.equal(r.instructions,13615798839);
 assert.equal(r.cpu_instructions_since_c28,8719150000);
 assert.equal(r.source_checkpoint_unchanged,true);
});
test('an executing formatter is only a stage observation; not filesystem completion',()=>{
 const r=diagnoseCompletedC29(fixture());
 assert.equal(r.stage,'userdata_formatter_executing');
 assert.deepEqual(r.current_task,{pid:400,name:'mke2fs',source:'process_list'});
 assert.equal(r.filesystem_formatting_completion_verified,false);
 assert.equal(r.live_framework_verified,false);
 assert.equal(r.scanout_exported,false);
 assert.equal(r.android_display_content_verified,false);
 assert.equal(r.ready_for_visual_review,false);
});
test('uncertain current task does not fake an executing process',()=>{
 const x=fixture();x.kernel.current_task_details.verification=null;
 const r=diagnoseCompletedC29(x);assert.equal(r.current_task,null);assert.equal(r.stage,'userdata_formatter_present');
});
test('observed background keystore is not a completed framework',()=>{
 const x=fixture();x.kernel.tasks.pop();x.kernel.current_task_details=null;x.kernel.current_task=null;x.kernel.current_pid=null;
 const r=diagnoseCompletedC29(x);assert.equal(r.stage,'userspace_services_observed');assert.equal(r.live_framework_verified,false);
});
test('missing both observers is explicitly incomplete, not a boot result',()=>{
 const x=fixture();x.kernel=null;x.frame=null;
 const r=diagnoseCompletedC29(x);assert.equal(r.stage,'observation_unavailable');
 assert.equal(r.read_only_observation_available,false);assert.equal(r.live_boot_completed_verified,false);
});
for (const [name,mutate] of [
 ['no authenticated preflight',x=>x.authenticated=false],
 ['wrong source commit',x=>x.contract.source_commit='0'.repeat(40)],
 ['replaced checkpoint',x=>x.contract.checkpoint_sha256='0'.repeat(64)],
 ['wrong instruction count',x=>x.report.guest.instructions++],
 ['changed result',x=>x.result.checkpoint_sha256='0'.repeat(64)],
 ['unfinished boundary',x=>x.boundary.clean_completed=false],
 ['wrong boundary',x=>x.boundary.instructions++],
 ['native failure',x=>x.host.exit_code=2],
 ['unaccepted CPU budget',x=>x.result.execution_progress_accepted=false],
 ['mixed observer SHA',x=>x.kernel.checkpoint_sha256='1'.repeat(64)],
 ['mixed observer PC',x=>x.kernel.pc='0xdead'],
 ['frame from another checkpoint',x=>x.frame.source_checkpoint_sha256='1'.repeat(64)],
 ['partial observer evidence',x=>x.frame=null]
])test('rejects '+name,()=>{
 const x=fixture();mutate(x);assert.throws(()=>diagnoseCompletedC29(x));
});
