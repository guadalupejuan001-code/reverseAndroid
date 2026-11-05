// Freeze a published journal generation while the native CPU continues running.
// Hard links keep the original immutable bytes alive if journal cleanup removes its name.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,statSync,existsSync,mkdtempSync,linkSync,copyFileSync,renameSync,rmSync} from 'node:fs';
import {resolve,join,dirname,basename} from 'node:path';import {pathToFileURL} from 'node:url';
import {verifyProfileState} from './android_profile_state.mjs';import {sha} from './android_c13_avb.mjs';import {analyzeGuestConsole} from './android_guest_outcome.mjs';
const capacity=1024*1024*1024;
const bounded=(path,limit)=>{assert.ok(statSync(path).size<=limit,'Oversized journal evidence');return read(path);};
export function validatePinOptions({inputSha,experiment,sourceCommit,minimumInstructions=1}){
 assert.match(inputSha,/^[0-9a-f]{64}$/);assert.match(experiment,/^[a-z0-9-]{1,96}$/);assert.match(sourceCommit,/^[0-9a-f]{40}$/);assert.ok(Number.isSafeInteger(minimumInstructions)&&minimumInstructions>=1);return {inputSha,experiment,sourceCommit,minimumInstructions};
}
export function pinCheckpoint(journal,output,options){
 const o=validatePinOptions(options);assert.ok(!existsSync(output),'Pinned output already exists');
 const pointerFile=join(journal,'latest.json');if(!existsSync(pointerFile))return {status:'not_ready'};
 const pointer=JSON.parse(bounded(pointerFile,65536));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);assert.match(pointer.checkpoint_sha256,/^[0-9a-f]{64}$/);
 const generation=join(journal,pointer.generation);let temporary;
 try{
  const reportBytes=bounded(join(generation,'report.json'),8*1024*1024),report=JSON.parse(reportBytes),v=report.provenance;
  assert.equal(v.input_checkpoint_sha256,o.inputSha);assert.equal(v.experiment,o.experiment);assert.equal(v.original_state_modified,false);assert.equal(v.ram_profile_changed,false);assert.equal(v.snapshot_round_trip_verified,true);
  if(report.guest.instructions<o.minimumInstructions&&!report.error)return {status:'not_ready'};
  temporary=mkdtempSync(join(dirname(resolve(output)),'.'+basename(output)+'-pending-'));
  linkSync(join(generation,'checkpoint.rvg'),join(temporary,'current.rvg'));write(join(temporary,'current.json'),reportBytes);
  const state=bounded(join(temporary,'current.rvg'),capacity),digest=sha(state);assert.equal(digest,pointer.checkpoint_sha256);
  const binary=verifyProfileState(state,report,digest,{maxSnapshotBytes:capacity}),outcome=analyzeGuestConsole(report.console);
  const manifest={schema:'reverse-pinned-observation-v1',checkpoint_sha256:digest,report_sha256:sha(reportBytes),source_commit:o.sourceCommit,input_checkpoint_sha256:o.inputSha,experiment:o.experiment,journal_generation:pointer.generation,instructions:report.guest.instructions,pc:report.guest.pc,stop_reason:report.stop_reason,error:report.error,original_snapshot_modified:false,observation_only:true,source_profile_verified:true,immutable_image_sha256:binary.immutable_image_sha256,...outcome,android_framework_verified:false,android_display_verified:false};
  write(join(temporary,'observation.json'),JSON.stringify(manifest,null,2)+'\n');renameSync(temporary,output);temporary=undefined;return {status:'pinned',...manifest};
 }catch(e){if(e.code==='ENOENT')return {status:'not_ready'};throw e;}
 finally{if(temporary)rmSync(temporary,{recursive:true,force:true});}
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
 const [journal,output,inputSha,experiment,sourceCommit,minimum]=process.argv.slice(2);assert.ok(journal&&output);
 const r=pinCheckpoint(journal,output,{inputSha,experiment,sourceCommit,minimumInstructions:minimum===undefined?1:Number(minimum)});console.log(JSON.stringify(r,null,2));if(r.status!=='pinned')process.exitCode=3;
}catch(e){console.error(e.stack);process.exitCode=1;}}
