#!/usr/bin/env node
// Genuine checkpoint replay against the original native probe's diagnostics.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {observeAndroidKernel} from '../tools/android_kernel_observer.mjs';
const [path,nativePath,expectedSha,taskReference]=process.argv.slice(2);
assert.ok(path&&nativePath&&/^[a-f0-9]{64}$/.test(expectedSha??''),'usage: RVG native.json SHA256 [archived-tasks.json]');
const bytes=readFileSync(path),native=JSON.parse(readFileSync(nativePath,'utf8'));
const actual=observeAndroidKernel(bytes,expectedSha);
assert.equal(actual.checkpoint_sha256,expectedSha);
assert.equal(actual.instructions,native.guest.instructions);
assert.equal(actual.pc,native.guest.pc);
assert.equal(BigInt(actual.physical_pc),BigInt(native.next_instruction.physical_pc));
assert.equal(BigInt(actual.next_opcode),BigInt(native.next_instruction.opcode));
assert.equal(actual.ticks,String(native.guest.virtual_ticks));
assert.equal(actual.exceptions,String(native.guest.exceptions));
assert.equal(actual.input_unchanged,true);
assert.equal(actual.android_init_observed,false);
assert.equal(createHash('sha256').update(bytes).digest('hex'),expectedSha);
const corrupt=Buffer.from(bytes);corrupt[corrupt.length-9]^=1;
assert.throws(()=>observeAndroidKernel(corrupt,expectedSha),/checkpoint SHA256 pin mismatch/);
const wrongKernel=Buffer.from(bytes);wrongKernel[8+(bytes[3]===51?44:43)*8+40+1024]^=1;
assert.throws(()=>observeAndroidKernel(wrongKernel),/kernel SHA256 pin mismatch/);
let archivedTasks=0,archivedStacks=0,irqFrames=0;
if(taskReference){
 const old=JSON.parse(readFileSync(taskReference,'utf8'));
 assert.equal(actual.task_count,old.length);
 for(const previous of old){
  const current=actual.tasks.find(t=>t.pid===previous.pid);
  assert.ok(current);
  assert.equal(current.name,previous.name);
  assert.equal(current.state,previous.state);
  if(current.running_on_cpu)continue;
  // The archived inspector recorded stacks only for selected tasks. Empty
  // reference arrays do not assert that a task had no unwindable frames.
  if(previous.stack.length){
   assert.deepEqual(current.stack.map(f=>f.lr),previous.stack.map(f=>f.lr));
   archivedStacks++;
  }
  // The old dump also attached zero-filled register words to terminal
  // ret_from_fork frames; those are not architectural IRQ records.
  for(const frame of previous.stack.filter(f=>f.lr!=='0x0'&&f.interrupted_registers?.some(x=>x!=='0x0'))){
   assert.ok(current.stack.some(f=>JSON.stringify(f.interrupted_registers)===JSON.stringify(frame.interrupted_registers)),'IRQ register frame mismatch for PID '+previous.pid);
   irqFrames++;
  }
  archivedTasks++;
 }
 assert.ok(archivedStacks>=3&&irqFrames>=1,'insufficient archived stack evidence');
}
console.log(JSON.stringify({status:'observer_matches_original_native_probe',instructions:actual.instructions,sha256:expectedSha,archived_tasks_compared:archivedTasks,archived_stacks_compared:archivedStacks,archived_irq_frames_compared:irqFrames,input_unchanged:true}));
