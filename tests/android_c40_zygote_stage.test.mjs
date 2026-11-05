import test from 'node:test';
import assert from 'node:assert/strict';
import {classifyZygoteStage} from '../tools/android_c40_zygote_stage.mjs';
const sha='094d5c9b9532fbb21ea029031b952f46c4df0e589c2f0d75a4dd64677bc49438';
const args=['/system/bin/app_process64','-Xzygote','/system/bin','--zygote','--start-system-server','--socket-name=zygote'];
const launch={read_only:true,pid:567,comm:'app_process64',exe_path:'/system/bin/app_process64',parent_pid:1,
 argv:args,argv_verified:true,launch_signature_verified:true,state:0,exit_state:0,has_user_mm:true};
const base=()=>({checkpoint_sha256:sha,input_unchanged:true,instructions:18393448839,task_count:201,tasks:[],
 zygote_launch:{schema:'reverse-c40-zygote-argv-v1',read_only:true,source_checkpoint_sha256:sha,processes:[launch]},
 live_boot_property:{boot_chain_property_observations:[{name:'init.svc.zygote',status:'property_observed',value:'running'}]}});
test('live executable and exact argv prove launcher, not socket readiness',()=>{
 const s=classifyZygoteStage(base());
 assert.equal(s.stage,'zygote_launcher_executing');
 assert.equal(s.launcher_pid,567);
 assert.deepEqual(s.launcher_argv,args);
 assert.equal(s.init_svc_zygote,'running');
 assert.equal(s.zygote_exec_confirmed,true);
 assert.equal(s.zygote_socket_responsive_verified,false);
 assert.equal(s.android_boot_completed_verified,false);
});
test('no matching argv: service property never invents a Zygote executable',()=>{
 const k=base();k.zygote_launch.processes=[{...launch,argv_verified:false,launch_signature_verified:false}];
 const s=classifyZygoteStage(k);assert.equal(s.stage,'zygote_init_service_running_no_verified_argv');
 assert.equal(s.zygote_exec_confirmed,false);assert.equal(s.launcher_pid,null);
});
test('process that exited or has no mm cannot establish live launcher',()=>{
 for(const mod of [{exit_state:16},{has_user_mm:false},{state:65536}]){
  const k=base();k.zygote_launch.processes=[{...launch,...mod}];
  assert.equal(classifyZygoteStage(k).zygote_exec_confirmed,false);
 }
});
test('a renamed zygote process is distinct from a completed framework',()=>{
 const k=base();k.zygote_launch.processes=[{...launch,pid:568,comm:'zygote64',argv:['zygote64']}];
 const s=classifyZygoteStage(k);
 assert.equal(s.stage,'zygote_process_renamed');assert.equal(s.named_zygote_pid,568);
 assert.equal(s.system_server_pid,null);assert.equal(s.zygote_boot_completed_verified,false);
});
test('a real system_server task is tracked but is not equivalent to boot_completed',()=>{
 const k=base();k.tasks=[{pid:590,name:'system_server',exit_state:0,has_user_mm:true,state:0}];
 const s=classifyZygoteStage(k);
 assert.equal(s.stage,'system_server_process_observed');
 assert.equal(s.system_server_pid,590);assert.equal(s.android_boot_completed_verified,false);
});
test('missing hash, altered memory and mismatched argv source are fatal',()=>{
 const k=base();delete k.checkpoint_sha256;assert.throws(()=>classifyZygoteStage(k));
 const a=base();a.input_unchanged=false;assert.throws(()=>classifyZygoteStage(a));
 const b=base();b.zygote_launch.source_checkpoint_sha256='f'.repeat(64);
 assert.throws(()=>classifyZygoteStage(b));
});
test('conflicting init.svc.zygote observations cannot authorize a service status',()=>{
 const k=base();k.live_boot_property.boot_chain_property_observations.push({name:'init.svc.zygote',status:'property_observed',value:'stopped'});
 assert.equal(classifyZygoteStage(k).init_svc_zygote,null);
});
