import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,writeFileSync as write,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {loadEpochInputPlan,verifyEpochInputEvent} from '../tools/reverseandroid-epoch-input.mjs';
import {hash} from '../tools/reverseandroid-lights-profile.mjs';
const contract={checkpoint_sha256:'a'.repeat(64),instructions:123456},frame={status:'scanout_exported',ppm_sha256:'b'.repeat(64)};
function fixture(fn){const root=mkdtempSync(join(tmpdir(),'epoch-input-'));try{
  const data=Buffer.from('\n/system/bin/input keyevent 3\n');write(join(root,'input.bin'),data);
  const plan={schema:'reverseandroid-authenticated-console-input-plan-v1',purpose:'ui_response_test',source_checkpoint_sha256:contract.checkpoint_sha256,
    source_instructions:contract.instructions,input_file:'input.bin',input_bytes:data.length,input_sha256:hash(data),before_frame_ppm_sha256:frame.ppm_sha256};
  const path=join(root,'plan.json'),save=()=>write(path,JSON.stringify(plan));save();return fn({root,path,plan,data,save});
}finally{rmSync(root,{recursive:true,force:true});}}
test('no plan means no new input; an unrequested native event is rejected',()=>{
  assert.equal(loadEpochInputPlan(null,contract),null);verifyEpochInputEvent(undefined,null,{});
  assert.throws(()=>verifyEpochInputEvent({schema:'reverse-modeled-uart-input-v1'},null,{}));
});
test('bind actual UI input bytes to the completed-boot snapshot and before frame',()=>fixture(({path,data,plan})=>{
  const input=loadEpochInputPlan(path,contract,{bootVerified:true,frame});assert.ok(input.data.equals(data));
  verifyEpochInputEvent({schema:'reverse-modeled-uart-input-v1',instructions:contract.instructions,counter_subtick:17,bytes:data.length,sha256:plan.input_sha256},input,{instructions:contract.instructions,counter_subtick:17});
  for(const patch of [{instructions:1},{counter_subtick:18},{sha256:'c'.repeat(64)},{bytes:1}])assert.throws(()=>verifyEpochInputEvent({schema:'reverse-modeled-uart-input-v1',instructions:contract.instructions,counter_subtick:17,bytes:data.length,sha256:plan.input_sha256,...patch},input,{instructions:contract.instructions,counter_subtick:17}));
}));
test('UI input cannot substitute for actual boot or an authenticated frame',()=>fixture(({path})=>{
  assert.throws(()=>loadEpochInputPlan(path,contract,{bootVerified:false,frame}),/completed-boot/);
  assert.throws(()=>loadEpochInputPlan(path,contract,{bootVerified:true,frame:null}),/authenticated before frame/);
  assert.throws(()=>loadEpochInputPlan(path,contract,{bootVerified:true,frame:{...frame,ppm_sha256:'c'.repeat(64)}}),/different before frame/);
}));
test('reject crossed snapshots, boundaries and changed file bytes',()=>fixture(({root,path,plan,save})=>{
  assert.throws(()=>loadEpochInputPlan(path,{...contract,checkpoint_sha256:'c'.repeat(64)},{bootVerified:true,frame}),/different snapshot/);
  assert.throws(()=>loadEpochInputPlan(path,{...contract,instructions:123457},{bootVerified:true,frame}),/different instruction/);
  write(join(root,'input.bin'),Buffer.alloc(plan.input_bytes,0));assert.throws(()=>loadEpochInputPlan(path,contract,{bootVerified:true,frame}),/bytes changed/);
  plan.input_bytes=4097;save();assert.throws(()=>loadEpochInputPlan(path,contract,{bootVerified:true,frame}));
}));
test('diagnostic console events remain explicit and require the same source and byte pins',()=>fixture(({path,plan,save})=>{
  plan.purpose='diagnostic_console';delete plan.before_frame_ppm_sha256;save();assert.ok(loadEpochInputPlan(path,contract));
  plan.purpose='implicit';save();assert.throws(()=>loadEpochInputPlan(path,contract),/Unspecified input/);
}));
