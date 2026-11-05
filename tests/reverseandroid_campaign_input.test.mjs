import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {validateCampaignInput,verifyCampaignInputEvent} from '../tools/reverseandroid-campaign-input.mjs';

const none={input_file:null,input_bytes:0,input_sha256:null,source_instructions:100};
const bytes=Buffer.from('recorded input\n');
const declared={input_file:'tools/inputs/test.bin',input_bytes:bytes.length,input_sha256:createHash('sha256').update(bytes).digest('hex'),source_instructions:100};
const event={instructions:100,bytes:declared.input_bytes,sha256:declared.input_sha256};

test('a continuation with no new input never reads a file and rejects injected UART',()=>{
  const noRead=()=>{throw Error('No file should be read');};
  assert.equal(validateCampaignInput(none,1,undefined,noRead),false);
  assert.equal(validateCampaignInput(none,3,undefined,noRead),false);
  assert.throws(()=>validateCampaignInput(none,1,'unplanned.bin',noRead));
  verifyCampaignInputEvent(none,1,undefined);
  assert.throws(()=>verifyCampaignInputEvent(none,1,event));
});
test('the first explicit input must match the frozen path, bytes and native instruction boundary',()=>{
  assert.equal(validateCampaignInput(declared,1,declared.input_file,()=>bytes),true);
  assert.throws(()=>validateCampaignInput(declared,1,undefined,()=>bytes));
  assert.throws(()=>validateCampaignInput(declared,1,declared.input_file,()=>Buffer.from('changed input\n')));
  verifyCampaignInputEvent(declared,1,event);
  assert.throws(()=>verifyCampaignInputEvent(declared,1,{...event,instructions:101}));
});
test('a later stage cannot repeat an otherwise valid authenticated command',()=>{
  assert.equal(validateCampaignInput(declared,2,undefined),false);
  assert.throws(()=>validateCampaignInput(declared,2,declared.input_file));
  verifyCampaignInputEvent(declared,2,undefined);
  assert.throws(()=>verifyCampaignInputEvent(declared,2,event));
});
test('a missing or contradictory input declaration cannot silently mean no input',()=>{
  assert.throws(()=>validateCampaignInput({},1,undefined));
  assert.throws(()=>validateCampaignInput({...none,input_bytes:1},1,undefined));
  assert.throws(()=>validateCampaignInput({...none,input_sha256:declared.input_sha256},1,undefined));
});
