import {test} from 'node:test';
import assert from 'node:assert/strict';
import {analyzeGuestProgress,classifyGuestStop} from '../tools/android_guest_progress.mjs';
const sample=(start,end,pc,x19,x20,deltas,stop='instruction_budget')=>({
  initial_instructions:start,host_elapsed_ns:1_000_000_000,
  guest:{pc,instructions:end,delta_overrides:deltas},
  next_instruction:{registers:{x19,x20}},stop_reason:stop,
  console:'',android_boot:false,linux_userspace_init_observed:false
});
test('actual repeated ARM64 PCs with changing pointers and XI deltas are not frozen',()=>{
  const a=sample(20000,2020000,'0x40407588','0x3021a4','0xffffffc08224ce38',34562);
  const b=sample(2020000,4020000,'0x40407594','0x2df590','0xffffffc08226fa4c',52726);
  const result=analyzeGuestProgress([a,b]);
  assert.equal(result.progress_kind,'cpu_state_changes_stage_unclassified');
  assert.equal(result.original_guest_instructions_executed,4000000);
  assert.equal(result.measured_instructions_per_second,2000000);
  assert.equal(result.suspected_no_progress,false);
  assert.equal(result.android_first_stage_init_observed,false);
});
test('never classify a stationary CPU loop as successful Android boot',()=>{
  const a=sample(0,100,'0x40000040','0x0','0x0',0);
  const b=sample(100,200,'0x40000040','0x0','0x0',0);
  const r=analyzeGuestProgress([a,b]);
  assert.equal(r.progress_kind,'possible_busy_wait_no_observed_data_progress');
  assert.equal(r.suspected_no_progress,true);
  assert.equal(r.adb,false);
});
test('report actual guest stop reasons, not only loop heuristics',()=>{
  const a=sample(0,100,'0x40000040','0x1','0x2',0);
  const b=sample(100,101,'0x40000044','0x1','0x2',0,
    'Unsupported AArch64 system register read 0xc03b');
  const r=analyzeGuestProgress([a,b]);
  assert.equal(r.progress_kind,'architectural_stop');
  assert.equal(r.all_stops_instruction_budget,false);
});
test('reject missing or discontinuous real checkpoint input',()=>{
  const a=sample(0,10,'0x40000040','0x1','0x2',0);
  const b=sample(12,20,'0x40000040','0x1','0x2',0);
  assert.throws(()=>analyzeGuestProgress([a,b]),/noncontiguous/);
  assert.throws(()=>analyzeGuestProgress([a]),/2..64/);
  b.initial_instructions=10;b.guest.pc='not a pointer';
  assert.throws(()=>analyzeGuestProgress([a,b]),/pc is not a hex address/);
});
test('refuse unsupported Android boot flag in telemetry without a first-stage oracle',()=>{
  const a=sample(0,10,'0x40000040','0x1','0x2',0);
  const b=sample(10,20,'0x40000040','0x1','0x2',0);
  b.android_boot=true;
  assert.throws(()=>analyzeGuestProgress([a,b]),/boot observation requires/);
});
test('actual host-wall pauses remain reanudable controls and retain the original stop',()=>{
  const a=sample(2831426448,2875196448,'0xffffffc080208174','0x1','0x2',10,'host_wall_budget');
  const b=sample(2875196448,2917736448,'0xffffffc08020610c','0x3','0x4',20,'host_wall_budget');
  const r=analyzeGuestProgress([a,b]);
  assert.equal(r.progress_kind,'host_time_pause');
  assert.equal(r.samples[1].stop,'host_wall_budget');
  assert.equal(r.samples[1].stop_classification,'host_time_pause');
  assert.equal(r.original_guest_instructions_executed,86310000);
  assert.equal(r.android_first_stage_init_observed,false);
});
test('waiting and EL0 observation are separate boundaries and do not prove boot',()=>{
  for(const [stop,kind] of [['waiting_for_device_or_interrupt','device_or_interrupt_wait'],
    ['userspace_entry','userspace_observation_boundary']]){
    const a=sample(0,10,'0x40000040','0x1','0x2',0);
    const b=sample(10,20,'0x40000044','0x1','0x2',0,stop);
    const r=analyzeGuestProgress([a,b]);
    assert.equal(r.progress_kind,kind);
    assert.equal(r.android_first_stage_init_observed,false);
    assert.equal(r.adb,false);
  }
  assert.equal(classifyGuestStop('Unsupported AArch64 instruction 0x0'),'architectural_stop');
});
test('resuming a host pause does not retain a false final architectural stop',()=>{
  const a=sample(0,10,'0x40000040','0x1','0x2',0,'host_wall_budget');
  const b=sample(10,20,'0x40000044','0x3','0x4',1);
  assert.equal(analyzeGuestProgress([a,b]).progress_kind,'cpu_state_changes_stage_unclassified');
});
