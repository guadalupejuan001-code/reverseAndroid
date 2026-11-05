// Host-only wait and evidence collection. It never reads or writes guest memory.
import assert from 'node:assert/strict';
import {readFileSync as read,existsSync,copyFileSync as copy,linkSync,appendFileSync} from 'node:fs';
import {join} from 'node:path';import {setTimeout as delay} from 'node:timers/promises';
const [marker,output,started,stdout,stderr,bundle]=process.argv.slice(2);assert.ok(marker&&output&&started&&bundle);
const deadline=Number(read(started,'utf8').trim())*1000+70*60*1000;assert.ok(Number.isSafeInteger(deadline));
while(!existsSync(marker)){assert.ok(Date.now()<deadline,'Bounded continuation completion deadline');await delay(5000);}
if(existsSync(output)){
 for(const [from,to]of[[stdout,'supervised-stdout.json'],[stderr,'supervisor.log']])if(existsSync(from))copy(from,join(output,to));
 for(const name of ['tested-source.tar.gz','build-sha256.txt','host-tests.tap','checkpoint-frame-tests.tap'])if(existsSync(join(bundle,name)))linkSync(join(bundle,name),join(output,name));
}
assert.equal(read(marker,'utf8').trim(),'0','Continuation failed; inspect the preserved canonical state and diagnostics');
const r=JSON.parse(read(join(output,'boundary-summary.json'),'utf8'));assert.equal(r.clean_completed,true);
if(process.env.GITHUB_OUTPUT)appendFileSync(process.env.GITHUB_OUTPUT,`clean=${r.clean_completed}\nready=${r.ready_for_visual_review}\n`);
console.log(JSON.stringify(r));
