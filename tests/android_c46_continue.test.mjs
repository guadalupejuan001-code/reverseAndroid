import test from 'node:test';import assert from 'node:assert/strict';
import {milestone} from '../tools/android_c46_continue.mjs';
const sha='a'.repeat(64);
function fixture(){return [
 {checkpoint_sha256:sha,instructions:25200742732,read_only:true,input_unchanged:true,system_server_alive_verified:true},
 {checkpoint_sha256:sha,instructions:25200742732,original_snapshot_unchanged:true,framework_runtime_verified:true,sys_boot_completed_live:true,selinux_enforcing_live:true,reasons:[],scanout_exported:false},
 {checkpoint_sha256:sha,instructions:25200742732,clean_completed:true},
 {checkpoint_sha256:sha}];}
test('completed framework and an authenticated clean source prove boot, not a picture',()=>{
 const r=milestone(...fixture());assert.equal(r.android_boot_completed_verified,true);assert.equal(r.ready_for_visual_review,false);assert.equal(r.android_display_content_verified,false);
});
for(const field of ['framework_runtime_verified','sys_boot_completed_live','selinux_enforcing_live'])test('reject missing boot proof: '+field,()=>{
 const f=fixture();f[1][field]=false;assert.equal(milestone(...f).android_boot_completed_verified,false);
});
test('a running system_server alone never proves completed boot',()=>{
 const f=fixture();f[1].reasons=['live_sys_boot_completed_unverified'];assert.equal(milestone(...f).android_boot_completed_verified,false);
 f[1].reasons=[];f[0].system_server_alive_verified=false;assert.equal(milestone(...f).android_boot_completed_verified,false);
});
test('crossed source, counter, interrupted execution and modified observations are rejected',()=>{
 for(const index of [0,1,2]){const f=fixture();f[index].checkpoint_sha256='b'.repeat(64);assert.throws(()=>milestone(...f));}
 const f=fixture();f[1].instructions--;assert.throws(()=>milestone(...f));
 const g=fixture();g[2].clean_completed=false;assert.throws(()=>milestone(...g));
 const h=fixture();h[1].original_snapshot_unchanged=false;assert.throws(()=>milestone(...h));
});
test('verified scanout requests visual review without inventing verified display content',()=>{
 const f=fixture();f[1].scanout_exported=true;const r=milestone(...f);assert.equal(r.ready_for_visual_review,true);assert.equal(r.android_display_content_verified,false);
});
