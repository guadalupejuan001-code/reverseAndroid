// Combine authenticated, read-only evidence from one exact Android checkpoint.
// Policy fixtures are not boot results. Exported pixels still need visual review.
import assert from 'node:assert/strict';
import {sha} from './android_c13_avb.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
const KERNEL='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8';
// Linux v6.12 include/linux/sched.h: __state describes scheduling; exit_state
// separately describes death. TASK_FREEZABLE (0x2000) and TASK_WAKEKILL (0x100)
// can accompany ordinary interruptible/uninterruptible userspace sleeps.
export function isLiveUserspaceTask(task){
 if(!task||!Number.isSafeInteger(task.pid)||task.pid<=0||task.exit_state!==0||task.has_user_mm!==true||!Number.isInteger(task.state)||task.state<0||task.state>=0x10000)return false;
 if(task.state===0)return true;
 const sleep=task.state&~0x2100;
 return sleep===1||sleep===2;
}
// ART can change Zygote's kernel thread name to main while argv remains
// zygote64. Require fresh argv/executable/parent evidence and PID1's own live
// service PID, rather than treating every main-named task as a Zygote.
export function verifiedMainZygoteProcesses(kernel){
 const z=kernel?.zygote_launch,p=kernel?.live_boot_property;
 if(kernel?.status!=='read_only_kernel_observation'||kernel.input_unchanged!==true||
    !/^[0-9a-f]{64}$/.test(kernel.checkpoint_sha256??'')||z?.schema!=='reverse-c40-zygote-argv-v1'||
    z.read_only!==true||z.source_checkpoint_sha256!==kernel.checkpoint_sha256)return [];
 const init=(kernel.tasks??[]).filter(t=>t.pid===1&&t.name==='init');
 if(init.length!==1||!isLiveUserspaceTask(init[0])||p?.schema!=='reverse-live-pid1-property-v1'||
    p.read_only!==true||p.pid!==1||p.task!==init[0].task||p.read_error||
    typeof p.mm!=='string'||p.mm.length>18||!/^0x[0-9a-f]+$/.test(p.mm)||BigInt(p.mm)<0xffff000000000000n||
    !Number.isSafeInteger(p.vma_count)||p.vma_count<=0||p.vma_count>8192||
    !Number.isSafeInteger(p.property_mappings)||p.property_mappings<2||p.property_mappings>p.vma_count)return [];
 const observed=name=>{
  const matches=(p.boot_chain_property_observations??[]).filter(o=>o.name===name&&o.status==='property_observed');
  if(!matches.length||!matches.every(o=>typeof o.value==='string'&&Number.isSafeInteger(o.serial)&&o.serial>=0&&o.serial<=0xffffffff&&
     !(o.serial&1)&&!(o.serial&0x10000)&&(o.serial>>>24)===Buffer.byteLength(o.value)&&o.vma_shared===true&&o.live_pid1_mapping_verified===true&&
     /^\/dev\/__properties__\/u:object_r:[a-zA-Z0-9_]+:s0$/.test(o.file)))return null;
  const values=new Set(matches.map(o=>o.value));return values.size===1?[...values][0]:null;
 };
 const servicePid=observed('init.svc_debug_pid.zygote');
 if(observed('init.svc.zygote')!=='running'||!/^[1-9][0-9]{0,9}$/.test(servicePid??''))return [];
 const pid=Number(servicePid);if(!Number.isSafeInteger(pid))return [];
 const candidates=(z.processes??[]).filter(process=>process.read_only===true&&process.comm==='main'&&process.pid===pid&&
   isLiveUserspaceTask(process)&&process.argv_verified===true&&process.exe_path==='/system/bin/app_process64'&&process.parent_pid===1&&
   Array.isArray(process.argv)&&process.argv.length===1&&process.argv[0]==='zygote64');
 if(candidates.length!==1)return [];
 const process=candidates[0],tasks=(kernel.tasks??[]).filter(t=>t.pid===pid&&t.name===process.comm&&
   t.state===process.state&&t.exit_state===process.exit_state&&t.has_user_mm===process.has_user_mm&&isLiveUserspaceTask(t));
 return tasks.length===1?[{process,task:tasks[0]}]:[];
}
export function evaluateFrameworkEvidence({checkpointSha,report,result,host,kernel,frame,ppm}){
 assert.match(checkpointSha,/^[0-9a-f]{64}$/);
 assert.equal(report.checkpoint_sha256,checkpointSha);assert.equal(result.checkpoint_sha256,checkpointSha);
 assert.equal(result.instructions,report.guest.instructions);
 assert.equal(kernel.status,'read_only_kernel_observation');assert.equal(kernel.checkpoint_sha256,checkpointSha);assert.equal(kernel.kernel_sha256,KERNEL);assert.equal(kernel.input_unchanged,true);
 assert.equal(kernel.instructions,report.guest.instructions);assert.equal(kernel.pc,report.guest.pc);assert.equal(kernel.ticks,String(report.guest.virtual_ticks));assert.equal(kernel.exceptions,String(report.guest.exceptions));
 assert.equal(frame.schema,'reverse-snapshot-scanout-v1');assert.equal(frame.source_checkpoint_sha256,checkpointSha);assert.equal(frame.cpu_instructions_executed,0);assert.equal(frame.original_snapshot_unchanged,true);
 const reasons=[],need=(ok,reason)=>{if(!ok)reasons.push(reason);};
 need(host.status==='process_completed'&&host.exit_code===0&&host.trigger===null&&!host.forced_kill&&host.checkpoint_bytes_modified_by_supervisor===false,'native_process_not_cleanly_completed');
 const outcome=analyzeGuestConsole(report.console);
 need(result.execution_progress_accepted===true&&!outcome.guest_boot_failure&&!outcome.guest_reboot_requested&&['guest_kernel_panic','guest_boot_failure','guest_terminal_failure','guest_reboot_requested'].every(key=>result[key]===false),'guest_progress_not_accepted');
 need(report.error===''&&['host_wall_budget','instruction_budget'].includes(report.stop_reason)&&!report.guest.halted&&!report.guest.waiting,'checkpoint_not_a_clean_budget');
 const security=kernel.live_kernel_security;
 need(security?.read_only===true&&security.btf_layout_verified===true&&security.selinux_initialized===true&&security.selinux_enforcing===true&&security.policy_present===true&&!security.read_error,'live_selinux_enforcement_unverified');
 const processEvidence=[];
 for(const name of ['init','zygote64','system_server','surfaceflinger']){
  const found=kernel.tasks.filter(t=>t.name===name&&(name!=='init'||t.pid===1));
  if(name==='zygote64')found.push(...verifiedMainZygoteProcesses(kernel).map(item=>item.task));
  const alive=found.length===1&&isLiveUserspaceTask(found[0]);
  need(alive,'live_process_missing:'+name);const evidence={name,verified:alive,pid:alive?found[0].pid:null};
  if(alive&&name==='zygote64'&&found[0].name==='main'){evidence.observed_comm='main';evidence.identity_basis='live_argv_executable_parent_and_pid1_service_pid';}
  processEvidence.push(evidence);
 }
 const p=kernel.live_boot_property,init=kernel.tasks.find(t=>t.pid===1);
 const validProperty=o=>o.status==='property_observed'&&o.name==='sys.boot_completed'&&o.value==='1'&&Number.isSafeInteger(o.serial)&&o.serial>=0&&o.serial<=0xffffffff&&!(o.serial&1)&&o.serial>>>24===1&&!(o.serial&0x10000)&&o.vma_shared===true&&o.live_pid1_mapping_verified===true&&/^\/dev\/__properties__\/u:object_r:[a-zA-Z0-9_]+:s0$/.test(o.file);
 need(p?.schema==='reverse-live-pid1-property-v1'&&p.read_only===true&&p.pid===1&&p.task===init?.task&&p.live_property_mapping_verified===true&&p.boot_completed_property_live===true&&p.ambiguous_property_values===false&&p.observations?.length>0&&p.observations.every(validProperty),'live_sys_boot_completed_unverified');
 let scanout=false;
 if(frame.status==='scanout_exported'){
  assert.equal(frame.instructions,report.guest.instructions);assert.equal(frame.pc,report.guest.pc);assert.equal(frame.frame_counter,report.guest.display_frame);
  assert.equal(frame.width,report.guest.display_width);assert.equal(frame.height,report.guest.display_height);assert.ok(frame.width>0&&frame.width<=1280&&frame.height>0&&frame.height<=1024);
  assert.ok(Buffer.isBuffer(ppm));const header=Buffer.from(`P6\n${frame.width} ${frame.height}\n255\n`);
  assert.equal(ppm.length,header.length+frame.width*frame.height*3);assert.ok(ppm.subarray(0,header.length).equals(header));assert.equal(sha(ppm),frame.ppm_sha256);assert.equal(sha(ppm.subarray(header.length)),frame.rgb_sha256);scanout=true;
 }else{assert.equal(frame.status,'no_scanout');assert.equal(ppm,undefined);}
 const framework=reasons.length===0;
 return {schema:'reverse-android-runtime-evidence-v1',checkpoint_sha256:checkpointSha,instructions:report.guest.instructions,framework_runtime_verified:framework,sys_boot_completed_live:!reasons.includes('live_sys_boot_completed_unverified'),selinux_enforcing_live:!reasons.includes('live_selinux_enforcement_unverified'),processes:processEvidence,reasons,scanout_exported:scanout,ready_for_visual_review:framework&&scanout,android_display_content_verified:false,original_snapshot_unchanged:true};
}
