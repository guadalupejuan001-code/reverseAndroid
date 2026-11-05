// Reusable clean-budget continuation for the exact C27 board and original init proof.
// ISA stops, incomplete observations, terminal guests and other boards are rejected.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,statSync,mkdirSync,linkSync,copyFileSync as copy,existsSync} from 'node:fs';
import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';import {spawnSync} from 'node:child_process';
import {verifyC28State as verifyBoardState,DISK_SHA,SEED_SHA} from './android_c28_state.mjs';
import {preflight as verifyOriginalRoot} from './android_c28_result.mjs';
import {continuationWatchdogMs} from './android_profile_resume.mjs';
import {sha} from './android_c13_avb.mjs';import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {acceptsProgress} from './android_c15_result.mjs';import {superviseNativeCycle} from './android_supervised_cycle.mjs';
export const ROOT=Object.freeze(JSON.parse(read(new URL('./android_c28_contract.json',import.meta.url))));
const capacity=1073741824,hashPattern=/^[0-9a-f]{64}$/;
const bounded=(p,n)=>{assert.ok(statSync(p).size<=n,'Oversized evidence');return read(p);};const json=p=>JSON.parse(bounded(p,8*1024*1024));
export function hostTransportBound(value=capacity){
 assert.ok(value===capacity||value===2147483648,'Unsupported explicit host transport bound');return value;
}
export function contractTransportBound(c,maximum=capacity){
 const value=hostTransportBound(c.host_snapshot_transport_bytes??capacity);
 assert.ok(value<=hostTransportBound(maximum),'Contract requires an explicit larger host transport');return value;
}
export function validateContract(c,{maxTransportBytes=capacity}={}){
 assert.equal(c.schema,'reverse-c27-profile-resume-contract-v1');contractTransportBound(c,maxTransportBytes);
 for(const key of ['checkpoint_sha256','report_sha256','result_sha256','process_sha256'])assert.match(c[key],hashPattern,key);
 assert.match(c.source_commit,/^[0-9a-f]{40}$/);assert.equal(c.root_checkpoint_sha256,ROOT.checkpoint_sha256);return c;
}
export function verifyNativePins(p,{maxTransportBytes=capacity}={}){
 hostTransportBound(maxTransportBytes);
 assert.equal(p.schema,'reverse-c27-profile-native-pins-v1');assert.equal(p.root_checkpoint_sha256,ROOT.checkpoint_sha256);assert.equal(p.disk_sha256,DISK_SHA);assert.equal(p.counter_seed_sha256,SEED_SHA);assert.equal(p.logical_ram_bytes,2147483648);assert.equal(p.counter_instructions_per_tick,64);assert.equal(p.host_snapshot_transport_bytes,maxTransportBytes);return {compiled_native_profile_verified:true};
}
export function verifyCleanBoundary(r,prior,host,c,{maxTransportBytes=capacity}={}){
 const sourceCapacity=contractTransportBound(c,maxTransportBytes);
 assert.ok(['host_wall_budget','instruction_budget'].includes(r.stop_reason),'A completed clean budget is required');assert.equal(r.error,'');
 assert.equal(host.status,'process_completed');assert.equal(host.exit_code,0);assert.equal(host.trigger,null);assert.equal(host.forced_kill,false);assert.equal(host.checkpoint_bytes_modified_by_supervisor,false);
 assert.equal(prior.execution_progress_accepted,true);for(const key of ['guest_kernel_panic','guest_boot_failure','guest_terminal_failure','guest_reboot_requested'])assert.equal(prior[key],false);
 assert.equal(prior.checkpoint_sha256,r.checkpoint_sha256);assert.equal(prior.instructions,r.guest.instructions);assert.equal(prior.stop_reason,r.stop_reason);assert.equal(prior.error,r.error);assert.equal(prior.advanced_instructions,r.advanced_instructions);
 assert.ok(Number.isSafeInteger(r.guest.instructions)&&r.guest.instructions>0);assert.ok(Number.isSafeInteger(r.initial_instructions)&&r.initial_instructions>=0);assert.ok(Number.isSafeInteger(r.advanced_instructions)&&r.advanced_instructions>0);assert.equal(r.advanced_instructions,r.guest.instructions-r.initial_instructions);
 assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);assert.equal(r.guest.system_mode,true);assert.equal(r.guest.pci_bus,true);assert.match(r.guest.pc,/^0x[0-9a-f]{1,16}$/);assert.ok(Number.isInteger(r.guest.counter_subtick)&&r.guest.counter_subtick>=0&&r.guest.counter_subtick<64);
 const v=r.provenance;
 if(prior.schema==='reverse-c27-fresh-result-v1'){
  assert.equal(sourceCapacity,capacity,'The original fresh root transport is unchanged');
  assert.equal(c.source_commit,ROOT.source_commit);for(const key of ['checkpoint_sha256','report_sha256','result_sha256','process_sha256'])assert.equal(c[key],ROOT[key]);
  assert.equal(r.guest.instructions,ROOT.instructions);assert.equal(r.guest.pc,ROOT.pc);assert.equal(r.guest.counter_subtick,ROOT.phase);assert.equal(r.initial_instructions,0);
  assert.equal(v.experiment,'c27-fresh-blank-data-profile');assert.equal(v.fresh_machine,true);assert.equal(v.original_checkpoint_cpu_restored,false);assert.equal(v.prior_init_proof_reused,false);assert.equal(v.source_initrd_unchanged,true);
  assert.equal(r.first_user_instruction_verified,true);assert.equal(r.prior_user_instruction_verified,false);
  for(const key of ['first_user_instruction_verified','second_stage_init_observed','selinux_enforcing_observed','fresh_machine','source_initrd_unchanged'])assert.equal(prior[key],true);assert.equal(prior.old_cpu_or_init_proof_reused,false);
  assert.match(r.console,/init: init second stage started!/);assert.match(r.console,/audit: type=1404[^\r\n]*enforcing=1/);
 }else{
  assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
  for(const key of ['source_c27_original_init_verified','source_c27_second_stage_verified','source_c27_selinux_enforcing_verified','clean_source_boundary_verified'])assert.equal(prior[key],true);
  assert.equal(v.snapshot_round_trip_verified,true);
  if(prior.schema!=='reverse-c28-result-v1')assert.equal(v.source_profile_verified,true);
  assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.host_snapshot_transport_bytes,sourceCapacity);assert.equal(v.host_transport_policy_serialized,false);assert.equal(v.timing_seed_origin,'checkpoint');assert.equal(v.timing_seed_sha256,SEED_SHA);assert.equal(v.disk_sha256,DISK_SHA);assert.equal(prior.guest_state_modified_before_resume,false);
  assert.match(v.input_checkpoint_sha256,hashPattern);assert.match(v.input_report_sha256,hashPattern);assert.equal(prior.input_checkpoint_sha256,v.input_checkpoint_sha256);
  if(prior.schema==='reverse-c28-result-v1'){
   assert.equal(c.source_commit,'8dcb7159d22497b46b736227f4447df36c40bb23');assert.equal(v.experiment,'c28-c27-clean-continuation');assert.equal(v.input_checkpoint_sha256,ROOT.checkpoint_sha256);assert.equal(v.input_report_sha256,ROOT.report_sha256);assert.equal(r.initial_instructions,ROOT.instructions);
   for(const key of ['source_c27_original_init_verified','source_c27_second_stage_verified','source_c27_selinux_enforcing_verified'])assert.equal(v[key],true);
  }else{
   assert.equal(prior.schema,'reverse-c27-profile-budget-result-v1');assert.equal(v.experiment,'c27-profile-budget-continuation');assert.equal(v.root_checkpoint_sha256,ROOT.checkpoint_sha256);assert.equal(prior.root_checkpoint_sha256,ROOT.checkpoint_sha256);
   assert.ok(r.initial_instructions>=ROOT.instructions);assert.equal(prior.input_report_sha256,v.input_report_sha256);
  }
 }
 if(sourceCapacity!==capacity){assert.equal(prior.host_snapshot_transport_bytes,sourceCapacity);assert.equal(prior.host_transport_policy_serialized,false);}
 const outcome=analyzeGuestConsole(r.console);assert.equal(outcome.guest_boot_failure,false);assert.equal(outcome.guest_reboot_requested,false);return {clean_source_boundary_verified:true,source_c27_original_init_verified:true,source_c27_second_stage_verified:true,source_c27_selinux_enforcing_verified:true};
}
export function preflight(contract,source,{maxTransportBytes=capacity}={}){
 const c=validateContract(contract,{maxTransportBytes}),sourceCapacity=contractTransportBound(c,maxTransportBytes),state=bounded(join(source,'current.rvg'),sourceCapacity),report=bounded(join(source,'current.json'),8*1024*1024),result=bounded(join(source,'result.json'),131072),process=bounded(join(source,'process-outcome.json'),131072);
 if(existsSync(join(source,'source-commit.txt')))assert.equal(bounded(join(source,'source-commit.txt'),128).toString('utf8').trim(),c.source_commit,'Wrong source commit');
 for(const [bytes,key]of[[state,'checkpoint_sha256'],[report,'report_sha256'],[result,'result_sha256'],[process,'process_sha256']])assert.equal(sha(bytes),c[key],key);
 const r=JSON.parse(report),prior=JSON.parse(result),host=JSON.parse(process),proof=verifyCleanBoundary(r,prior,host,c,{maxTransportBytes}),binary=verifyBoardState(state,r,c.checkpoint_sha256,{maxSnapshotBytes:sourceCapacity});
 if(prior.schema==='reverse-c27-fresh-result-v1')verifyOriginalRoot(source);
 return {binary,report:r,prior,source_commit:c.source_commit,...proof};
}
export function contractForCompletedSource(source,sourceCommit,{maxTransportBytes=capacity}={}){
 hostTransportBound(maxTransportBytes);const transport=hostTransportBound(json(join(source,'current.json')).provenance?.host_snapshot_transport_bytes??capacity);assert.ok(transport<=maxTransportBytes);
 const c={schema:'reverse-c27-profile-resume-contract-v1',root_checkpoint_sha256:ROOT.checkpoint_sha256,source_commit:sourceCommit};
 if(transport!==capacity)c.host_snapshot_transport_bytes=transport;
 for(const [name,key,limit]of[['current.rvg','checkpoint_sha256',transport],['current.json','report_sha256',8*1024*1024],['result.json','result_sha256',131072],['process-outcome.json','process_sha256',131072]])c[key]=sha(bounded(join(source,name),limit));
 preflight(c,source,{maxTransportBytes});return c;
}
export function audit(contract,source,root,cycles,{maxTransportBytes=capacity}={}){
 hostTransportBound(maxTransportBytes);
 const c=validateContract(contract,{maxTransportBytes}),input=preflight(c,source,{maxTransportBytes}),pointer=json(join(root,'journal/latest.json'));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),maxTransportBytes),r=json(join(dir,'report.json')),digest=sha(state),v=r.provenance;
 assert.equal(digest,pointer.checkpoint_sha256);const a=verifyBoardState(state,r,digest,{maxSnapshotBytes:maxTransportBytes});assert.equal(a.immutable_image_sha256,input.binary.immutable_image_sha256);assert.equal(r.initial_instructions,input.report.guest.instructions);
 assert.equal(v.experiment,'c27-profile-budget-continuation');assert.equal(v.root_checkpoint_sha256,ROOT.checkpoint_sha256);assert.equal(v.input_checkpoint_sha256,c.checkpoint_sha256);assert.equal(v.input_report_sha256,c.report_sha256);assert.equal(v.snapshot_round_trip_verified,true);assert.equal(v.source_profile_verified,true);assert.equal(v.host_snapshot_transport_bytes,maxTransportBytes);assert.equal(v.host_transport_policy_serialized,false);assert.equal(v.cycle_limit,cycles);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_origin,'checkpoint');assert.equal(v.timing_seed_sha256,SEED_SHA);assert.equal(v.disk_sha256,DISK_SHA);assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const outcome=analyzeGuestConsole(r.console),result={schema:'reverse-c27-profile-budget-result-v1',checkpoint_sha256:digest,root_checkpoint_sha256:ROOT.checkpoint_sha256,input_checkpoint_sha256:c.checkpoint_sha256,input_report_sha256:c.report_sha256,source_commit:c.source_commit,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,source_c27_original_init_verified:true,source_c27_second_stage_verified:true,source_c27_selinux_enforcing_verified:true,clean_source_boundary_verified:true,host_snapshot_transport_bytes:maxTransportBytes,host_transport_policy_serialized:false,execution_progress_accepted:acceptsProgress(r,outcome)&&!outcome.guest_boot_failure,...outcome,new_disk_sha256:DISK_SHA,ram_bytes:2147483648,guest_state_modified_before_resume:false,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 linkSync(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
export async function run(contract,source,command,disk,root,{wallMs=900000,instructions=2000000000,cycles=3,onStatus,maxTransportBytes=capacity}={}){
 const watchdogMs=continuationWatchdogMs(wallMs,cycles);assert.ok(Number.isSafeInteger(instructions)&&instructions>0&&instructions<=2000000000);assert.ok(!existsSync(root),'Output already exists');const c=validateContract(contract,{maxTransportBytes}),input=preflight(c,source,{maxTransportBytes});
 const pin=spawnSync(command,['--pins'],{encoding:'utf8',timeout:30000,maxBuffer:65536});assert.ifError(pin.error);assert.equal(pin.status,0,pin.stderr);const pins=JSON.parse(pin.stdout),verified=verifyNativePins(pins,{maxTransportBytes});mkdirSync(root);write(join(root,'native-pins.json'),JSON.stringify(pins,null,2)+'\n');write(join(root,'native-pins-verification.json'),JSON.stringify(verified)+'\n');write(join(root,'input-contract.json'),JSON.stringify(c,null,2)+'\n');
 const g=input.report.guest,host=await superviseNativeCycle({command,args:[join(source,'current.rvg'),join(source,'current.json'),disk,join(root,'journal'),String(wallMs),String(instructions),String(cycles),c.checkpoint_sha256,c.report_sha256,String(g.instructions),g.pc,String(g.counter_subtick)],outputDirectory:root,initialInstructions:g.instructions,startupMs:600000,noProgressMs:90000,wallMs:watchdogMs,graceMs:30000,onStatus});
 const result=existsSync(join(root,'journal/latest.json'))?audit(c,source,root,cycles,{maxTransportBytes}):null;return {process:host,result};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const[mode,contractFile,source,command,disk,output,wall,limit,count]=process.argv.slice(2);assert.ok(['preflight','run','audit'].includes(mode)&&contractFile&&source);const c=json(contractFile);
 if(mode==='preflight'){const{binary,report,prior,...r}=preflight(c,source);console.log(JSON.stringify(r,null,2));}
 else if(mode==='audit')console.log(JSON.stringify(audit(c,source,command,Number(disk)),null,2));
 else{const r=await run(c,source,command,disk,output,{wallMs:wall===undefined?900000:Number(wall),instructions:limit===undefined?2000000000:Number(limit),cycles:count===undefined?3:Number(count),onStatus:s=>process.stderr.write('REVERSE_C27_PROFILE_SUPERVISOR '+JSON.stringify(s)+'\n')});console.log(JSON.stringify(r,null,2));if(r.process.status!=='process_completed'||!r.result?.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
