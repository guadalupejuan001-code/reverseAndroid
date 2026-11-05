import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';

// The campaign controls input only. Original native execution and admission
// remain responsible for every CPU transition and complete checkpoint.
export function validateCampaignInput(plan,stage,environment,readInput=readFileSync){
  assert.ok(Number.isInteger(stage)&&stage>=1);
  assert.ok(plan.input_file===null||typeof plan.input_file==='string'&&plan.input_file.length>0,'Declare an authenticated input file or explicit null');
  if(plan.input_file===null){
    assert.equal(plan.input_bytes,0);assert.equal(plan.input_sha256,null);
  }
  if(stage!==1||plan.input_file===null){
    assert.equal(environment,undefined,'This continuation must not inject or repeat UART input');
    return false;
  }
  assert.equal(typeof environment,'string','The declared input event is missing');
  assert.equal(resolve(environment),resolve(plan.input_file));
  const input=readInput(plan.input_file);
  assert.ok(input.length>0&&input.length<=4096);
  assert.equal(input.length,plan.input_bytes);
  assert.equal(createHash('sha256').update(input).digest('hex'),plan.input_sha256);
  return true;
}

export function verifyCampaignInputEvent(plan,stage,event){
  if(stage===1&&plan.input_file!==null){
    assert.ok(event,'Native provenance did not record the declared input');
    assert.equal(event.instructions,plan.source_instructions);
    assert.equal(event.bytes,plan.input_bytes);assert.equal(event.sha256,plan.input_sha256);
  }else assert.equal(event,undefined,'Native provenance recorded an unexpected input event');
}
