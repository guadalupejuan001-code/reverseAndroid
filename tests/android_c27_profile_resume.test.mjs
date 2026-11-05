// Metadata-policy fixtures only; actual state acceptance has a separate native integration check.
import test from 'node:test';import assert from 'node:assert/strict';
import {ROOT,validateContract,verifyNativePins,verifyCleanBoundary,hostTransportBound,contractTransportBound} from '../tools/android_c27_profile_resume.mjs';
import {DISK_SHA,SEED_SHA} from '../tools/android_c28_state.mjs';
const pins=()=>({schema:'reverse-c27-profile-native-pins-v1',root_checkpoint_sha256:ROOT.checkpoint_sha256,disk_sha256:DISK_SHA,counter_seed_sha256:SEED_SHA,logical_ram_bytes:2147483648,counter_instructions_per_tick:64,host_snapshot_transport_bytes:1073741824});
const contract=()=>({schema:'reverse-c27-profile-resume-contract-v1',checkpoint_sha256:ROOT.checkpoint_sha256,report_sha256:ROOT.report_sha256,result_sha256:ROOT.result_sha256,process_sha256:ROOT.process_sha256,source_commit:ROOT.source_commit,root_checkpoint_sha256:ROOT.checkpoint_sha256});
function fresh(){return {
 c:contract(),r:{checkpoint_sha256:ROOT.checkpoint_sha256,guest:{instructions:ROOT.instructions,pc:ROOT.pc,counter_subtick:ROOT.phase,halted:false,waiting:false,system_mode:true,pci_bus:true},initial_instructions:0,advanced_instructions:ROOT.instructions,stop_reason:'host_wall_budget',error:'',first_user_instruction_verified:true,prior_user_instruction_verified:false,console:'init: init second stage started!\naudit: type=1404 enforcing=1',provenance:{experiment:'c27-fresh-blank-data-profile',fresh_machine:true,original_checkpoint_cpu_restored:false,prior_init_proof_reused:false,source_initrd_unchanged:true}},
 p:{schema:'reverse-c27-fresh-result-v1',checkpoint_sha256:ROOT.checkpoint_sha256,instructions:ROOT.instructions,advanced_instructions:ROOT.instructions,stop_reason:'host_wall_budget',error:'',execution_progress_accepted:true,first_user_instruction_verified:true,second_stage_init_observed:true,selinux_enforcing_observed:true,fresh_machine:true,source_initrd_unchanged:true,old_cpu_or_init_proof_reused:false,guest_kernel_panic:false,guest_boot_failure:false,guest_terminal_failure:false,guest_reboot_requested:false},
 h:{status:'process_completed',exit_code:0,trigger:null,forced_kill:false,checkpoint_bytes_modified_by_supervisor:false}
};}
function child(generic=false){const x=fresh();x.c.source_commit=generic?'1'.repeat(40):'8dcb7159d22497b46b736227f4447df36c40bb23';x.c.checkpoint_sha256='a'.repeat(64);x.r.checkpoint_sha256=x.c.checkpoint_sha256;x.r.initial_instructions=ROOT.instructions+(generic?1000000:0);x.r.guest.instructions=x.r.initial_instructions+150000;x.r.advanced_instructions=150000;x.r.first_user_instruction_verified=false;x.r.verified_entry_checkpoint_sha256='';x.r.console='init: ordinary continuation';
 x.r.provenance={experiment:generic?'c27-profile-budget-continuation':'c28-c27-clean-continuation',root_checkpoint_sha256:ROOT.checkpoint_sha256,input_checkpoint_sha256:ROOT.checkpoint_sha256,input_report_sha256:ROOT.report_sha256,snapshot_round_trip_verified:true,source_profile_verified:true,original_state_modified:false,ram_profile_changed:false,host_snapshot_transport_bytes:1073741824,host_transport_policy_serialized:false,timing_seed_origin:'checkpoint',timing_seed_sha256:SEED_SHA,disk_sha256:DISK_SHA,source_c27_original_init_verified:true,source_c27_second_stage_verified:true,source_c27_selinux_enforcing_verified:true};
 Object.assign(x.p,{schema:generic?'reverse-c27-profile-budget-result-v1':'reverse-c28-result-v1',checkpoint_sha256:x.c.checkpoint_sha256,root_checkpoint_sha256:ROOT.checkpoint_sha256,input_checkpoint_sha256:ROOT.checkpoint_sha256,input_report_sha256:ROOT.report_sha256,instructions:x.r.guest.instructions,advanced_instructions:150000,source_c27_original_init_verified:true,source_c27_second_stage_verified:true,source_c27_selinux_enforcing_verified:true,clean_source_boundary_verified:true,guest_state_modified_before_resume:false});return x;}
