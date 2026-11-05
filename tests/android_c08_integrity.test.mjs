// Corruptions and zero-step report fixtures are TEST DATA, not Android runs.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdtempSync,mkdirSync,linkSync,existsSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {C07_PIN as pin,hash} from '../tools/android_verified_c07_source.mjs';
import {bindC08Snapshot} from '../tools/android_c08_result.mjs';
import {fnv1a64} from '../tools/rvg_checksum.mjs';
assert.ok(process.env.C07_REPORT&&process.env.C07_STATE,'original C07 artifact paths required');
const state=readFileSync(process.env.C07_STATE),source=JSON.parse(readFileSync(process.env.C07_REPORT));
const header=360,imageBytes=Number(state.readBigUInt64LE(8+39*8));
const kernelBytes=Number(state.readBigUInt64LE(header+8)),dtbBytes=Number(state.readBigUInt64LE(header+16));
const initrdBytes=Number(state.readBigUInt64LE(header+24));
function corrupted(offset,{repairChecksum=false}={}){
  const b=Buffer.from(state);b[offset]^=1;
  if(repairChecksum)b.writeBigUInt64LE(fnv1a64(b,b.length-8),b.length-8);
  const p=structuredClone(source);p.checkpoint_sha256=hash(b);
  return ()=>bindC08Snapshot(b,p,{checkpoint_sha256:p.checkpoint_sha256});
}
test('optimized full-file checksum agrees with the original C07 trailer',()=>assert.equal(fnv1a64(state,state.length-8),state.readBigUInt64LE(state.length-8)));
for(const [name,offset] of [['checksum',state.length-1],['kernel',header+48+100],['RAM header',8]])
  test('rejects previously accepted corruption even with recomputed outer hashes: '+name,()=>assert.throws(corrupted(offset),/mismatch|differs/));
for(const [name,offset] of [
  ['kernel',header+48+100],['DTB',header+48+kernelBytes+16],
  ['ramdisk',header+48+kernelBytes+dtbBytes+50],
  ['bootargs',header+48+kernelBytes+dtbBytes+initrdBytes],
  ['disk identity',header+imageBytes-64],['logical RAM',8],
  ['halted state',64],['exit code',56]])
  test('recomputing ALL hashes still cannot change '+name,()=>assert.throws(corrupted(offset,{repairChecksum:true}),/mismatch|differs/));
test('false halted report is rejected without modifying the snapshot',()=>{
  const p=structuredClone(source);p.guest.halted=true;
  assert.throws(()=>bindC08Snapshot(state,p,{checkpoint_sha256:pin.checkpoint_sha256}),/native halt/);
});
function collectorFixture(t,{falseCapability=false}={}){
  const root=mkdtempSync(join(tmpdir(),'reverse-c08-collector-'));t.after(()=>rmSync(root,{recursive:true,force:true}));
  const dir=join(root,'journal','generation-1');mkdirSync(dir,{recursive:true});
  linkSync(resolve(process.env.C07_STATE),join(dir,'checkpoint.rvg'));
  const p=structuredClone(source);p.initial_instructions=pin.instructions;p.advanced_instructions=0;
  p.provenance.input_checkpoint_sha256=pin.checkpoint_sha256;p.stop_reason='running';p.console='';
  if(falseCapability)p.provenance.android_display_verified=true;
  writeFileSync(join(dir,'report.json'),JSON.stringify(p));
  writeFileSync(join(root,'journal','latest.json'),JSON.stringify({generation:'generation-1',checkpoint_sha256:pin.checkpoint_sha256}));
  const result=spawnSync(process.execPath,['tools/android_c08_result.mjs',root,resolve(process.env.C07_REPORT)],{cwd:resolve('.'),encoding:'utf8',timeout:20000});
  assert.equal(result.error,undefined,result.error?.message);return {root,result};
}
test('collector rejects a false capability before promoting current files',t=>{
  const {root,result}=collectorFixture(t,{falseCapability:true});assert.equal(result.status,1,result.stderr);
  assert.equal(existsSync(join(root,'current.rvg')),false);assert.equal(existsSync(join(root,'current.json')),false);
  assert.equal(existsSync(join(root,'journal','generation-1','checkpoint.rvg')),true);
});
test('collector preserves a verified zero-step fixture without claiming progress',t=>{
  const {root,result}=collectorFixture(t);assert.equal(result.status,2,result.stderr);
  assert.equal(existsSync(join(root,'current.rvg')),true);
  const milestone=JSON.parse(readFileSync(join(root,'milestone.json')));
  assert.equal(milestone.execution_progress_verified,false);assert.equal(milestone.advanced_instructions,0);
  assert.equal(milestone.android_display_verified,false);
});
