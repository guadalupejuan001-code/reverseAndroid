// Freeze actual immutable progress while a supervised C27-profile CPU keeps running.
import assert from 'node:assert/strict';import {readFileSync as read,existsSync} from 'node:fs';
import {join} from 'node:path';import {setTimeout as delay} from 'node:timers/promises';
import {pinCheckpoint} from './android_c28_pin.mjs';
const [journal,output,contractFile,sourceCommit,minimum,completionMarker]=process.argv.slice(2);
assert.ok(journal&&output&&contractFile&&completionMarker);const contract=JSON.parse(read(contractFile));
const elapsed=Number(minimum);assert.ok(Number.isSafeInteger(elapsed)&&elapsed>0&&elapsed<=900000);const deadline=Date.now()+25*60*1000;
for(;;){
 try{
  if(existsSync(join(journal,'latest.json'))){
   const p=JSON.parse(read(join(journal,'latest.json')));assert.match(p.generation,/^generation-[1-9][0-9]*$/);const r=JSON.parse(read(join(journal,p.generation,'report.json')));
   if(r.error||r.host_elapsed_ms>=elapsed||existsSync(completionMarker)){
    const pin=pinCheckpoint(journal,output,{inputSha:contract.checkpoint_sha256,experiment:'c27-profile-budget-continuation',sourceCommit,minimumInstructions:1});
    if(pin.status==='pinned'){console.log(JSON.stringify(pin));break;}
   }
  }
 }catch(e){if(e.code!=='ENOENT')throw e;}
 if(existsSync(completionMarker)){console.log('Native finished before a suitable observation; inspect canonical final evidence');break;}
 assert.ok(Date.now()<deadline,'Timed out waiting for a published observation');await delay(2000);
}
