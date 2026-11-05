// C42 read-only milestones around the existing authenticated native continuation.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,existsSync,openSync,closeSync,appendFileSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {spawnSync} from 'node:child_process';
import {preflight} from './android_c27_profile_resume.mjs';
import {continueAndObserve} from './android_c27_sequence.mjs';
import {classifyZygoteStage} from './android_c40_zygote_stage.mjs';
import {isLiveUserspaceTask,verifiedMainZygoteProcesses} from './android_framework_evidence.mjs';

const json=p=>JSON.parse(readFileSync(p,'utf8'));
const save=(p,v)=>writeFileSync(p,JSON.stringify(v,null,2)+'\n');
const HEX=/^[0-9a-f]{64}$/;
const decimal=s=>typeof s==='string'&&/^[0-9]{1,24}$/.test(s)?BigInt(s):null;

export function summarizeZygoteProgress(kernel,previous=null){
 const stage=classifyZygoteStage(kernel);
 assert.ok(Number.isSafeInteger(kernel.instructions)&&kernel.instructions>0);
 const tasks=kernel.tasks??[],processes=kernel.zygote_launch.processes;
 const mainZygotes=new Set(verifiedMainZygoteProcesses(kernel).map(item=>item.process));
 const taskFor=p=>tasks.find(t=>t.pid===p.pid&&t.name===p.comm&&isLiveUserspaceTask(t));
 const zygotes=processes.filter(p=>p.argv_verified===true&&p.exe_path==='/system/bin/app_process64'&&
   p.parent_pid===1&&isLiveUserspaceTask(p)&&taskFor(p)&&
   (['zygote64','zygote'].includes(p.comm)||(p.comm==='app_process64'&&p.launch_signature_verified===true)||mainZygotes.has(p)));
 const servers=processes.filter(p=>p.comm==='system_server'&&isLiveUserspaceTask(p)&&taskFor(p));
 const matching=servers.filter(p=>p.argv_verified===true&&p.exe_path==='/system/bin/app_process64'&&
   zygotes.some(z=>z.pid===p.parent_pid));
 const server=servers.length===1&&matching.length===1?matching[0]:null;
 const process=zygotes.length===1?taskFor(zygotes[0]):null;
 const runtime=decimal(process?.accounting?.sum_exec_runtime);
 let delta=null;
 if(previous){
  assert.equal(previous.schema,'reverse-c42-zygote-progress-v1');
  assert.match(previous.checkpoint_sha256,HEX);
  assert.ok(kernel.instructions>previous.instructions,'C42 observer did not advance');
  const before=decimal(previous.zygote_runtime_ns);
  if(process&&previous.zygote_pid===process.pid&&runtime!==null&&before!==null){
   assert.ok(runtime>=before,'Same Zygote PID CPU accounting regressed');delta=(runtime-before).toString();
  }
 }
 const stack=(process?.stack??[]).map(s=>s.interrupted_symbol??s.symbol).filter(Boolean);
 const status=server?'system_server_live_with_zygote_parent':!process?'zygote_not_verified':
   process.state===0?'zygote_runnable':
   stack.some(s=>/io_schedule|folio_wait|filemap|erofs|verity/.test(s))?'zygote_waiting_for_file_io':'zygote_sleeping';
 return {schema:'reverse-c42-zygote-progress-v1',checkpoint_sha256:kernel.checkpoint_sha256,
  instructions:kernel.instructions,read_only:true,input_unchanged:true,
  stage:stage.stage,progress_status:status,zygote_pid:process?.pid??null,
  zygote_comm:process?.name??null,zygote_state:process?.state??null,
  zygote_runtime_ns:runtime===null?null:runtime.toString(),zygote_runtime_delta_ns:delta,
  zygote_kernel_stack:stack,
  previous_checkpoint_sha256:previous?.checkpoint_sha256??null,
  system_server_alive_verified:!!server,system_server_pid:server?.pid??null,
  system_server_parent_pid:server?.parent_pid??null,
  system_server_candidate_pids:servers.map(p=>p.pid),
  zygote_socket_responsive_verified:false,android_boot_completed_verified:false};
}

export function verifyProgressBinding(progress,contract,sourceProof,previous){
 assert.equal(progress.checkpoint_sha256,contract.checkpoint_sha256);
 assert.equal(progress.instructions,sourceProof.report.guest.instructions);
 if(previous){
  assert.equal(progress.previous_checkpoint_sha256,previous.checkpoint_sha256);
  assert.equal(sourceProof.report.initial_instructions,previous.instructions);
  assert.equal(sourceProof.report.provenance.input_checkpoint_sha256,previous.checkpoint_sha256);
 }
 return true;
}

