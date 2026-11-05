import test from 'node:test';
import assert from 'node:assert/strict';
import {classifyRecovery} from '../tools/android_c40_recovery_evidence.mjs';

const sourceCheckpointSha='094d5c9b9532fbb21ea029031b952f46c4df0e589c2f0d75a4dd64677bc49438';
const sourceInstructions=18393448839;
const journal={verified:true,sha256:'f'.repeat(64),instructions:18394448839,report_stop_reason:'instruction_budget'};
const report=()=>({stop_reason:'instruction_budget',error:'',initial_instructions:sourceInstructions,
  advanced_instructions:1000000,guest:{instructions:sourceInstructions+1000000,halted:false,waiting:false},console:''});
const host=()=>({status:'process_completed',exit_code:0,trigger:null,forced_kill:false,last_reported_instructions:sourceInstructions+1000000});
const classify=(overrides={})=>classifyRecovery({sourceCheckpointSha,sourceInstructions,processOutcome:overrides.process??host(),
  report:overrides.report??report(),journalStatus:overrides.journal??journal});

test('clean native budget and cryptographically verified journal can be considered for a separate contract gate',()=>{
 const p=classify();
 assert.equal(p.clean_native_budget,true);
 assert.equal(p.journal_sha_verified,true);
 assert.equal(p.progressed,true);
 assert.equal(p.failure_class,'none');
 assert.equal(p.recovery_candidate_is_promotion_authority,false);
 assert.equal(p.resumption_source_remains_last_clean_contract,true);
});
test('C40-like unexpected process exit keeps C39 as the only authorized source',()=>{
 const p=classify({process:{...host(),status:'process_failed',exit_code:2}});
 assert.equal(p.failure_class,'native_process_process_failed');
 assert.equal(p.clean_native_budget,false);
 assert.equal(p.original_source_sha256,sourceCheckpointSha);
 assert.equal(p.journal_sha_verified,true);
});
test('supervisor timeout, SIGTERM and killed process do not become clean checkpoints',()=>{
 const p=classify({process:{...host(),status:'host_stopped',trigger:'host_watchdog_no_progress',exit_code:null,exit_signal:'SIGTERM',forced_kill:false}});
 assert.equal(p.failure_class,'host_supervisor_host_watchdog_no_progress');
 assert.equal(p.supervisor_signal,'SIGTERM');
 assert.equal(p.clean_native_budget,false);
});
test('an architectural ISA stop is evidence, not a resumable successful stop',()=>{
 const p=classify({report:{...report(),stop_reason:'architectural_stop',error:'Unsupported AArch64 instruction'}});
 assert.equal(p.failure_class,'guest_or_interpreter_error');
 assert.equal(p.clean_native_budget,false);
});
test('the real native exit 2 preserves an architectural stop as the primary cause',()=>{
 const error='Unsupported AArch64 instruction 0x00000000 at 0x400000';
 const p=classify({process:{...host(),status:'process_failed',exit_code:2},
  report:{...report(),stop_reason:'architectural_stop',error}});
 assert.equal(p.failure_class,'guest_or_interpreter_error');
 assert.equal(p.guest_error,error);
 assert.equal(p.supervisor_exit_code,2);
 assert.equal(p.clean_native_budget,false);
 assert.equal(p.recovery_candidate_is_promotion_authority,false);
});
test('a native exit 2 with a guest device wait reports that wait instead of a generic process failure',()=>{
 const r=report();r.stop_reason='waiting_for_device_or_interrupt';r.guest.waiting=true;
 const p=classify({process:{...host(),status:'process_failed',exit_code:2},report:r});
 assert.equal(p.failure_class,'guest_stop_waiting_for_device_or_interrupt');
 assert.equal(p.supervisor_exit_code,2);
 assert.equal(p.clean_native_budget,false);
});
test('a supervisor termination remains primary even if the guest records an interrupted stop',()=>{
 const p=classify({process:{...host(),status:'host_stopped',trigger:'host_watchdog_no_progress',exit_code:2},
  report:{...report(),stop_reason:'host_interrupted',error:''}});
 assert.equal(p.failure_class,'host_supervisor_host_watchdog_no_progress');
 assert.equal(p.clean_native_budget,false);
});
test('an unfinished running report cannot invent a guest stop when the process fails',()=>{
 const p=classify({process:{...host(),status:'process_failed',exit_code:1},
  report:{...report(),stop_reason:'running',error:''}});
 assert.equal(p.failure_class,'native_process_process_failed');
 assert.equal(p.clean_native_budget,false);
});
test('a journal with no verified hash cannot be trusted, even when the host reports success',()=>{
 const p=classify({journal:{verified:false,available:true,error:'SHA mismatch'}});
 assert.equal(p.clean_native_budget,false);
 assert.equal(p.failure_class,'no_verified_recoverable_journal');
 assert.equal(p.journal_checkpoint_sha256,null);
});
test('missing journal and missing host outcome preserve C39 source and do not invent a terminal state',()=>{
 const p=classifyRecovery({sourceCheckpointSha,sourceInstructions,processOutcome:null,report:null,journalStatus:null});
 assert.equal(p.observed_instructions,sourceInstructions);
 assert.equal(p.clean_native_budget,false);
 assert.equal(p.failure_class,'no_verified_recoverable_journal');
 assert.equal(p.resumption_source_remains_last_clean_contract,true);
});
test('the guest reports a panic even when a zero exit and budget would otherwise appear clean',()=>{
 const r=report();r.console='Kernel panic - not syncing: emergency\n';
 const p=classify({report:r});
 assert.equal(p.clean_native_budget,false);
 assert.equal(p.recovery_candidate_is_promotion_authority,false);
});
