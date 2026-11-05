// Fixed-board continuation of clean authenticated budgets. Never accepts an ISA stop or a guest failure.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,statSync,mkdirSync,copyFileSync as copy,linkSync as link,existsSync} from 'node:fs';
import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';import {spawnSync} from 'node:child_process';
import {verifyProfileState,DISK_SHA,SEED_SHA} from './android_profile_state.mjs';
import {sha} from './android_c13_avb.mjs';import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {acceptsProgress} from './android_c15_result.mjs';import {superviseNativeCycle} from './android_supervised_cycle.mjs';
const capacity=1024*1024*1024,hashPattern=/^[0-9a-f]{64}$/;
const bounded=(path,limit)=>{assert.ok(statSync(path).size<=limit,'Oversized evidence');return read(path);};
const json=path=>JSON.parse(bounded(path,8*1024*1024));
export function continuationWatchdogMs(wallMs=900000,cycles=3){
 assert.ok(Number.isSafeInteger(wallMs)&&wallMs>0&&wallMs<=900000);assert.ok(Number.isInteger(cycles)&&cycles>=1&&cycles<=4);
 // Native wall budgets already include their periodic checkpoint time.
 // Reserve startup and final publication once, rather than once per cycle.
 const total=600000+cycles*wallMs+120000;assert.ok(total<=3600000,'Continuation budgets exceed the one-hour supervisor bound');return total;
}
export function validateContract(c){
 assert.equal(c.schema,'reverse-profile-resume-contract-v1');
 for(const key of ['checkpoint_sha256','report_sha256','result_sha256','process_sha256'])assert.match(c[key],hashPattern,key);
 assert.match(c.source_commit,/^[0-9a-f]{40}$/);return c;
}
export function verifyNativePins(p){
 assert.equal(p.schema,'reverse-profile-native-pins-v1');assert.equal(p.disk_sha256,DISK_SHA);assert.equal(p.counter_seed_sha256,SEED_SHA);assert.equal(p.logical_ram_bytes,2147483648);assert.equal(p.counter_instructions_per_tick,64);assert.equal(p.host_snapshot_transport_bytes,capacity);return {compiled_native_profile_verified:true};
}
export function verifyCleanBoundary(r,prior,host){
 assert.ok(['host_wall_budget','instruction_budget'].includes(r.stop_reason),'Only a clean budget boundary can resume');assert.equal(r.error,'');
 assert.equal(host.status,'process_completed');assert.equal(host.exit_code,0);assert.equal(prior.execution_progress_accepted,true);
 for(const key of ['guest_kernel_panic','guest_boot_failure','guest_terminal_failure','guest_reboot_requested'])assert.equal(prior[key],false);
 for(const key of ['source_c21_original_init_verified','source_c21_second_stage_verified','source_c21_selinux_enforcing_verified'])assert.equal(prior[key],true);
 assert.equal(r.guest.halted,false);assert.equal(r.guest.waiting,false);assert.equal(r.guest.system_mode,true);assert.equal(r.guest.pci_bus,true);
 assert.equal(r.provenance.original_state_modified,false);assert.equal(r.provenance.ram_profile_changed,false);assert.equal(r.provenance.snapshot_round_trip_verified,true);
 assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 assert.equal(prior.checkpoint_sha256,r.checkpoint_sha256);assert.equal(prior.instructions,r.guest.instructions);assert.equal(prior.stop_reason,r.stop_reason);assert.equal(prior.error,r.error);
 assert.ok(Number.isSafeInteger(r.guest.instructions)&&r.guest.instructions>0);assert.match(r.guest.pc,/^0x[0-9a-f]{1,16}$/);assert.ok(Number.isInteger(r.guest.counter_subtick)&&r.guest.counter_subtick>=0&&r.guest.counter_subtick<64);
 const outcome=analyzeGuestConsole(r.console);assert.equal(outcome.guest_boot_failure,false);assert.equal(outcome.guest_reboot_requested,false);return outcome;
}
export function preflight(contract,source){
 const c=validateContract(contract),state=bounded(join(source,'current.rvg'),capacity),report=bounded(join(source,'current.json'),8*1024*1024),result=bounded(join(source,'result.json'),131072),process=bounded(join(source,'process-outcome.json'),131072);
 for(const [bytes,key] of [[state,'checkpoint_sha256'],[report,'report_sha256'],[result,'result_sha256'],[process,'process_sha256']])assert.equal(sha(bytes),c[key],key);
 const r=JSON.parse(report),prior=JSON.parse(result),host=JSON.parse(process);verifyCleanBoundary(r,prior,host);
 const a=verifyProfileState(state,r,c.checkpoint_sha256,{maxSnapshotBytes:capacity});assert.equal(a.immutable_image_sha256,'f4708c601115d0c37b1dc0c73b5efb5337fd14decc2eb5de9d593dc2ae86356d');
 return {binary:a,report:r,prior,source_commit:c.source_commit,clean_source_boundary_verified:true};
}
export function audit(contract,source,root,cycles){
 assert.ok(Number.isInteger(cycles)&&cycles>=1&&cycles<=4);const c=validateContract(contract),input=preflight(c,source),pointer=json(join(root,'journal/latest.json'));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
 const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),capacity),r=json(join(dir,'report.json')),digest=sha(state),v=r.provenance;
 assert.equal(digest,pointer.checkpoint_sha256);const a=verifyProfileState(state,r,digest,{maxSnapshotBytes:capacity});assert.equal(a.immutable_image_sha256,input.binary.immutable_image_sha256);assert.equal(r.initial_instructions,input.report.guest.instructions);
 assert.equal(v.experiment,'profile-budget-continuation');assert.equal(v.input_checkpoint_sha256,c.checkpoint_sha256);assert.equal(v.input_report_sha256,c.report_sha256);assert.equal(v.snapshot_round_trip_verified,true);assert.equal(v.source_profile_verified,true);assert.equal(v.host_snapshot_transport_bytes,capacity);assert.equal(v.host_transport_policy_serialized,false);assert.equal(v.cycle_limit,cycles);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.timing_seed_sha256,SEED_SHA);assert.equal(v.disk_sha256,DISK_SHA);
 assert.equal(r.first_user_instruction_verified,false);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 const outcome=analyzeGuestConsole(r.console),result={schema:'reverse-profile-budget-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:c.checkpoint_sha256,input_report_sha256:c.report_sha256,source_commit:c.source_commit,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,pc:r.guest.pc,stop_reason:r.stop_reason,error:r.error,source_c21_original_init_verified:true,source_c21_second_stage_verified:true,source_c21_selinux_enforcing_verified:true,clean_source_boundary_verified:true,host_snapshot_transport_bytes:capacity,host_transport_policy_serialized:false,execution_progress_accepted:acceptsProgress(r,outcome)&&!outcome.guest_boot_failure,...outcome,new_disk_sha256:DISK_SHA,ram_bytes:2147483648,guest_state_modified_before_resume:false,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
 link(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));write(join(root,'console-new.txt'),r.console);write(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');return result;
}
export async function run(contract,source,command,disk,root,{wallMs=900000,instructions=2000000000,cycles=3,onStatus}={}){
 assert.ok(Number.isSafeInteger(wallMs)&&wallMs>0&&wallMs<=900000);assert.ok(Number.isSafeInteger(instructions)&&instructions>0&&instructions<=2000000000);assert.ok(Number.isInteger(cycles)&&cycles>=1&&cycles<=4);
 const watchdogMs=continuationWatchdogMs(wallMs,cycles);assert.ok(!existsSync(root),'Output already exists');const c=validateContract(contract),input=preflight(c,source);
 const pin=spawnSync(command,['--pins'],{encoding:'utf8',timeout:30000,maxBuffer:65536});assert.ifError(pin.error);assert.equal(pin.status,0,pin.stderr);const pins=JSON.parse(pin.stdout),verified=verifyNativePins(pins);
 mkdirSync(root);write(join(root,'native-pins.json'),JSON.stringify(pins,null,2)+'\n');write(join(root,'native-pins-verification.json'),JSON.stringify(verified)+'\n');write(join(root,'input-contract.json'),JSON.stringify(c,null,2)+'\n');
 const r=input.report.guest;
 const outcome=await superviseNativeCycle({command,args:[join(source,'current.rvg'),join(source,'current.json'),disk,join(root,'journal'),String(wallMs),String(instructions),String(cycles),c.checkpoint_sha256,c.report_sha256,String(r.instructions),r.pc,String(r.counter_subtick)],outputDirectory:root,initialInstructions:r.instructions,startupMs:600000,noProgressMs:90000,wallMs:watchdogMs,graceMs:30000,onStatus});
 const result=existsSync(join(root,'journal/latest.json'))?audit(c,source,root,cycles):null;
 return {process:outcome,result};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [mode,contractFile,source,command,disk,output,wall,limit,count]=process.argv.slice(2);assert.ok(['preflight','run','audit'].includes(mode)&&contractFile&&source,'Expected preflight|run|audit contract.json source [native disk output wall-ms instructions cycles]');const c=json(contractFile);
 if(mode==='preflight'){const {binary,report,prior,...r}=preflight(c,source);console.log(JSON.stringify(r,null,2));}
 else if(mode==='audit')console.log(JSON.stringify(audit(c,source,command,Number(disk)),null,2));
 else{assert.ok(command&&disk&&output);const r=await run(c,source,command,disk,output,{wallMs:wall===undefined?900000:Number(wall),instructions:limit===undefined?2000000000:Number(limit),cycles:count===undefined?3:Number(count),onStatus:s=>process.stderr.write('REVERSE_PROFILE_SUPERVISOR '+JSON.stringify(s)+'\n')});console.log(JSON.stringify(r,null,2));if(r.process.status!=='process_completed'||!r.result?.execution_progress_accepted)process.exitCode=2;}
}catch(e){console.error(e.stack);process.exitCode=1;}}
