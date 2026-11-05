// Explicit input authorization for the existing modeled PL011 queue.
import assert from 'node:assert/strict';
import {readFileSync as read,statSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {hash} from './reverseandroid-lights-profile.mjs';
export function loadEpochInputPlan(file,contract,{bootVerified=false,frame=null}={}){
  if(!file)return null;
  assert.ok(statSync(file).isFile()&&statSync(file).size<=16384,'Oversized input plan');
  const plan=JSON.parse(read(file,'utf8'));
  assert.equal(plan.schema,'reverseandroid-authenticated-console-input-plan-v1');
  assert.ok(['ui_response_test','diagnostic_console'].includes(plan.purpose),'Unspecified input purpose');
  assert.equal(plan.source_checkpoint_sha256,contract.checkpoint_sha256,'Input belongs to a different snapshot');
  assert.equal(plan.source_instructions,contract.instructions,'Input belongs to a different instruction boundary');
  assert.ok(typeof plan.input_file==='string'&&plan.input_file.length>0&&plan.input_file.length<=4096);
  assert.match(plan.input_sha256,/^[0-9a-f]{64}$/);
  assert.ok(Number.isSafeInteger(plan.input_bytes)&&plan.input_bytes>0&&plan.input_bytes<=4096);
  const path=resolve(dirname(resolve(file)),plan.input_file),info=statSync(path);
  assert.ok(info.isFile()&&info.size===plan.input_bytes&&info.size<=4096,'Input byte extent mismatch');
  const data=read(path);assert.equal(hash(data),plan.input_sha256,'Input bytes changed');
  if(plan.purpose==='ui_response_test'){
    assert.equal(bootVerified,true,'A UI input test requires original completed-boot evidence');
    assert.equal(frame?.status,'scanout_exported','A UI input test requires an authenticated before frame');
    assert.equal(plan.before_frame_ppm_sha256,frame.ppm_sha256,'UI input belongs to a different before frame');
  }
  return {plan,path,data};
}
export function verifyEpochInputEvent(event,input,sourceGuest){
  if(!input){assert.equal(event,undefined,'Unrequested UART input');return;}
  assert.equal(event?.schema,'reverse-modeled-uart-input-v1');
  assert.equal(event.instructions,sourceGuest.instructions);assert.equal(event.counter_subtick,sourceGuest.counter_subtick);
  assert.equal(event.bytes,input.data.length);assert.equal(event.sha256,input.plan.input_sha256);
}
