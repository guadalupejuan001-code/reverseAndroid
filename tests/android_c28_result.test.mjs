// Metadata policy fixtures only. Real CPU/source acceptance is tested separately on the pinned C27 artifact.
import test from 'node:test';import assert from 'node:assert/strict';
import {verifyNativePins,verifyCleanSourceBoundary,INPUT_SHA,REPORT_SHA,BOUNDARY} from '../tools/android_c28_result.mjs';
import {DISK_SHA,SEED_SHA} from '../tools/android_c28_state.mjs';
const pins=()=>({schema:'reverse-c28-native-pins-v1',state_sha256:INPUT_SHA,report_sha256:REPORT_SHA,disk_sha256:DISK_SHA,counter_seed_sha256:SEED_SHA,host_snapshot_transport_bytes:1073741824});
test('compiled pins bind the exact clean C27 source before opening a disk',()=>assert.equal(verifyNativePins(pins()).compiled_native_pins_verified,true));
for(const key of Object.keys(pins()))test('a changed compiled pin is rejected: '+key,()=>{const p=pins();p[key]=typeof p[key]==='string'?'x':1;assert.throws(()=>verifyNativePins(p));});
function fixture(){return {
 r:{guest:{instructions:BOUNDARY.instructions,pc:BOUNDARY.pc,counter_subtick:BOUNDARY.phase,halted:false,waiting:false},stop_reason:'host_wall_budget',error:'',first_user_instruction_verified:true,prior_user_instruction_verified:false,console:'init: init second stage started!\n[1.0][ T1] audit: type=1404 enforcing=1'},
 prior:{schema:'reverse-c27-fresh-result-v1',checkpoint_sha256:INPUT_SHA,instructions:BOUNDARY.instructions,stop_reason:'host_wall_budget',error:'',execution_progress_accepted:true,first_user_instruction_verified:true,second_stage_init_observed:true,selinux_enforcing_observed:true,fresh_machine:true,source_initrd_unchanged:true,old_cpu_or_init_proof_reused:false,guest_kernel_panic:false,guest_boot_failure:false,guest_terminal_failure:false,guest_reboot_requested:false,android_framework_verified:false,android_display_verified:false},
 host:{status:'process_completed',exit_code:0}
};}
const verify=x=>verifyCleanSourceBoundary(x.r,x.prior,x.host);
test('a clean source policy carries the actually observed original init boundary',()=>assert.equal(verify(fixture()).clean_source_boundary_verified,true));
const changes=[
 ['running source',x=>x.r.stop_reason='running'],['ISA stop',x=>x.r.stop_reason='architectural_stop'],['failed native',x=>x.host.status='process_failed'],['native nonzero exit',x=>x.host.exit_code=2],['checkpoint mismatch',x=>x.prior.checkpoint_sha256='0'.repeat(64)],['changed native count',x=>x.r.guest.instructions++],['changed PC',x=>x.r.guest.pc='0x1'],['changed timer phase',x=>x.r.guest.counter_subtick=-1],['halted',x=>x.r.guest.halted=true],['waiting',x=>x.r.guest.waiting=true],['source report error',x=>x.r.error='unsupported'],['unobserved init entry',x=>x.r.first_user_instruction_verified=false],['inherited entry',x=>x.r.prior_user_instruction_verified=true],['missing second stage',x=>x.r.console='audit: type=1404 enforcing=1'],['missing SELinux proof',x=>x.r.console='init: init second stage started!'],['panic console',x=>x.r.console+='\nKernel panic - not syncing: fixture'],['reboot console',x=>x.r.console+='\nreboot: Restarting system with command recovery'],['late-mount error',x=>x.r.console+="\ninit: Command 'mount_all --late' action=late-fs (init.rc:73) took 1ms and failed: queue_fs_event() failed: Invalid code: -1"]
];
for(const [name,mutate]of changes)test('unsafe source rejected: '+name,()=>{const x=fixture();mutate(x);assert.throws(()=>verify(x));});
for(const key of ['execution_progress_accepted','first_user_instruction_verified','second_stage_init_observed','selinux_enforcing_observed','fresh_machine','source_initrd_unchanged'])test('missing proven source property: '+key,()=>{const x=fixture();x.prior[key]=false;assert.throws(()=>verify(x));});
for(const key of ['old_cpu_or_init_proof_reused','guest_kernel_panic','guest_boot_failure','guest_terminal_failure','guest_reboot_requested','android_framework_verified','android_display_verified'])test('contradictory source property: '+key,()=>{const x=fixture();x.prior[key]=true;assert.throws(()=>verify(x));});