const verify=x=>{validateContract(x.c);return verifyCleanBoundary(x.r,x.p,x.h,x.c);};
test('only the exact completed fresh root establishes the original init provenance',()=>assert.equal(verify(fresh()).source_c27_original_init_verified,true));
test('the verified C28 continuation carries the existing root',()=>assert.equal(verify(child()).clean_source_boundary_verified,true));
test('later clean profile budgets carry the same root without a new entry claim',()=>assert.equal(verify(child(true)).source_c27_original_init_verified,true));
test('compiled pins bind the board, clock, root and finite transport',()=>assert.equal(verifyNativePins(pins()).compiled_native_profile_verified,true));
for(const key of Object.keys(pins()))test('compiled mismatch rejected: '+key,()=>{const p=pins();p[key]=typeof p[key]==='string'?'bad':0;assert.throws(()=>verifyNativePins(p));});
for(const key of ['checkpoint_sha256','report_sha256','result_sha256','process_sha256','source_commit','root_checkpoint_sha256'])test('contract pin rejected: '+key,()=>{const x=fresh();x.c[key]='bad';assert.throws(()=>verify(x));});
const unsafe=[
 ['running observation',x=>x.r.stop_reason='running'],['ISA stop',x=>x.r.stop_reason='architectural_stop'],['failed native',x=>x.h.status='process_failed'],['native exit',x=>x.h.exit_code=2],['watchdog',x=>x.h.trigger='host_watchdog_wall'],['forced kill',x=>x.h.forced_kill=true],['host state mutation',x=>x.h.checkpoint_bytes_modified_by_supervisor=true],['prior failure',x=>x.p.guest_boot_failure=true],['prior reboot',x=>x.p.guest_reboot_requested=true],['unaccepted source',x=>x.p.execution_progress_accepted=false],['halted',x=>x.r.guest.halted=true],['waiting',x=>x.r.guest.waiting=true],['not a system',x=>x.r.guest.system_mode=false],['different board',x=>x.r.guest.pci_bus=false],['error',x=>x.r.error='unsupported'],['zero progress',x=>{x.r.advanced_instructions=0;x.p.advanced_instructions=0;}],['report/result count mismatch',x=>x.p.instructions++],['malformed PC',x=>x.r.guest.pc='invalid'],['invalid phase',x=>x.r.guest.counter_subtick=64],['panic console',x=>x.r.console+='\nKernel panic - not syncing: fixture'],['reboot console',x=>x.r.console+='\nreboot: Restarting system'],['data mount failed',x=>x.r.console+="\ninit: Command 'mount_all --late' action=late-fs (init.rc:73) took 1ms and failed: queue_fs_event() failed: Invalid code: -1"]
];
for(const[name,mutate]of unsafe)test('unsafe boundary rejected: '+name,()=>{for(const source of [fresh,child,()=>child(true)]){const x=source();mutate(x);assert.throws(()=>verify(x));}});
for(const key of ['checkpoint_sha256','report_sha256','result_sha256','process_sha256'])test('a different fresh root is rejected despite matching success metadata: '+key,()=>{const x=fresh();x.c[key]='b'.repeat(64);assert.throws(()=>verify(x));});
for(const key of ['first_user_instruction_verified','second_stage_init_observed','selinux_enforcing_observed','fresh_machine','source_initrd_unchanged'])test('fresh proof missing: '+key,()=>{const x=fresh();x.p[key]=false;assert.throws(()=>verify(x));});
for(const key of ['source_c27_original_init_verified','source_c27_second_stage_verified','source_c27_selinux_enforcing_verified','clean_source_boundary_verified'])test('continuation proof missing: '+key,()=>{for(const generic of [false,true]){const x=child(generic);x.p[key]=false;assert.throws(()=>verify(x));}});
const badChild=[['new entry claim',x=>x.r.first_user_instruction_verified=true],['entry claim reused',x=>x.r.prior_user_instruction_verified=true],['mutated state',x=>x.r.provenance.original_state_modified=true],['RAM change',x=>x.r.provenance.ram_profile_changed=true],['wrong clock seed',x=>x.r.provenance.timing_seed_sha256='0'.repeat(64)],['wrong disk',x=>x.r.provenance.disk_sha256='0'.repeat(64)],['no round trip',x=>x.r.provenance.snapshot_round_trip_verified=false],['serialized host policy',x=>x.r.provenance.host_transport_policy_serialized=true],['different input binding',x=>x.p.input_checkpoint_sha256='0'.repeat(64)]];
for(const[name,mutate]of badChild)test('altered continuation rejected: '+name,()=>{for(const generic of [false,true]){const x=child(generic);mutate(x);assert.throws(()=>verify(x));}});
test('unsupported result schemas and disconnected generic roots are rejected',()=>{let x=child();x.p.schema='other';assert.throws(()=>verify(x));x=child(true);x.r.provenance.root_checkpoint_sha256='0'.repeat(64);assert.throws(()=>verify(x));x=child(true);x.p.root_checkpoint_sha256='0'.repeat(64);assert.throws(()=>verify(x));x=child();x.c.source_commit='0'.repeat(40);assert.throws(()=>verify(x));});

