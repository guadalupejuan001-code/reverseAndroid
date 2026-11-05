import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const [runtimeArg,buildArg]=process.argv.slice(2),runtime=path.resolve(runtimeArg),build=path.resolve(buildArg);
const spec=JSON.parse(fs.readFileSync(path.join(root,'data/interface.json'),'utf8'));
const A=8200000000000000000n,W=7400000000000000000n;
const run=(...args)=>execFileSync(runtime,args,{encoding:'utf8',maxBuffer:32*1024*1024,stdio:['ignore','pipe','pipe']});
const image=path.join(build,'BBY-Q.rxe'),state=path.join(build,'test-agent.rvp'),step=path.join(build,'test-step.xi');
fs.copyFileSync(path.join(build,'controller.rvp'),state);
const put=(n,v)=>`store8 ${n} ${v}\n`;
let observation=put(A+16n,1)+put(A+17n,8)+put(A+18n,10);
for(let id=1;id<=31;id++)observation+=put(A+32n+BigInt(id),1);
for(let id=16;id<=31;id++)observation+=put(8300000000000000512n+BigInt(id),2);
const consume=fs.readFileSync(path.join(root,'xi/consume.xi'),'utf8').split('\n').slice(1).join('\n').replace(/^halt$/gm,'');
const read=`load8 r0 ${A+64n}\nprint r0\nload8 r1 ${A+65n}\nprint r1\n`;
fs.writeFileSync(step,'#!reverse-pc-machine-v1\n'+read+consume+observation+'halt\n');
function action(){return run('run',step,state,state).split('\n').filter(l=>l.startsWith('{')).map(l=>JSON.parse(l).value);}
const transcript=[];let checks=0;
function ask(text,language,expected){
  const started=performance.now();
  const output=run('app-turn',image,state,state,language===1?'es':'en',text).trim();
  const elapsed=performance.now()-started;
  const result=action();assert.deepEqual(result,expected,text);checks++;
  transcript.push({input:text,language,output,op:result[0],target:result[1],cli_turn_ms:elapsed});return output;
}
// No connected desktop means no action, including when the RXE is imported
// into an older host. A restored transcript is never an execution request.
assert.equal(ask('abre archivos',1,[0,0]),spec.replies[3][0]);
for(const pattern of spec.patterns)for(const form of pattern.forms){
  if(!pattern.kind){ask(form,1,[pattern.op,0]);continue;}
  for(const target of spec.targets){
    if(pattern.kind===2?![1,2].includes(target.kind):pattern.kind!==target.kind)continue;
    for(const alias of target.aliases){
      const text=form.replace('{T}',alias);
      assert.equal(ask(text,2,[pattern.op,target.id]),spec.replies[0][1]);
    }
  }
  if(checks%100<40)console.log('CONTROL_GRAMMAR_PROGRESS',checks);
}
const grammarChecks=checks-1;
for(const text of ['no abras archivos','no abrir calculadora','qué significa abrir archivos','no open files','what is open files',
  'abre archivos y calculadora','abre archivos si','open files if','cierra desconocido','borra archivos','delete files',
  'ejecuta set r0 9','close files and notes','abre enfoque','switch to files','muestra principal',
  'abre archivos; cierra notas','"abre archivos"','¿qué significa calculadora?'])ask(text,1,[0,0]);
const negativeChecks=checks-grammarChecks-1;
fs.writeFileSync(path.join(build,'missing.xi'),'#!reverse-pc-machine-v1\n'+put(A+33n,0)+'halt\n');
run('run',path.join(build,'missing.xi'),state,state);
assert.equal(ask('cierra archivos',1,[0,0]),spec.replies[4][0]);
fs.writeFileSync(path.join(build,'ambiguous.xi'),'#!reverse-pc-machine-v1\n'+put(A+33n,2)+'halt\n');
run('run',path.join(build,'ambiguous.xi'),state,state);
assert.equal(ask('focus files',2,[0,0]),spec.replies[5][1]);
assert.equal(ask('hola',1,[0,0]),'Hola. Soy BBY Q.');
assert.equal(ask('aprende que interfazprueba es clasea',1,[0,0]),'Aprendí esa afirmación del diálogo.');
assert.equal(ask('Oye, ¿qué requiere interfazprueba?',1,[0,0]),'Según lo aprendido: Interfazprueba requiere agua.');
assert.equal(ask('qué requiere interfazprueba si',1,[0,0]),'No tengo información suficiente para responder.');
assert.equal(ask('qué requiere interfazprueba',1,[0,0]),'Según lo aprendido: Interfazprueba requiere agua.');
assert.equal(ask('abre archivos',1,[1,1]),spec.replies[0][0]);
assert.doesNotMatch(ask('por qué',1,[0,0]),/Interfazprueba|interfazprueba/);
assert.equal(ask('qué requiere interfazprueba',1,[0,0]),'Según lo aprendido: Interfazprueba requiere agua.');

// Upgrade a real E016 live account, retaining all user state outside the
// declared delta rather than restarting from the new packaged seed.
const legacyImage=path.join(build,'legacy-BBY-Q.rxe');
const old=path.join(build,'legacy-live.rvp'),migrated=path.join(build,'migrated-live.rvp');
fs.copyFileSync(path.join(build,'legacy-e016.rvp'),old);
assert.equal(run('app-turn',legacyImage,old,old,'es','ana tiene dos manzanas').trim(),'Ana tiene dos manzanas.');
assert.equal(run('app-turn',legacyImage,old,old,'es','ella recibe tres mas').trim(),'Ana tiene cinco manzanas.');
run('run',path.join(build,'upgrade-e016.rxe'),old,migrated);
assert.equal(run('app-turn',image,migrated,migrated,'es','cuántas manzanas tiene ana').trim(),'Ana tiene cinco manzanas.');checks+=3;
const oldHash=crypto.createHash('sha256').update(fs.readFileSync(old)).digest('hex');
fs.writeFileSync(path.join(build,'conflict.xi'),'#!reverse-pc-machine-v1\n'+put(A,99)+'halt\n');
run('run',path.join(build,'conflict.xi'),old,path.join(build,'conflicted.rvp'));
assert.throws(()=>run('run',path.join(build,'upgrade-e016.rxe'),path.join(build,'conflicted.rvp'),path.join(build,'must-not-exist.rvp')));
assert.ok(!fs.existsSync(path.join(build,'must-not-exist.rvp')));
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(old)).digest('hex'),oldHash);checks+=3;
const report={checks,grammar_combinations:grammarChecks,negative_controls:negativeChecks,
  tested_bytecode_sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(build,'code.rxe'))).digest('hex'),
  tested_seed_sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(build,'controller.rvp'))).digest('hex'),
  real_rxe_execution:true,old_account_retained:true,conflicting_migration_rejected:true,
  scope:'Finite interface grammar and actual XI/RXE; this is not a general-language or Android UI benchmark',transcript};
fs.writeFileSync(path.join(build,'native-results.json'),JSON.stringify(report,null,2)+'\n');
console.log('BBYQ_CONTROL_NATIVE_PASS',checks);
