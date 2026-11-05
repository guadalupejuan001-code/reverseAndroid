import test from 'node:test';import assert from 'node:assert/strict';
import {summarizeZygoteProgress,verifyProgressBinding} from '../tools/android_c42_zygote_progress.mjs';
const sha='a'.repeat(64);
const task=(pid,name,state=0)=>({pid,name,state,exit_state:0,has_user_mm:true,
 accounting:{sum_exec_runtime:'1500000000'},stack:[{symbol:'__schedule+0x10'}]});
const proc=(pid,comm,parent=1)=>({pid,comm,parent_pid:parent,state:0,exit_state:0,has_user_mm:true,
 argv_verified:true,exe_path:'/system/bin/app_process64',launch_signature_verified:comm==='app_process64',argv:['/system/bin/app_process64','--zygote']});
const kernel=()=>({checkpoint_sha256:sha,input_unchanged:true,instructions:19316342731,
 tasks:[task(567,'app_process64')],zygote_launch:{schema:'reverse-c40-zygote-argv-v1',
 source_checkpoint_sha256:sha,read_only:true,processes:[proc(567,'app_process64')]}});
test('a running launcher is progress evidence without inventing system_server',()=>{
 const p=summarizeZygoteProgress(kernel());assert.equal(p.zygote_pid,567);
 assert.equal(p.progress_status,'zygote_runnable');assert.equal(p.system_server_alive_verified,false);
 assert.equal(p.zygote_socket_responsive_verified,false);
});
test('a renamed Zygote and live app_process child prove system_server lineage',()=>{
 const k=kernel();k.tasks=[task(567,'zygote64',1),task(700,'system_server',8193)];
 k.zygote_launch.processes=[proc(567,'zygote64'),{...proc(700,'system_server',567),state:8193}];
 const p=summarizeZygoteProgress(k);assert.equal(p.system_server_alive_verified,true);
 assert.equal(p.system_server_pid,700);assert.equal(p.system_server_parent_pid,567);
 assert.equal(p.android_boot_completed_verified,false);
});
for(const [label,change]of[
 ['zombie',k=>{k.tasks[1].exit_state=16;}],['stopped',k=>{k.tasks[1].state=4;}],
 ['kernel thread',k=>{k.tasks[1].has_user_mm=false;}],['wrong parent',k=>{k.zygote_launch.processes[1].parent_pid=1;}],
 ['unverified argv',k=>{k.zygote_launch.processes[1].argv_verified=false;}],
 ['wrong executable',k=>{k.zygote_launch.processes[1].exe_path='/system/bin/sh';}],
 ['duplicate',k=>{k.zygote_launch.processes.push({...k.zygote_launch.processes[1],pid:701});k.tasks.push(task(701,'system_server'));}]
])test('system_server rejected: '+label,()=>{
 const k=kernel();k.tasks.push(task(700,'system_server'));k.zygote_launch.processes.push(proc(700,'system_server',567));
 change(k);assert.equal(summarizeZygoteProgress(k).system_server_alive_verified,false);
});
test('a genuine I/O sleep and growing CPU accounting remain distinct from a deadlock claim',()=>{
 const k=kernel();k.tasks[0].state=258;k.zygote_launch.processes[0].state=258;
 k.tasks[0].stack=[{symbol:'io_schedule+0x30'},{symbol:'filemap_get_pages+0x208'}];
 const old={schema:'reverse-c42-zygote-progress-v1',checkpoint_sha256:'b'.repeat(64),instructions:k.instructions-1000000,zygote_pid:567,zygote_runtime_ns:'1000000000'};
 const p=summarizeZygoteProgress(k,old);assert.equal(p.progress_status,'zygote_waiting_for_file_io');
 assert.equal(p.zygote_runtime_delta_ns,'500000000');assert.equal(p.system_server_alive_verified,false);
});
test('a stopped launcher cannot confer a live parent on a purported server',()=>{
 const k=kernel();k.tasks[0].state=4;k.zygote_launch.processes[0].state=4;
 k.tasks.push(task(700,'system_server'));k.zygote_launch.processes.push(proc(700,'system_server',567));
 assert.equal(summarizeZygoteProgress(k).system_server_alive_verified,false);
});
test('no progress, crossed snapshot and regressing accounting are rejected',()=>{
 const k=kernel(),old=summarizeZygoteProgress(k);assert.throws(()=>summarizeZygoteProgress(k,old),/did not advance/);
 k.instructions++;old.zygote_runtime_ns='2000000000';assert.throws(()=>summarizeZygoteProgress(k,old),/accounting regressed/);
 k.zygote_launch.source_checkpoint_sha256='b'.repeat(64);assert.throws(()=>summarizeZygoteProgress(k));
});
test('progress must bind the exact contract, counter and preceding source',()=>{
 const p=summarizeZygoteProgress(kernel()),contract={checkpoint_sha256:sha},proof={report:{guest:{instructions:p.instructions},initial_instructions:p.instructions-1,provenance:{input_checkpoint_sha256:'b'.repeat(64)}}};
 const old={checkpoint_sha256:'b'.repeat(64),instructions:p.instructions-1};p.previous_checkpoint_sha256=old.checkpoint_sha256;
 assert.equal(verifyProgressBinding(p,contract,proof,old),true);
 assert.throws(()=>verifyProgressBinding(p,{checkpoint_sha256:'c'.repeat(64)},proof,old));
 proof.report.initial_instructions--;assert.throws(()=>verifyProgressBinding(p,contract,proof,old));
});