const largeHost={maxTransportBytes:2147483648};
function largeChild(){const x=child(true);x.c.host_snapshot_transport_bytes=2147483648;x.r.provenance.host_snapshot_transport_bytes=2147483648;x.p.host_snapshot_transport_bytes=2147483648;x.p.host_transport_policy_serialized=false;return x;}
test('the larger host bound is explicit in contract, report, result and compiled native pins',()=>{
 const x=largeChild();assert.throws(()=>validateContract(x.c));assert.throws(()=>verifyCleanBoundary(x.r,x.p,x.h,x.c));
 assert.equal(validateContract(x.c,largeHost),x.c);assert.equal(verifyCleanBoundary(x.r,x.p,x.h,x.c,largeHost).clean_source_boundary_verified,true);
 const p={...pins(),host_snapshot_transport_bytes:2147483648};assert.throws(()=>verifyNativePins(p));assert.equal(verifyNativePins(p,largeHost).compiled_native_profile_verified,true);
 assert.throws(()=>verifyNativePins(pins(),largeHost));
});
test('old sources retain their exact original bound when opened by the larger host reader',()=>{
 const x=child(true);assert.equal(contractTransportBound(x.c,2147483648),1073741824);assert.equal(verifyCleanBoundary(x.r,x.p,x.h,x.c,largeHost).clean_source_boundary_verified,true);
});
for(const [name,change]of[
 ['report bound',x=>x.r.provenance.host_snapshot_transport_bytes=1073741824],
 ['result bound',x=>x.p.host_snapshot_transport_bytes=1073741824],
 ['missing declaration',x=>delete x.c.host_snapshot_transport_bytes],
 ['serialized policy',x=>x.p.host_transport_policy_serialized=true],
 ['architectural stop',x=>x.r.stop_reason='architectural_stop'],
 ['unclean native',x=>x.h.status='process_failed'],
 ['guest clock',x=>x.r.provenance.timing_seed_sha256='0'.repeat(64)],
 ['guest RAM',x=>x.r.provenance.ram_profile_changed=true],
 ['guest disk',x=>x.r.provenance.disk_sha256='0'.repeat(64)]
])test('larger transport does not bypass '+name,()=>{const x=largeChild();change(x);assert.throws(()=>verifyCleanBoundary(x.r,x.p,x.h,x.c,largeHost));});
test('the original fresh root cannot be relabeled as a new transport policy',()=>{const x=fresh();x.c.host_snapshot_transport_bytes=2147483648;assert.throws(()=>verifyCleanBoundary(x.r,x.p,x.h,x.c,largeHost));});
test('arbitrary, nonnumeric and unbounded host capacities are rejected',()=>{for(const n of [0,-1,256*1024*1024,1073741825,2147483649,Infinity,NaN,'2147483648'])assert.throws(()=>hostTransportBound(n));});
