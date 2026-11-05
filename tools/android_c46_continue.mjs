// C46 keeps the existing native CPU, source admission, clock and recovery gates.
// system_server alone never proves completed Android boot.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,appendFileSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {runStage} from './android_c42_zygote_progress.mjs';
import {hostTransportBound} from './android_c27_profile_resume.mjs';
const json=p=>JSON.parse(readFileSync(p,'utf8'));
export function milestone(progress,runtime,boundary,contract){
 assert.equal(boundary.clean_completed,true,'Only a clean verified budget may become a continuation source');
 for(const x of [progress,runtime,boundary])assert.equal(x.checkpoint_sha256,contract.checkpoint_sha256,'Crossed milestone checkpoint');
 assert.equal(progress.instructions,runtime.instructions);
 assert.equal(progress.instructions,boundary.instructions);
 assert.equal(progress.read_only,true);assert.equal(progress.input_unchanged,true);
 assert.equal(runtime.original_snapshot_unchanged,true);
 const boot=progress.system_server_alive_verified===true&&runtime.framework_runtime_verified===true&&
  runtime.sys_boot_completed_live===true&&runtime.selinux_enforcing_live===true&&runtime.reasons.length===0;
 return {schema:'reverse-c46-android-continuation-v1',checkpoint_sha256:contract.checkpoint_sha256,
  instructions:progress.instructions,source_boundary_verified:true,
  system_server_alive_verified:progress.system_server_alive_verified===true,
  android_boot_completed_verified:boot,scanout_exported:runtime.scanout_exported===true,
  ready_for_visual_review:boot&&runtime.scanout_exported===true,
  android_display_content_verified:false,unresolved:runtime.reasons};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 try{
  const [source,output,native,disk,frame,commit,previous,budget='500000000',transport='1073741824']=process.argv.slice(2);
  assert.ok(source&&output&&native&&disk&&frame&&commit&&previous);
  assert.match(budget,/^[1-9][0-9]{0,9}$/);const instructions=Number(budget);assert.ok(instructions<=2000000000);
  const maxTransportBytes=hostTransportBound(Number(transport));
  const progress=await runStage({source,output,native,disk,frame,commit,previousFile:previous,instructions,maxTransportBytes});
  const result=milestone(progress,json(join(output,'runtime-evidence.json')),json(join(output,'boundary-summary.json')),json(join(output,'output-contract.json')));
  writeFileSync(join(output,'c46-progress.json'),JSON.stringify(result,null,2)+'\n');
  if(process.env.GITHUB_OUTPUT)appendFileSync(process.env.GITHUB_OUTPUT,`boot=${result.android_boot_completed_verified}\nserver=${result.system_server_alive_verified}\nvisual=${result.ready_for_visual_review}\ninstructions=${result.instructions}\n`);
  console.log(JSON.stringify(result,null,2));
 }catch(error){console.error(error.stack);process.exitCode=1;}
}
