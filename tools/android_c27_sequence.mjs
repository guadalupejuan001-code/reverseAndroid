// One bounded continuation followed by exact, read-only runtime/screen evidence.
// A subsequent invocation is authorized only by the fully validated output contract.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,mkdirSync,existsSync,linkSync,copyFileSync as copy,openSync,closeSync,appendFileSync} from 'node:fs';
import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';import {spawnSync} from 'node:child_process';
import {run,contractForCompletedSource} from './android_c27_profile_resume.mjs';
import {inspectNativeRecovery} from './android_c40_recovery_evidence.mjs';
import {DISK_SHA,DISK_BYTES} from './android_c28_state.mjs';
import {sha} from './android_c13_avb.mjs';import {evaluateFrameworkEvidence} from './android_framework_evidence.mjs';
const json=p=>JSON.parse(read(p,'utf8'));
function observe(command,args,output,timeout){
 const out=openSync(output,'wx',0o600),err=openSync(output+'.log','wx',0o600);
 try{const r=spawnSync(command,args,{stdio:['ignore',out,err],timeout});return {exit_code:r.status,signal:r.signal,error:r.error?.message??null};}finally{closeSync(out);closeSync(err);}
}
export async function continueAndObserve({contract,source,native,disk,frameNative,output,sourceCommit,wallMs=900000,instructions=2000000000,cycles=3,maxTransportBytes=1073741824}){
 assert.match(sourceCommit,/^[0-9a-f]{40}$/);assert.ok(!existsSync(output),'Output already exists');mkdirSync(output,{recursive:true});write(join(output,'source-commit.txt'),sourceCommit+'\n');
 let continuation,error;
 try{continuation=await run(contract,source,native,disk,join(output,'native'),{wallMs,instructions,cycles,maxTransportBytes,onStatus:s=>process.stderr.write('REVERSE_C27_SEQUENCE '+JSON.stringify(s)+'\n')});}
 catch(e){error=e.stack;write(join(output,'continuation-error.txt'),error+'\n');}
 const inner=join(output,'native');
 if(existsSync(join(inner,'current.rvg'))){
  linkSync(join(inner,'current.rvg'),join(output,'current.rvg'));
  for(const name of ['current.json','result.json','process-outcome.json','native-pins.json','native-pins-verification.json','input-contract.json','console-new.txt'])if(existsSync(join(inner,name)))copy(join(inner,name),join(output,name));
 }else if(existsSync(join(inner,'journal/latest.json'))){
  const p=json(join(inner,'journal/latest.json'));assert.match(p.generation,/^generation-[1-9][0-9]*$/);const generation=join(inner,'journal',p.generation);
  linkSync(join(generation,'checkpoint.rvg'),join(output,'recovery-prefix.rvg'));copy(join(generation,'report.json'),join(output,'recovery-prefix.json'));
 }
 let recoveryEvidence;
 try{
  recoveryEvidence=await inspectNativeRecovery({sourceContract:contract,source,nativeDirectory:inner,maxTransportBytes});
  write(join(output,'recovery-evidence.json'),JSON.stringify(recoveryEvidence,null,2)+'\n');
 }catch(recoveryDiagnosticFailure){
  write(join(output,'recovery-evidence-error.txt'),recoveryDiagnosticFailure.stack+'\n');
 }
 const summary={schema:'reverse-c27-sequence-boundary-v1',clean_completed:false,ready_for_visual_review:false,android_display_content_verified:false,continuation_error:error??null};
 if(recoveryEvidence){summary.native_process_status=recoveryEvidence.supervisor_status;summary.native_stop_reason=recoveryEvidence.guest_stop_reason;summary.failure_class=recoveryEvidence.failure_class;summary.journal_sha_verified=recoveryEvidence.journal_sha_verified;summary.recovery_candidate_is_promotion_authority=false;}
 if(existsSync(join(output,'current.rvg'))&&existsSync(join(output,'current.json'))){
  const report=json(join(output,'current.json')),digest=sha(read(join(output,'current.rvg')));assert.equal(digest,report.checkpoint_sha256);summary.checkpoint_sha256=digest;summary.instructions=report.guest.instructions;
  const frame=observe(frameNative,[join(output,'current.rvg'),disk,digest,DISK_SHA,String(DISK_BYTES),join(output,'checkpoint-frame.ppm')],join(output,'checkpoint-frame.json'),600000);
  const kernel=observe(process.execPath,[new URL('./android_kernel_observer.mjs',import.meta.url).pathname,join(output,'current.rvg'),digest,String(maxTransportBytes),'--live-properties','--zygote-probe'],join(output,'kernel-observation.json'),300000);
  write(join(output,'observation-outcomes.json'),JSON.stringify({frame,kernel},null,2)+'\n');
  if([0,3].includes(frame.exit_code)&&kernel.exit_code===0){
   const evidence=evaluateFrameworkEvidence({checkpointSha:digest,report,result:json(join(output,'result.json')),host:json(join(output,'process-outcome.json')),kernel:json(join(output,'kernel-observation.json')),frame:json(join(output,'checkpoint-frame.json')),ppm:frame.exit_code===0?read(join(output,'checkpoint-frame.ppm')):undefined});
   write(join(output,'runtime-evidence.json'),JSON.stringify(evidence,null,2)+'\n');summary.ready_for_visual_review=evidence.ready_for_visual_review;
  }else summary.observation_error='Read-only evidence could not be validated; inspect separate outcomes/logs';
  if(!error&&continuation?.process.status==='process_completed'&&continuation?.result.execution_progress_accepted){
   try{const next=contractForCompletedSource(output,sourceCommit,{maxTransportBytes});write(join(output,'output-contract.json'),JSON.stringify(next,null,2)+'\n');summary.clean_completed=true;}
   catch(e){summary.output_contract_error=e.stack;}
  }
 }
 write(join(output,'boundary-summary.json'),JSON.stringify(summary,null,2)+'\n');return summary;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [contract,source,native,disk,frameNative,output,sourceCommit,wall,limit,count]=process.argv.slice(2);assert.ok(contract&&source&&native&&disk&&frameNative&&output&&sourceCommit);
 const r=await continueAndObserve({contract:json(contract),source,native,disk,frameNative,output,sourceCommit,wallMs:wall===undefined?900000:Number(wall),instructions:limit===undefined?2000000000:Number(limit),cycles:count===undefined?3:Number(count)});
 console.log(JSON.stringify(r,null,2));if(process.env.GITHUB_OUTPUT)appendFileSync(process.env.GITHUB_OUTPUT,`clean=${r.clean_completed}\nready=${r.ready_for_visual_review}\n`);if(!r.clean_completed)process.exitCode=2;
}catch(e){console.error(e.stack);process.exitCode=1;}}
