// Captured process identity is a read-only input; these policy tests do not boot Android.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {verifiedMainZygoteProcesses,evaluateFrameworkEvidence} from '../tools/android_framework_evidence.mjs';
import {classifyZygoteStage} from '../tools/android_c40_zygote_stage.mjs';
import {summarizeZygoteProgress} from '../tools/android_c42_zygote_progress.mjs';
const capture=JSON.parse(readFileSync(new URL('./fixtures/c62-zygote-main-observation.json',import.meta.url)));
const fixture=()=>structuredClone(capture.kernel);
const process=k=>k.zygote_launch.processes.find(p=>p.pid===567);
const task=k=>k.tasks.find(p=>p.pid===567);
const property=(k,name)=>k.live_boot_property.boot_chain_property_observations.find(o=>o.name===name);
const pid=k=>property(k,'init.svc_debug_pid.zygote');
const running=k=>property(k,'init.svc.zygote');
test('actual captured main task has live original Zygote argv, executable, parent and PID1 service PID',()=>{
 assert.equal(capture.fixture_only,true);assert.equal(capture.full_android_boot_attested,false);
 const k=fixture(),before=JSON.stringify(k),found=verifiedMainZygoteProcesses(k);assert.equal(found.length,1);assert.equal(found[0].task.name,'main');assert.equal(found[0].process.argv[0],'zygote64');assert.equal(found[0].task.pid,567);
 assert.equal(classifyZygoteStage(k).stage,'zygote_main_thread_observed');
 const progress=summarizeZygoteProgress(k);assert.equal(progress.zygote_pid,567);assert.equal(progress.zygote_comm,'main');assert.equal(progress.progress_status,'zygote_sleeping');assert.equal(progress.system_server_alive_verified,false);assert.equal(progress.android_boot_completed_verified,false);assert.equal(JSON.stringify(k),before);
});
const mutations={
 'wrong executable':k=>process(k).exe_path='/system/bin/sh',
 'different argv':k=>process(k).argv=['not_a_zygote'],
 'generic app_process argv':k=>process(k).argv=['/system/bin/app_process64','SomeApplication'],
 'extra arguments after renamed argv':k=>process(k).argv=['zygote64','impostor'],
 'argv unverified':k=>process(k).argv_verified=false,
 'wrong parent':k=>process(k).parent_pid=2,
 'wrong process PID':k=>process(k).pid=568,
 'wrong process name':k=>process(k).comm='other',
 'process observation not read-only':k=>process(k).read_only=false,
 'missing task':k=>k.tasks=k.tasks.filter(t=>t.pid!==567),
 'task name mismatch':k=>task(k).name='other',
 'task state mismatch':k=>task(k).state=0,
 'task is zombie':k=>{task(k).exit_state=16;process(k).exit_state=16;},
 'task has no userspace mm':k=>{task(k).has_user_mm=false;process(k).has_user_mm=false;},
 'task is stopped':k=>{task(k).state=4;process(k).state=4;},
 'duplicate process evidence':k=>k.zygote_launch.processes.push(structuredClone(process(k))),
 'duplicate task evidence':k=>k.tasks.push(structuredClone(task(k))),
 'argv from another checkpoint':k=>k.zygote_launch.source_checkpoint_sha256='0'.repeat(64),
 'argv observation mutable':k=>k.zygote_launch.read_only=false,
 'argv schema mismatch':k=>k.zygote_launch.schema='other',
 'checkpoint not attested unchanged':k=>k.input_unchanged=false,
 'observer status invalid':k=>k.status='unverified',
 'PID1 property mappings absent':k=>k.live_boot_property.property_mappings=0,
 'PID1 mm invalid':k=>k.live_boot_property.mm='0x1',
 'PID1 VMA walk invalid':k=>k.live_boot_property.vma_count=0,
 'PID1 VMA walk outside budget':k=>k.live_boot_property.vma_count=8193,
 'PID1 property mapping count impossible':k=>k.live_boot_property.property_mappings=k.live_boot_property.vma_count+1,
 'PID1 mapping walk error':k=>k.live_boot_property.read_error='invalid walk',
 'property PID is not init':k=>k.live_boot_property.pid=2,
 'property task is not current init':k=>k.live_boot_property.task='0xffff1234',
 'property observation mutable':k=>k.live_boot_property.read_only=false,
 'init task absent':k=>k.tasks=k.tasks.filter(t=>t.pid!==1),
 'init task is not alive':k=>k.tasks.find(t=>t.pid===1).exit_state=16,
 'service not running':k=>{running(k).value='stopped';running(k).serial=7<<24;},
 'PID1 service PID mismatches':k=>pid(k).value='568',
 'PID1 service PID has leading zeros':k=>{pid(k).value='0567';pid(k).serial=4<<24;},
 'PID1 service PID not canonical':k=>{pid(k).value='+567';pid(k).serial=4<<24;},
 'PID1 service PID observation absent':k=>k.live_boot_property.boot_chain_property_observations=k.live_boot_property.boot_chain_property_observations.filter(o=>o.name!=='init.svc_debug_pid.zygote'),
 'PID1 service PID unstable':k=>pid(k).serial|=1,
 'PID1 service PID long-value flag':k=>pid(k).serial|=0x10000,
 'PID1 service PID wrong byte length':k=>pid(k).serial=1<<24,
 'PID1 service PID private mapping unverified':k=>pid(k).live_pid1_mapping_verified=false,
 'PID1 service PID not shared':k=>pid(k).vma_shared=false,
 'PID1 service PID wrong file context':k=>pid(k).file='/tmp/pretend-property',
 'PID1 service PID conflicting values':k=>{const o=structuredClone(pid(k));o.value='568';k.live_boot_property.boot_chain_property_observations.push(o);},
};
for(const [name,mutate] of Object.entries(mutations))test('reject main identity: '+name,()=>{const k=fixture();mutate(k);assert.deepEqual(verifiedMainZygoteProcesses(k),[]);});
test('system_server must still have the verified live Zygote as parent',()=>{
 const k=fixture();k.tasks.push({pid:1001,name:'system_server',state:0,exit_state:0,has_user_mm:true});k.zygote_launch.processes.push({read_only:true,pid:1001,comm:'system_server',state:0,exit_state:0,has_user_mm:true,exe_path:'/system/bin/app_process64',argv:['system_server'],argv_verified:true,parent_pid:567});
 assert.equal(summarizeZygoteProgress(k).system_server_alive_verified,true);k.zygote_launch.processes.at(-1).parent_pid=999;assert.equal(summarizeZygoteProgress(k).system_server_alive_verified,false);pid(k).value='568';assert.equal(summarizeZygoteProgress(k).zygote_pid,null);
});
test('recognizing main does not attest completed Android boot or a display',()=>{
 const kernel=fixture(),checkpointSha=kernel.checkpoint_sha256,report={checkpoint_sha256:checkpointSha,error:'',stop_reason:'instruction_budget',console:'',guest:{instructions:kernel.instructions,pc:kernel.pc,virtual_ticks:Number(kernel.ticks),exceptions:Number(kernel.exceptions),halted:false,waiting:false}},result={checkpoint_sha256:checkpointSha,instructions:kernel.instructions,execution_progress_accepted:true,guest_kernel_panic:false,guest_boot_failure:false,guest_terminal_failure:false,guest_reboot_requested:false},host={status:'process_completed',exit_code:0,trigger:null,forced_kill:false,checkpoint_bytes_modified_by_supervisor:false},frame={schema:'reverse-snapshot-scanout-v1',source_checkpoint_sha256:checkpointSha,cpu_instructions_executed:0,original_snapshot_unchanged:true,status:'no_scanout'};
 const evidence=evaluateFrameworkEvidence({checkpointSha,report,result,host,kernel,frame});assert.equal(evidence.processes.find(p=>p.name==='zygote64').verified,true);assert.equal(evidence.processes.find(p=>p.name==='zygote64').observed_comm,'main');assert.equal(evidence.framework_runtime_verified,false);assert.equal(evidence.sys_boot_completed_live,false);assert.equal(evidence.ready_for_visual_review,false);assert.equal(evidence.android_display_content_verified,false);assert.ok(evidence.reasons.includes('live_process_missing:system_server'));
});
