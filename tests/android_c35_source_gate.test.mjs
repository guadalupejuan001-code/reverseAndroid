// Tests validate policy only: no emulated Android boot here.
import test from 'node:test';
import assert from 'node:assert/strict';
import {C33_SOURCE} from '../tools/android_c34_stage.mjs';
import {admitC34Source,C34_PRODUCER} from '../tools/android_c35_source_gate.mjs';
function fixture(){
  const instructions=C33_SOURCE.instructions+100000;
  const checkpoint='a'.repeat(64);
  return {
    authenticated:true,
    contract:{schema:'reverse-c27-profile-resume-contract-v1',
      source_commit:C34_PRODUCER.commit,checkpoint_sha256:checkpoint,
      report_sha256:'b'.repeat(64),result_sha256:'c'.repeat(64),process_sha256:'d'.repeat(64)},
    report:{checkpoint_sha256:checkpoint,initial_instructions:C33_SOURCE.instructions,
      guest:{instructions,halted:false,waiting:false},advanced_instructions:100000,
      stop_reason:'host_wall_budget',error:'',
      provenance:{experiment:'c27-profile-budget-continuation',
        input_checkpoint_sha256:C33_SOURCE.checkpoint_sha256,
        snapshot_round_trip_verified:true,original_state_modified:false,ram_profile_changed:false}},
    boundary:{schema:'reverse-c27-sequence-boundary-v1',clean_completed:true,
      checkpoint_sha256:checkpoint,instructions},
    stage:{schema:'reverse-android-c34-stage-v1',
      original_c33_source_authenticated:true,c34_output_authenticated:true,
      source_sha256:C33_SOURCE.checkpoint_sha256,output_sha256:checkpoint,instructions,
      added_instructions:100000,source_checkpoint_unchanged:true,
      stage:'odsign_running',selinux_enforcing_live:true,
      live_framework_verified:false,sys_boot_completed_live:false,
      scanout_exported:false,ready_for_visual_review:false,
      android_display_content_verified:false}
  };
}
test('authentic output and C34 evidence are eligible for C35',()=>{
  const r=admitC34Source(fixture());
  assert.equal(r.source_commit,C34_PRODUCER.commit);
  assert.equal(r.source_checkpoint_sha256,'a'.repeat(64));
  assert.equal(r.source_stage,'odsign_running');
  assert.equal(r.android_boot_claimed,false);
});
test('zygote and framework stage are never falsely converted into boot claims',()=>{
  for(const stage of ['zygote_start_requested','framework_processes_running','framework_verified']){
    const x=fixture();x.stage.stage=stage;
    const r=admitC34Source(x);assert.equal(r.source_stage,stage);
    assert.equal(r.android_boot_claimed,false);
  }
});
const invalid=[
  ['not preflight authenticated',x=>x.authenticated=false],
  ['wrong C34 producer commit',x=>x.contract.source_commit='0'.repeat(40)],
  ['missing digest',x=>x.contract.process_sha256=''],
  ['source is actually C33',x=>x.contract.checkpoint_sha256=C33_SOURCE.checkpoint_sha256],
  ['wrong report hash',x=>x.report.checkpoint_sha256='0'.repeat(64)],
  ['source counter mismatch',x=>x.report.initial_instructions++],
  ['no CPU advancement',x=>{x.report.guest.instructions=C33_SOURCE.instructions;x.report.advanced_instructions=0}],
  ['invalid instruction delta',x=>x.report.advanced_instructions++],
  ['architectural trap',x=>x.report.error='Unsupported instruction'],
  ['reboot/halted',x=>x.report.guest.halted=true],
  ['wrong origin',x=>x.report.provenance.input_checkpoint_sha256='0'.repeat(64)],
  ['altered original',x=>x.report.provenance.original_state_modified=true],
  ['bad boundary',x=>x.boundary.clean_completed=false],
  ['wrong boundary digest',x=>x.boundary.checkpoint_sha256='0'.repeat(64)],
  ['untrusted C34 stage',x=>x.stage.c34_output_authenticated=false],
  ['mixed C33 stage origin',x=>x.stage.source_sha256='0'.repeat(64)],
  ['mixed C34 stage digest',x=>x.stage.output_sha256='0'.repeat(64)],
  ['wrong stage instruction count',x=>x.stage.instructions++],
  ['unsupported stage state',x=>x.stage.stage='android_ready'],
  ['failed odsign service',x=>x.stage.stage='odsign_exit_nonzero'],
  ['SELinux enforcement missing',x=>x.stage.selinux_enforcing_live=false],
  ['modified input checkpoint',x=>x.stage.source_checkpoint_unchanged=false],
  ['visual proof invented',x=>x.stage.android_display_content_verified=true],
  ['inconsistent visual readiness',x=>x.stage.ready_for_visual_review=true]
];
for(const [name,change] of invalid)test('reject '+name,()=>{
  const x=fixture();change(x);assert.throws(()=>admitC34Source(x));
});