export function inspectProgress(root,previousFile=null,{maxTransportBytes=1073741824}={}){
 const contract=json(join(root,'output-contract.json')),proof=preflight(contract,root,{maxTransportBytes});
 const target=join(root,'c42-kernel.json'),log=target+'.log';
 assert.ok(!existsSync(target),'C42 observation already exists');
 const out=openSync(target,'wx'),err=openSync(log,'wx');let result;
 try{result=spawnSync(process.execPath,['--max-old-space-size=5000',
   new URL('./android_kernel_observer.mjs',import.meta.url).pathname,
   join(root,'current.rvg'),contract.checkpoint_sha256,String(maxTransportBytes),'--live-properties','--zygote-probe','--scheduler-probe'],
   {stdio:['ignore',out,err],timeout:300000});}
 finally{closeSync(out);closeSync(err);}
 assert.ifError(result.error);assert.equal(result.status,0,'C42 kernel observer failed; inspect its log');
 const kernel=json(target),previous=previousFile?json(previousFile):null;
 assert.equal(kernel.instructions,proof.report.guest.instructions);
 assert.equal(kernel.checkpoint_sha256,contract.checkpoint_sha256);
 const security=kernel.live_kernel_security;
 assert.equal(security.btf_layout_verified,true);assert.equal(security.selinux_initialized,true);
 assert.equal(security.selinux_enforcing,true);assert.equal(security.policy_present,true);
 for(const name of ['odsign.key.done','odsign.verification.done']){
  const matches=kernel.live_boot_property.boot_chain_property_observations.filter(p=>p.name===name&&p.status==='property_observed');
  assert.ok(matches.length>0&&matches.every(p=>p.value==='1'),'Security property missing or regressed: '+name);
 }
 const progress=summarizeZygoteProgress(kernel,previous);
 verifyProgressBinding(progress,contract,proof,previous);
 save(join(root,'c42-progress.json'),progress);return progress;
}

export async function runStage({source,output,native,disk,frame,commit,previousFile,instructions=500000000,wallMs=900000,maxTransportBytes=1073741824}){
 const contract=json(join(source,'output-contract.json')),before=preflight(contract,source,{maxTransportBytes});
 const boundary=await continueAndObserve({contract,source,native,disk,frameNative:frame,output,
   sourceCommit:commit,wallMs,instructions,cycles:1,maxTransportBytes});
 assert.equal(boundary.clean_completed,true,'C42 continuation is not a safe source');
 const after=preflight(json(join(output,'output-contract.json')),output,{maxTransportBytes});
 assert.equal(after.report.initial_instructions,before.report.guest.instructions);
 assert.equal(after.report.provenance.input_checkpoint_sha256,contract.checkpoint_sha256);
 assert.equal(after.report.provenance.input_report_sha256,contract.report_sha256);
 assert.ok(after.report.advanced_instructions>0&&after.report.advanced_instructions<=instructions);
 return inspectProgress(output,previousFile,{maxTransportBytes});
}

if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 let failureRoot;
 try{
  const [mode,...args]=process.argv.slice(2);let progress;
  if(mode==='inspect'){
   const [root,previous]=args;assert.ok(root);failureRoot=root;progress=inspectProgress(root,previous??null);
  }else{
   assert.equal(mode,'run');const [source,output,native,disk,frame,commit,previous,instructionText='500000000']=args;
   assert.ok(source&&output&&native&&disk&&frame&&commit&&previous);failureRoot=output;
   assert.match(instructionText,/^[1-9][0-9]{0,9}$/,'Expected a positive decimal instruction budget');
   const instructions=Number(instructionText);
   assert.ok(instructions<=2000000000,'Instruction budget exceeds the native 2-billion ceiling');
   progress=await runStage({source,output,native,disk,frame,commit,previousFile:previous,instructions});
  }
  if(process.env.GITHUB_OUTPUT)appendFileSync(process.env.GITHUB_OUTPUT,`server=${progress.system_server_alive_verified}\ninstructions=${progress.instructions}\n`);
  console.log(JSON.stringify(progress,null,2));
 }catch(error){
  if(failureRoot&&existsSync(failureRoot))writeFileSync(join(failureRoot,'c42-stage-error.txt'),error.stack+'\n');
  console.error(error.stack);process.exitCode=1;
 }
}
