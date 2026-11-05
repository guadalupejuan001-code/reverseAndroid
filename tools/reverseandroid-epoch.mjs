// Admission for an explicitly different boot epoch. Historical C27/C46 source
// checks remain unchanged. Original RVG audit, CPU roundtrip, kernel observer,
// framework checks, scanout decoder and C46 milestone are still required.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,existsSync,mkdirSync,copyFileSync as copy,linkSync,openSync,closeSync,statSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {spawnSync} from 'node:child_process';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {hash} from './reverseandroid-lights-profile.mjs';
import {superviseNativeCycle} from './android_supervised_cycle.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {evaluateFrameworkEvidence} from './android_framework_evidence.mjs';
import {summarizeZygoteProgress} from './android_c42_zygote_progress.mjs';
import {milestone} from './android_c46_continue.mjs';
import {loadEpochInputPlan,verifyEpochInputEvent} from './reverseandroid-epoch-input.mjs';
const KERNEL='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8';
const DISK='4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e';
const INITRD='63b5ac6ba9d7f1b1a50aead170299a67bae5deb17438fafe61329448f30b8331';
const SEED='699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb';
const PARENT='7ecff7c2d460c582eeb3cbdf47fdd6686790cff2add75cd6c7437af7c9b9c3ec';
const ROOT='a39f886c126c9695024c29d5cef315d90739d39415da521e1f5e4f4e42228288';
const IMAGE='dded70c4020cef2ccdadb9ac8bb885f3af206eef8c936798d6ec8ced0fde60fb';
const CAP=2147483648;
const json=p=>JSON.parse(read(p,'utf8')),save=(p,x)=>write(p,JSON.stringify(x,null,2)+'\n',{flag:'wx'});
function cleanHost(h){assert.equal(h.status,'process_completed');assert.equal(h.exit_code,0);assert.equal(h.trigger,null);assert.equal(h.forced_kill,false);assert.equal(h.checkpoint_bytes_modified_by_supervisor,false);}
export function verifyEpochState(state,report,digest,epoch){
  const a=auditGkiRvg(state,KERNEL,digest,{maxSnapshotBytes:CAP}),g=report.guest;
  assert.equal(a.logical_ram_bytes,2147483648);assert.equal(a.classical_override_limit,'0');
  assert.equal(a.immutable_image_sha256,IMAGE);assert.equal(a.bootargs_sha256,'4ba3144cf672697172712c073c3f78a61a8855c1969285882d1bdd8580b77407');
  assert.equal(a.rvi_version,'RVI3');assert.equal(a.external_disk_sha256,DISK);assert.equal(a.external_disk_bytes,10888413184);
  if(epoch.immutable_image_sha256)assert.equal(a.immutable_image_sha256,epoch.immutable_image_sha256);
  if(epoch.bootargs_sha256)assert.equal(a.bootargs_sha256,epoch.bootargs_sha256);
  for(const [aKey,gKey] of [['instructions','instructions'],['native_pc','pc'],['native_sp','sp']])assert.equal(a[aKey],g[gKey]);
  assert.equal(a.native_halted,0);assert.equal(a.native_exit_code,0);assert.equal(g.halted,false);assert.equal(g.system_mode,true);assert.equal(g.pci_bus,true);
  const field=i=>Number(state.readBigUInt64LE(8+i*8)),at=8+(state[3]===51?44:43)*8,kn=Number(state.readBigUInt64LE(at+8)),dt=Number(state.readBigUInt64LE(at+16)),rd=Number(state.readBigUInt64LE(at+24));
  assert.equal(hash(state.subarray(at+48+kn+dt,at+48+kn+dt+rd)),INITRD);
  const machine=at+field(39)+field(40)+field(41),clock=machine+field(42)-64;
  assert.equal(state.readBigUInt64LE(machine),7n);assert.equal(state.readBigUInt64LE(machine+8),1n);
  assert.equal(state.readBigUInt64LE(clock),2n);assert.equal(hash(state.subarray(clock+8,clock+40)),SEED);
  assert.equal(state.readBigUInt64LE(clock+48),64n);assert.equal(Number(state.readBigUInt64LE(clock+56)),g.counter_subtick);
  assert.equal(g.counter_instructions_per_tick,64);assert.equal(g.logical_ram_bytes,2147483648);
  assert.ok(g.counter_subtick>=0&&g.counter_subtick<64);return a;
}
export function finalizeEpochRoot(root,profileFile,parentContractFile){
  assert.ok(!existsSync(join(root,'epoch.json')));
  const proof=json(join(root,'power-cycle.json')),profile=json(profileFile),parent=json(parentContractFile);
  assert.equal(proof.schema,'reverseandroid-hardware-power-cycle-v1');assert.equal(proof.parent_checkpoint_sha256,PARENT);assert.equal(parent.checkpoint_sha256,PARENT);
  assert.equal(proof.instructions,0);assert.equal(proof.parent_instructions,88895950532);assert.equal(proof.checkpoint_sha256,ROOT);
  for(const key of ['parent_original_roundtrip_verified','fresh_original_roundtrip_verified','effective_persistent_drive_unchanged','volatile_memory_reset','counter_seed_preserved','source_cpio_payload_unchanged','bootargs_unchanged'])assert.equal(proof[key],true,key);
  assert.equal(proof.prior_init_proof_reused,false);assert.equal(proof.live_android_properties_written,false);assert.equal(proof.derived_initrd_sha256,INITRD);
  assert.equal(profile.derived_initrd_sha256,INITRD);assert.deepEqual(profile.added_bootconfig,{'androidboot.vsock_lights_port':'6900'});
  const state=read(join(root,'current.rvg')),a=verifyEpochState(state,proof,proof.checkpoint_sha256,{});
  const epoch={schema:'reverseandroid-boot-epoch-v1',cycle:'C76',parent_checkpoint_sha256:PARENT,parent_instructions:88895950532,
    root_checkpoint_sha256:proof.checkpoint_sha256,power_cycle_proof_sha256:hash(read(join(root,'power-cycle.json'))),
    immutable_image_sha256:a.immutable_image_sha256,bootargs_sha256:a.bootargs_sha256,initrd_sha256:INITRD,original_disk_sha256:DISK,
    persistent_drive_delta_sha256:proof.persistent_drive_delta_sha256,live_android_properties_written:false,
    initialization_is_not_boot_acceptance:true};
  copy(profileFile,join(root,'boot-profile.json'));copy(parentContractFile,join(root,'parent-output-contract.json'));save(join(root,'epoch.json'),epoch);
  const c={schema:'reverseandroid-epoch-source-v1',checkpoint_sha256:proof.checkpoint_sha256,report_file:'power-cycle.json',report_sha256:hash(read(join(root,'power-cycle.json'))),
    epoch_sha256:hash(read(join(root,'epoch.json'))),instructions:0,epoch_original_init_verified:false,initialized_hardware_root:true};
  save(join(root,'output-contract.json'),c);return c;
}
export function preflightEpoch(source){
  const c=json(join(source,'output-contract.json')),epoch=json(join(source,'epoch.json'));
  assert.equal(c.schema,'reverseandroid-epoch-source-v1');assert.equal(epoch.schema,'reverseandroid-boot-epoch-v1');assert.equal(epoch.parent_checkpoint_sha256,PARENT);
  assert.equal(epoch.root_checkpoint_sha256,ROOT);assert.equal(c.epoch_sha256,'bc5e6f91cc5befb13ba8f8e96ffb74cad22edda25586324a0f469e1b26fd5e75');
  assert.equal(hash(read(join(source,'epoch.json'))),c.epoch_sha256);assert.ok(['power-cycle.json','current.json'].includes(c.report_file));
  assert.equal(hash(read(join(source,c.report_file))),c.report_sha256);assert.ok(statSync(join(source,'current.rvg')).size<=CAP);
  const r=json(join(source,c.report_file));verifyEpochState(read(join(source,'current.rvg')),r,c.checkpoint_sha256,epoch);
  assert.equal(r.checkpoint_sha256,c.checkpoint_sha256);assert.equal(r.guest.instructions,c.instructions);
  if(c.initialized_hardware_root){assert.equal(c.checkpoint_sha256,epoch.root_checkpoint_sha256);assert.equal(c.instructions,0);assert.equal(c.epoch_original_init_verified,false);assert.equal(c.report_sha256,epoch.power_cycle_proof_sha256);}
  else{
    for(const [file,key] of [['result.json','result_sha256'],['process-outcome.json','process_sha256']])assert.equal(hash(read(join(source,file))),c[key]);
    cleanHost(json(join(source,'process-outcome.json')));const result=json(join(source,'result.json'));
    assert.equal(result.execution_progress_accepted,true);assert.equal(result.checkpoint_sha256,c.checkpoint_sha256);assert.equal(result.instructions,c.instructions);
    assert.equal(result.epoch_original_init_verified,c.epoch_original_init_verified);assert.equal(result.input_checkpoint_sha256,r.provenance.input_checkpoint_sha256);
    assert.equal(r.provenance.epoch_root_checkpoint_sha256,epoch.root_checkpoint_sha256);assert.equal(r.provenance.snapshot_round_trip_verified,true);
    assert.ok(['instruction_budget','host_wall_budget'].includes(r.stop_reason));assert.equal(r.error,'');assert.equal(r.advanced_instructions,r.guest.instructions-r.initial_instructions);
    assert.equal(result.epoch_original_init_verified,r.first_user_instruction_verified||r.provenance.source_epoch_init_verified);
    assert.equal(analyzeGuestConsole(r.console).guest_boot_failure,false);
    const boundary=json(join(source,'boundary-summary.json')),gate=json(join(source,'c46-progress.json'));
    assert.equal(boundary.clean_completed,true);assert.equal(gate.source_boundary_verified,true);
    for(const item of [boundary,gate]){assert.equal(item.checkpoint_sha256,c.checkpoint_sha256);assert.equal(item.instructions,c.instructions);}
  }
  return {contract:c,epoch,report:r};
}
function observe(command,args,path,allowed=[0]){
  const out=openSync(path,'wx'),err=openSync(path+'.log','wx');let r;
  try{r=spawnSync(command,args,{stdio:['ignore',out,err],timeout:600000});}finally{closeSync(out);closeSync(err);}
  assert.ifError(r.error);assert.ok(allowed.includes(r.status),`Observation failed: ${path} (${r.status})`);return r.status;
}
export async function runEpochStage(source,output,native,disk,frame,commit,budget=1000000000,inputPlanFile=null,wallMs=900000){
  assert.match(commit,/^[0-9a-f]{40}$/);assert.ok(Number.isSafeInteger(budget)&&budget>0&&budget<=2000000000);assert.ok(!existsSync(output));
  assert.ok(Number.isSafeInteger(wallMs)&&wallMs>0&&wallMs<=900000,'Invalid explicit host wall budget');
  const input=preflightEpoch(source);
  const uart=loadEpochInputPlan(inputPlanFile,input.contract,{bootVerified:!input.contract.initialized_hardware_root&&json(join(source,'c46-progress.json')).android_boot_completed_verified===true,
    frame:existsSync(join(source,'checkpoint-frame.json'))?json(join(source,'checkpoint-frame.json')):null});
  global.gc?.();mkdirSync(output);mkdirSync(join(output,'native'));
  copy(join(source,'epoch.json'),join(output,'epoch.json'));copy(join(source,'boot-profile.json'),join(output,'boot-profile.json'));
  save(join(output,'input-contract.json'),input.contract);
  assert.ok(statSync(native).isFile()&&statSync(native).size<=128*1024*1024,'Invalid native executable extent');
  save(join(output,'host-policy.json'),{schema:'reverseandroid-epoch-host-policy-v1',controller_commit:commit,native_binary_sha256:hash(read(native)),instruction_budget:budget,host_wall_ms:wallMs,
    logical_RAM_clock_seed_cadence_IRQ_and_codecs_modified:false,external_input_plan_present:uart!==null});
  if(uart){write(join(output,'applied-input.bin'),uart.data,{flag:'wx'});save(join(output,'applied-input-plan.json'),{...uart.plan,input_file:'applied-input.bin'});}
  const journal=join(output,'native','journal');
  const c=input.contract,args=[join(source,'current.rvg'),join(source,c.report_file),disk,journal,String(wallMs),String(budget),c.checkpoint_sha256,c.report_sha256,input.epoch.root_checkpoint_sha256,String(c.epoch_original_init_verified)];
  if(uart)args.push(join(output,'applied-input.bin'),uart.plan.input_sha256);
  const host=await superviseNativeCycle({command:native,args,
    outputDirectory:output,initialInstructions:c.instructions,startupMs:600000,noProgressMs:90000,wallMs:wallMs+600000,graceMs:30000,
    onStatus:s=>process.stderr.write('EPOCH_SUPERVISOR '+JSON.stringify(s)+'\n')});
  cleanHost(host);const pointer=json(join(journal,'latest.json'));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
  const generation=join(journal,pointer.generation);linkSync(join(generation,'checkpoint.rvg'),join(output,'current.rvg'));copy(join(generation,'report.json'),join(output,'current.json'));
  const r=json(join(output,'current.json')),v=r.provenance,digest=pointer.checkpoint_sha256;
  verifyEpochState(read(join(output,'current.rvg')),r,digest,input.epoch);global.gc?.();
  assert.equal(r.checkpoint_sha256,digest);assert.equal(r.initial_instructions,c.instructions);assert.equal(r.advanced_instructions,r.guest.instructions-c.instructions);assert.ok(r.advanced_instructions>0);
  assert.ok(['instruction_budget','host_wall_budget'].includes(r.stop_reason));assert.equal(r.error,'');assert.equal(r.guest.waiting,false);
  assert.equal(v.experiment,'c76-lights-port-epoch');assert.equal(v.input_checkpoint_sha256,c.checkpoint_sha256);assert.equal(v.input_report_sha256,c.report_sha256);
  assert.equal(v.epoch_root_checkpoint_sha256,input.epoch.root_checkpoint_sha256);assert.equal(v.snapshot_round_trip_verified,true);assert.equal(v.source_epoch_init_verified,c.epoch_original_init_verified);
  assert.equal(v.source_cpu_modified_before_resume,false);assert.equal(v.new_uart_input_bytes,uart?.data.length??0);assert.equal(v.live_android_properties_written,false);
  verifyEpochInputEvent(v.external_uart_input,uart,input.report.guest);
  const outcome=analyzeGuestConsole(r.console);assert.equal(outcome.guest_boot_failure,false);assert.equal(outcome.guest_reboot_requested,false);
  const result={schema:'reverseandroid-epoch-result-v1',checkpoint_sha256:digest,input_checkpoint_sha256:c.checkpoint_sha256,instructions:r.guest.instructions,advanced_instructions:r.advanced_instructions,
    epoch_original_init_verified:r.first_user_instruction_verified||c.epoch_original_init_verified,execution_progress_accepted:true,...outcome,
    ...(uart?{external_uart_input:v.external_uart_input,input_purpose:uart.plan.purpose}:{})};
  save(join(output,'result.json'),result);
  const next={schema:c.schema,checkpoint_sha256:digest,report_file:'current.json',report_sha256:hash(read(join(output,'current.json'))),epoch_sha256:c.epoch_sha256,
    instructions:r.guest.instructions,epoch_original_init_verified:result.epoch_original_init_verified,initialized_hardware_root:false,
    result_sha256:hash(read(join(output,'result.json'))),process_sha256:hash(read(join(output,'process-outcome.json'))),source_commit:commit};
  save(join(output,'output-contract.json'),next);write(join(output,'console-new.txt'),r.console);write(join(output,'source-commit.txt'),commit+'\n');
  observe(frame,[join(output,'current.rvg'),disk,digest,DISK,'10888413184',join(output,'checkpoint-frame.ppm')],join(output,'checkpoint-frame.json'),[0,3]);
  observe(process.execPath,['--max-old-space-size=5000',new URL('./android_kernel_observer.mjs',import.meta.url).pathname,join(output,'current.rvg'),digest,String(CAP),'--live-properties','--zygote-probe'],join(output,'kernel-observation.json'));
  const runtime=evaluateFrameworkEvidence({checkpointSha:digest,report:r,result,host,kernel:json(join(output,'kernel-observation.json')),frame:json(join(output,'checkpoint-frame.json')),
    ppm:existsSync(join(output,'checkpoint-frame.ppm'))?read(join(output,'checkpoint-frame.ppm')):undefined});
  save(join(output,'runtime-evidence.json'),runtime);
  const progress=summarizeZygoteProgress(json(join(output,'kernel-observation.json')));save(join(output,'c42-progress.json'),progress);
  const boundary={clean_completed:true,checkpoint_sha256:digest,instructions:r.guest.instructions,original_rvg_roundtrip_verified:true,epoch_profile_verified:true};save(join(output,'boundary-summary.json'),boundary);
  const final=milestone(progress,runtime,boundary,next);
  if(final.android_boot_completed_verified)assert.equal(result.epoch_original_init_verified,true,'The new epoch must independently execute original init');
  save(join(output,'c46-progress.json'),final);
  if(process.env.GITHUB_OUTPUT)write(process.env.GITHUB_OUTPUT,`boot=${final.android_boot_completed_verified}\nready=${final.ready_for_visual_review}\n`,{flag:'a'});
  return final;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
  const [mode,...args]=process.argv.slice(2);
  const result=mode==='finalize-root'?finalizeEpochRoot(...args):mode==='preflight'?preflightEpoch(...args):mode==='run'?await runEpochStage(...args.slice(0,6),args[6]===undefined?1000000000:Number(args[6]),args[7]&&args[7]!=='-'?args[7]:null,args[8]===undefined?900000:Number(args[8])):assert.fail('Expected finalize-root, preflight or run');
  console.log(JSON.stringify(result,null,2));
}catch(error){console.error(error.stack);process.exitCode=1;}}
