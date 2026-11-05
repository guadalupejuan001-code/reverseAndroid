// C40R: forensic diagnosis of real native continuation checkpoints. Do not
// promote a host-stopped, exception, panic or incomplete journal into a source.
// The *only* resume authority is the existing four-file clean output contract.
import assert from 'node:assert/strict';
import {createReadStream,existsSync,statSync,readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {join} from 'node:path';
import {preflight,hostTransportBound} from './android_c27_profile_resume.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {acceptsProgress} from './android_c15_result.mjs';

const HEX=/^[a-f0-9]{64}$/, GENERATION=/^generation-[1-9][0-9]*$/;
const MAX_SNAPSHOT=1073741824;
const limitedJson=(file,max=8*1024*1024)=>{
  assert.ok(statSync(file).size<=max,'Recovery diagnostic input oversized: '+file);
  return JSON.parse(readFileSync(file,'utf8'));
};
const safeJson=(p,max)=>{
  if(!existsSync(p))return null;
  try{return limitedJson(p,max);}
  catch(e){return {unreadable:true,reason:e.message};}
};
async function shaFile(file,maximum=MAX_SNAPSHOT){
  assert.ok(statSync(file).size<=maximum,'Recovery snapshot is oversized');
  const h=createHash('sha256');
  for await (const bytes of createReadStream(file,{highWaterMark:1024*1024}))h.update(bytes);
  return h.digest('hex');
}

export function classifyRecovery({sourceCheckpointSha,sourceInstructions,processOutcome,report,journalStatus}){
  assert.match(sourceCheckpointSha,HEX);
  assert.ok(Number.isSafeInteger(sourceInstructions)&&sourceInstructions>0);
  const processStatus=processOutcome?.status??'unavailable';
  const stopReason=report?.stop_reason??journalStatus?.report_stop_reason??'unavailable';
  const reported=report?.guest?.instructions??journalStatus?.instructions??sourceInstructions;
  const progressed=Number.isSafeInteger(reported)&&reported>sourceInstructions;
  const clean=!!(journalStatus?.verified===true&&processOutcome?.status==='process_completed'&&processOutcome.exit_code===0&&
    processOutcome.trigger===null&&processOutcome.forced_kill===false&&
    report&&['host_wall_budget','instruction_budget'].includes(report.stop_reason)&&
    report.error===''&&report.guest?.halted===false&&report.guest?.waiting===false&&
    acceptsProgress(report,analyzeGuestConsole(report.console??'')));
  let failure='none';
  if(!clean){
    if(report?.console&&analyzeGuestConsole(report.console).guest_boot_failure)failure='guest_boot_failure';
    else if(processOutcome?.trigger)failure='host_supervisor_'+processOutcome.trigger;
    // The native runner deliberately exits 2 on guest/ISA stops. Preserve
    // the structured cause instead of masking it with that expected exit.
    else if(report?.error)failure='guest_or_interpreter_error';
    else if(report?.stop_reason&&!['running','host_wall_budget','instruction_budget'].includes(report.stop_reason))failure='guest_stop_'+report.stop_reason;
    else if(['process_failed','spawn_failed'].includes(processStatus))failure='native_process_'+processStatus;
    else if(!journalStatus?.verified)failure='no_verified_recoverable_journal';
    else failure='unclean_continuation_unclassified';
  }
  return {schema:'reverse-c40r-recovery-classification-v1',original_source_sha256:sourceCheckpointSha,
    original_source_instructions:sourceInstructions,observed_instructions:reported,
    progressed,supervisor_status:processStatus,supervisor_trigger:processOutcome?.trigger??null,
    supervisor_exit_code:processOutcome?.exit_code??null,supervisor_signal:processOutcome?.exit_signal??null,
    supervisor_forced_kill:processOutcome?.forced_kill??null,
    guest_stop_reason:stopReason,guest_error:report?.error??null,
    last_progress_marker:processOutcome?.last_reported_instructions??null,
    journal_sha_verified:journalStatus?.verified===true,
    journal_checkpoint_sha256:journalStatus?.verified?journalStatus.sha256:null,
    journal_instructions:journalStatus?.verified?journalStatus.instructions:null,
    failure_class:failure,clean_native_budget:clean,
    recovery_candidate_is_promotion_authority:false,
    resumption_source_remains_last_clean_contract:true};
}

export async function inspectNativeRecovery({sourceContract,source,nativeDirectory,maxTransportBytes=MAX_SNAPSHOT}){
 hostTransportBound(maxTransportBytes);
  const sourceProof=preflight(sourceContract,source,{maxTransportBytes});
  const outcome=safeJson(join(nativeDirectory,'process-outcome.json'),131072);
  const report=safeJson(join(nativeDirectory,'current.json'),8*1024*1024);
  const journalRoot=join(nativeDirectory,'journal');
  const pointer=safeJson(join(journalRoot,'latest.json'),131072);
  let journal={verified:false,available:false,error:null};
  if(pointer){
    try{
      assert.ok(!pointer.unreadable,'Recovery journal latest pointer cannot be read');
      assert.match(pointer.generation,GENERATION);
      assert.match(pointer.checkpoint_sha256,HEX);
      const dir=join(journalRoot,pointer.generation);
      const raw=join(dir,'checkpoint.rvg'),r=limitedJson(join(dir,'report.json'));
      const digest=await shaFile(raw,maxTransportBytes);
      assert.equal(digest,pointer.checkpoint_sha256,'Recovery journal SHA differs');
      assert.equal(r.checkpoint_sha256,digest,'Recovery journal report has wrong checkpoint SHA');
      assert.equal(r.initial_instructions,sourceProof.report.guest.instructions);
      assert.equal(r.provenance?.input_checkpoint_sha256,sourceContract.checkpoint_sha256);
      assert.equal(r.provenance?.input_report_sha256,sourceContract.report_sha256);
      assert.equal(r.provenance?.root_checkpoint_sha256,sourceContract.root_checkpoint_sha256);
      assert.ok(Number.isSafeInteger(r.guest?.instructions)&&r.guest.instructions>=r.initial_instructions);
      assert.equal(r.guest?.halted,false,'Recovery report describes a halted guest');
      journal={verified:true,available:true,sha256:digest,instructions:r.guest.instructions,
        report_stop_reason:r.stop_reason,guest_pc:r.guest.pc,
        snapshot_bytes:statSync(raw).size,report_bytes:statSync(join(dir,'report.json')).size,
        generation:pointer.generation,error:null};
    }catch(e){
      journal={verified:false,available:true,error:e.message,
        // No checkpoint hash is exposed as trustworthy until cryptographically checked.
        generation:GENERATION.test(pointer.generation??'')?pointer.generation:null};
    }
  }
  const classification=classifyRecovery({sourceCheckpointSha:sourceContract.checkpoint_sha256,
    sourceInstructions:sourceProof.report.guest.instructions,
    processOutcome:outcome?.unreadable?null:outcome,
    report:report?.unreadable?null:report,journalStatus:journal});
  const terminal=report?.unreadable?null:report;
  const signals=terminal?.console?analyzeGuestConsole(terminal.console):null;
  return {...classification,source_lineage_preflight_verified:true,
    journal,guest_failure_signals:signals?.guest_failure_signals??[],
    native_process_log_present:existsSync(join(nativeDirectory,'progress.log')),
    native_process_log_bytes:existsSync(join(nativeDirectory,'progress.log'))?statSync(join(nativeDirectory,'progress.log')).size:0,
    native_status_json_present:!!outcome,report_json_present:!!report,
    no_false_boot_claim:true,observer_only:true};
}
