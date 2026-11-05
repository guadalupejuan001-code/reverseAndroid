import test from 'node:test';import assert from 'node:assert/strict';
import {mkdtempSync,writeFileSync,mkdirSync,existsSync,rmSync} from 'node:fs';import {tmpdir} from 'node:os';import {join} from 'node:path';
import {validatePinOptions,pinCheckpoint} from '../tools/android_pin_checkpoint.mjs';
const options={inputSha:'1'.repeat(64),experiment:'profile-budget-continuation',sourceCommit:'2'.repeat(40),minimumInstructions:2};
test('validates finite provenance and instruction boundaries',()=>{
 assert.deepEqual(validatePinOptions(options),options);
 for(const [key,value] of [['inputSha','../state'],['experiment','../experiment'],['sourceCommit','head'],['minimumInstructions',0],['minimumInstructions',Infinity],['minimumInstructions',2**53]])assert.throws(()=>validatePinOptions({...options,[key]:value}));
});
test('missing or unchanged generation does not create a misleading output',()=>{
 const dir=mkdtempSync(join(tmpdir(),'reverse-pin-'));try{
  const output=join(dir,'output');assert.equal(pinCheckpoint(dir,output,options).status,'not_ready');assert.equal(existsSync(output),false);
  mkdirSync(join(dir,'generation-1'));writeFileSync(join(dir,'latest.json'),JSON.stringify({generation:'generation-1',checkpoint_sha256:'3'.repeat(64)}));
  writeFileSync(join(dir,'generation-1/report.json'),JSON.stringify({guest:{instructions:1},error:'',provenance:{input_checkpoint_sha256:options.inputSha,experiment:options.experiment,original_state_modified:false,ram_profile_changed:false,snapshot_round_trip_verified:true}}));
  assert.equal(pinCheckpoint(dir,output,options).status,'not_ready');assert.equal(existsSync(output),false);
 }finally{rmSync(dir,{recursive:true,force:true});}
});
test('rejects unsafe or malformed generation pointers before opening source files',()=>{
 const dir=mkdtempSync(join(tmpdir(),'reverse-pin-'));try{for(const generation of ['../outside','generation-0','generation-1/../other','/absolute','generation-01']){writeFileSync(join(dir,'latest.json'),JSON.stringify({generation,checkpoint_sha256:'3'.repeat(64)}));assert.throws(()=>pinCheckpoint(dir,join(dir,'output'),options));}}finally{rmSync(dir,{recursive:true,force:true});}
});
test('preserves an existing destination',()=>{
 const dir=mkdtempSync(join(tmpdir(),'reverse-pin-'));try{writeFileSync(join(dir,'owned.txt'),'keep');assert.throws(()=>pinCheckpoint(dir,join(dir,'owned.txt'),options));assert.equal(existsSync(join(dir,'owned.txt')),true);}finally{rmSync(dir,{recursive:true,force:true});}
});
