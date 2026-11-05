// C40: classify only evidence from the authenticated guest snapshot.
// app_process64 with --zygote is the launcher; it does NOT establish a
// registered, responsive Zygote socket or a booted Android framework.
import assert from 'node:assert/strict';
import {verifiedMainZygoteProcesses} from './android_framework_evidence.mjs';
const HEX=/^[0-9a-f]{64}$/;
const live=t=>t&&Number.isSafeInteger(t.pid)&&t.pid>0&&
  t.exit_state===0&&t.has_user_mm===true&&
  Number.isInteger(t.state)&&t.state>=0&&t.state<65536;
const observedProperty=(kernel,name)=>{
 const matches=(kernel?.live_boot_property?.boot_chain_property_observations??[])
  .filter(p=>p.name===name&&p.status==='property_observed');
 const values=new Set(matches.map(p=>p.value));
 return values.size===1?[...values][0]:null;
};
export function classifyZygoteStage(kernel){
 assert.ok(kernel&&typeof kernel==='object');
 assert.match(kernel.checkpoint_sha256??'',HEX,'C40 requires authenticated checkpoint hash');
 assert.equal(kernel.input_unchanged,true,'C40 observer must not mutate the checkpoint');
 const z=kernel.zygote_launch;
 assert.equal(z?.source_checkpoint_sha256,kernel.checkpoint_sha256,'C40 argv evidence must match the exact checkpoint');
 assert.equal(z?.schema,'reverse-c40-zygote-argv-v1');
 assert.equal(z?.read_only,true);
 const processes=(z.processes??[]).filter(live);
 const launch=processes.find(p=>p.comm==='app_process64'&&p.argv_verified===true&&
   p.launch_signature_verified===true&&p.parent_pid===1&&
   p.exe_path==='/system/bin/app_process64'&&p.argv?.includes('--zygote'))??null;
 const named=processes.find(p=>['zygote64','zygote'].includes(p.comm)&&
   p.argv_verified===true&&p.exe_path==='/system/bin/app_process64')??null;
 const server=kernel.tasks?.find(p=>p.name==='system_server'&&live(p))??null;
 const surface=kernel.tasks?.find(p=>p.name==='surfaceflinger'&&live(p))??null;
 const initZygote=observedProperty(kernel,'init.svc.zygote');
 const main=verifiedMainZygoteProcesses(kernel)[0]?.process??null;
 const stage=server?'system_server_process_observed':
   named?'zygote_process_renamed':
   main?'zygote_main_thread_observed':
   launch?'zygote_launcher_executing':
   initZygote==='running'?'zygote_init_service_running_no_verified_argv':
   'zygote_not_observed';
 return {schema:'reverse-c40-zygote-stage-v1',read_only:true,
   checkpoint_sha256:kernel.checkpoint_sha256,instructions:kernel.instructions,
   process_count:kernel.task_count,
   stage,init_svc_zygote:initZygote,
   launcher_pid:launch?.pid??null,launcher_argv:launch?.argv??null,
   named_zygote_pid:named?.pid??null,system_server_pid:server?.pid??null,
   surfaceflinger_pid:surface?.pid??null,
   zygote_exec_confirmed:!!launch||!!named||!!main,
   ...(main?{verified_zygote_pid:main.pid,observed_zygote_comm:'main',identity_basis:'live_argv_executable_parent_and_pid1_service_pid'}:{}),
   zygote_socket_responsive_verified:false,
   zygote_boot_completed_verified:false,
   android_boot_completed_verified:false,
   unverified_argv_errors:(z.processes??[]).filter(p=>p.read_error).map(p=>({pid:p.pid,error:p.read_error})),
   fidelity_note:'argv and init.svc are live guest observations; startup does not prove Zygote readiness'};
}
