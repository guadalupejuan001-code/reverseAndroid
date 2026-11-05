#!/usr/bin/env node
// Read-only Android guest early-boot diagnostic. Never interpret instruction
// counts as first-stage /init, Binder, ADB, graphics or game boot evidence.
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const fail=message=>{throw Error('REVERSE Android progress: '+message);};
const positive=(x,label)=>{
  if(!Number.isSafeInteger(x)||x<0)fail(label+' invalid');
  return x;
};
const address=(x,label)=>{
  if(typeof x!=='string'||!/^0x[0-9a-f]+$/i.test(x))
    fail(label+' is not a hex address');
  return BigInt(x);
};
// These are host controls and observation boundaries, not ISA failures.
export function classifyGuestStop(stop){
  if(stop==='host_wall_budget')return 'host_time_pause';
  if(stop==='instruction_budget')return 'instruction_budget';
  if(stop==='waiting_for_device_or_interrupt')return 'device_or_interrupt_wait';
  if(stop==='userspace_entry')return 'userspace_observation_boundary';
  return 'architectural_stop';
}
export function analyzeGuestProgress(samples){
  if(!Array.isArray(samples)||samples.length<2||samples.length>64)
    fail('need 2..64 real checkpoint samples');
  const records=samples.map((s,index)=>{
    const steps=positive(s?.guest?.instructions,'instructions');
    const start=positive(s?.initial_instructions,'initial instructions');
    const elapsed=positive(s?.host_elapsed_ns,'host elapsed');
    const overrides=positive(s?.guest?.delta_overrides??0,'delta overrides');
    const x19=address(s?.next_instruction?.registers?.x19,'x19');
    const x20=address(s?.next_instruction?.registers?.x20,'x20');
    const pc=address(s?.guest?.pc,'pc');
    const stop=String(s?.stop_reason??'');
    const uart=String(s?.console??'');
    if(steps<start||elapsed<=0)fail('bad instruction or timing bounds');
    if(s?.android_boot!==false||s?.linux_userspace_init_observed!==false)
      fail('boot observation requires different explicit acceptance oracle');
    return {index,initial:start,instructions:steps,elapsed_ns:elapsed,
      overrides,pc,x19,x20,uart_chars:uart.length,stop,
      stop_classification:classifyGuestStop(stop)};
  });
  for(let i=1;i<records.length;i++){
    if(records[i].initial!==records[i-1].instructions)
      fail('noncontiguous original XI/Ether checkpoint series');
  }
  const first=records[0],last=records.at(-1);
  const total=records.reduce((a,b)=>a+b.instructions-b.initial,0);
  const elapsed=records.reduce((a,b)=>a+b.elapsed_ns,0);
  const pointerChanged=last.x19!==first.x19||last.x20!==first.x20;
  const memoryChanged=last.overrides!==first.overrides;
  const pcChanged=last.pc!==first.pc;
  const allBudget=records.every(x=>x.stop==='instruction_budget');
  const samePC=records.every(x=>x.pc===first.pc);
  const completelyStatic=total>0&&!pointerChanged&&!memoryChanged&&samePC;
  const lastStop=last.stop_classification;
  const progressKind=lastStop!=='instruction_budget'?lastStop
    :pointerChanged&&memoryChanged?'cpu_state_changes_stage_unclassified'
    :completelyStatic?'possible_busy_wait_no_observed_data_progress'
    : 'instructions_executed_stage_unclassified';
  // These estimates are deliberately not UI/Android availability metrics.
  const averageIps=Math.round(total*1e9/elapsed);
  return {status:'real_guest_diagnostic_only',progress_kind:progressKind,
    original_guest_instructions_executed:total,
    observed_start_instruction:first.initial,
    observed_end_instruction:last.instructions,
    observed_wall_ns:elapsed,measured_instructions_per_second:averageIps,
    observed_pc_changed:pcChanged,
    observed_x19_changed:last.x19!==first.x19,
    observed_x20_changed:last.x20!==first.x20,
    observed_inverse_memory_deltas_changed:memoryChanged,
    samples:records.map(x=>({instruction:x.instructions,pc:'0x'+x.pc.toString(16),
      x19:'0x'+x.x19.toString(16),x20:'0x'+x.x20.toString(16),
      overrides:x.overrides,stop:x.stop,stop_classification:x.stop_classification,
      elapsed_ns:x.elapsed_ns})),
    suspected_no_progress:completelyStatic,
    all_stops_instruction_budget:allBudget,
    android_first_stage_init_observed:false,adb:false,
    note:completelyStatic
      ?'Suspicious repeat state: do not auto-advance, inspect timer/IRQ and kernel state'
      :'CPU transitions with changing state are not a completed Android boot'
  };
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const args=process.argv.slice(2);
    if(args.length<2||args.length>64)fail('usage: node android_guest_progress.mjs batch-1.json batch-2.json ...');
    console.log(JSON.stringify(analyzeGuestProgress(args.map(p=>JSON.parse(readFileSync(p,'utf8')))),null,2));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
