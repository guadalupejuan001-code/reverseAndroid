// C35 source admission: accepting C34 only after two independent artifact
// contracts and the native checkpoint have been authenticated by preflight.
// This is a host policy check, not synthetic Android boot progress.
import assert from 'node:assert/strict';
import {C33_SOURCE} from './android_c34_stage.mjs';
const SHA=/^[0-9a-f]{64}$/;
export const C34_PRODUCER=Object.freeze({
  commit:'75f92559d488ca13f81e51acc8157aa650ff86e0',
  run_id:41345457550,
  checkpoint_artifact:'reverse-c34-authenticated-continuation',
  diagnostic_artifact:'reverse-c34-diagnostics'
});
export function admitC34Source({
  contract,report,boundary,stage,authenticated=false
}){
  assert.equal(authenticated,true,'Full four-file checkpoint preflight required');
  assert.equal(contract?.schema,'reverse-c27-profile-resume-contract-v1');
  assert.equal(contract.source_commit,C34_PRODUCER.commit);
  assert.match(contract.checkpoint_sha256,SHA);
  assert.match(contract.report_sha256,SHA);
  assert.match(contract.result_sha256,SHA);
  assert.match(contract.process_sha256,SHA);
  assert.notEqual(contract.checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(report?.checkpoint_sha256,contract.checkpoint_sha256);
  assert.equal(report.initial_instructions,C33_SOURCE.instructions);
  assert.ok(Number.isSafeInteger(report.guest?.instructions)&&report.guest.instructions>C33_SOURCE.instructions);
  assert.equal(report.advanced_instructions,report.guest.instructions-C33_SOURCE.instructions);
  assert.equal(report.error,'');
  assert.ok(['host_wall_budget','instruction_budget'].includes(report.stop_reason));
  assert.equal(report.guest.halted,false);
  assert.equal(report.guest.waiting,false);
  assert.equal(report.provenance?.experiment,'c27-profile-budget-continuation');
  assert.equal(report.provenance.input_checkpoint_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(report.provenance.snapshot_round_trip_verified,true);
  assert.equal(report.provenance.original_state_modified,false);
  assert.equal(report.provenance.ram_profile_changed,false);
  assert.equal(boundary?.schema,'reverse-c27-sequence-boundary-v1');
  assert.equal(boundary.clean_completed,true);
  assert.equal(boundary.checkpoint_sha256,contract.checkpoint_sha256);
  assert.equal(boundary.instructions,report.guest.instructions);
  assert.equal(stage?.schema,'reverse-android-c34-stage-v1');
  assert.equal(stage.original_c33_source_authenticated,true);
  assert.equal(stage.c34_output_authenticated,true);
  assert.equal(stage.source_sha256,C33_SOURCE.checkpoint_sha256);
  assert.equal(stage.output_sha256,contract.checkpoint_sha256);
  assert.equal(stage.instructions,report.guest.instructions);
  assert.equal(stage.added_instructions,report.guest.instructions-C33_SOURCE.instructions);
  assert.equal(stage.source_checkpoint_unchanged,true);
  assert.equal(stage.android_display_content_verified,false);
  assert.ok(['odsign_running','odsign_exited_clean','odsign_exit_nonzero',
    'zygote_start_requested','framework_processes_running',
    'framework_verified','post_odsign_unresolved'].includes(stage.stage));
  assert.notEqual(stage.stage,'odsign_exit_nonzero','Service failure requires diagnosis before C35 continuation');
  assert.equal(stage.selinux_enforcing_live,true,'Live SELinux enforcement required');
  assert.equal(typeof stage.live_framework_verified,'boolean');
  assert.equal(typeof stage.sys_boot_completed_live,'boolean');
  assert.equal(typeof stage.scanout_exported,'boolean');
  assert.equal(typeof stage.ready_for_visual_review,'boolean');
  if(stage.ready_for_visual_review)assert.equal(stage.live_framework_verified&&stage.scanout_exported,true);
  return Object.freeze({
    schema:'reverse-c35-source-admission-v1',
    source_run_id:C34_PRODUCER.run_id,
    source_commit:C34_PRODUCER.commit,
    source_checkpoint_sha256:contract.checkpoint_sha256,
    source_report_sha256:contract.report_sha256,
    source_instructions:report.guest.instructions,
    source_stage:stage.stage,
    source_output_contract_authenticated:true,
    source_stage_verified:true,
    android_boot_claimed:false
  });
}
