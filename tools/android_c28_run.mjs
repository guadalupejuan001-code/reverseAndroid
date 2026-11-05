import assert from 'node:assert/strict';import {mkdirSync,existsSync,writeFileSync} from 'node:fs';import {join} from 'node:path';import {spawnSync} from 'node:child_process';
import {preflight,verifyNativePins,audit} from './android_c28_result.mjs';import {superviseNativeCycle} from './android_supervised_cycle.mjs';
try{
 const [source,command,disk,output,wall,limit]=process.argv.slice(2),wallMs=Number(wall),instructions=Number(limit);
 assert.ok(source&&command&&disk&&output&&!existsSync(output),'Expected source native disk new-output wall-ms instructions');
 assert.ok(Number.isSafeInteger(wallMs)&&wallMs>0&&wallMs<=900000);assert.ok(Number.isSafeInteger(instructions)&&instructions>0&&instructions<=2000000000);
 const input=preflight(source),pins=spawnSync(command,['--pins'],{encoding:'utf8',timeout:30000,maxBuffer:65536});assert.ifError(pins.error);assert.equal(pins.status,0,pins.stderr);const descriptor=JSON.parse(pins.stdout),verified=verifyNativePins(descriptor);
 mkdirSync(output);writeFileSync(join(output,'native-pins.json'),JSON.stringify(descriptor,null,2)+'\n');writeFileSync(join(output,'native-pins-verification.json'),JSON.stringify(verified)+'\n');
 const host=await superviseNativeCycle({command,args:[join(source,'current.rvg'),join(source,'current.json'),disk,join(output,'journal'),String(wallMs),String(instructions)],outputDirectory:output,initialInstructions:input.report.guest.instructions,startupMs:600000,noProgressMs:90000,wallMs:3600000,graceMs:30000,onStatus:s=>process.stderr.write('REVERSE_C28_SUPERVISOR '+JSON.stringify(s)+'\n')});
 const result=existsSync(join(output,'journal/latest.json'))?audit(source,output):null;console.log(JSON.stringify({process:host,result},null,2));
 if(host.status!=='process_completed'||!result?.execution_progress_accepted)process.exitCode=2;
}catch(e){console.error(e.stack);process.exitCode=1;}
