import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const [runtime,buildArg,bbyqArg]=process.argv.slice(2),build=path.resolve(buildArg),bbyq=path.resolve(bbyqArg||path.join(root,'../BBY-Q'));
const image=path.join(build,'BBY-Q.rxe'),state=path.join(build,'w4u-test.rvp'),step=path.join(build,'w4u-step.xi');
const A=8200000000000000000n,M=8300000000000000000n;
const run=(...args)=>execFileSync(runtime,args,{encoding:'utf8',maxBuffer:32*1024*1024,stdio:['ignore','pipe','pipe']});
const body=p=>fs.readFileSync(p,'utf8').split('\n').slice(1).join('\n');
const feedback=body(path.join(root,'reverse-os-android/app/src/main/assets/bbyq-interface/feedback.xi'));
const consume=body(path.join(root,'bbyq-interface/xi/consume.xi'));
let checks=0;const transcript=[];
function memory(){
  const b=fs.readFileSync(state),n=Number(b.readBigUInt64LE(5)),map=new Map();
  for(const row of b.subarray(29+n).toString().split('\n')){const [kind,a,v]=row.split(' ');if(kind==='O')map.set(BigInt(a),Number(v));}
  return a=>map.get(a)||0;
}
const put=(a,v)=>`store8 ${a} ${v}\n`;
function observation({present={},minimized={},maximized={},caps={},active=8,count,space=0,minCount=0}={}){
  let s=put(A+16n,1)+put(A+17n,active)+put(A+18n,count??Object.values(present).reduce((a,b)=>a+b,0))+put(M+710n,space)+put(M+711n,minCount);
  for(let i=0;i<32;i++)s+=put(A+32n+BigInt(i),present[i]||0)+put(M+640n+BigInt(i),minimized[i]||0)+put(M+672n+BigInt(i),maximized[i]||0)+put(M+512n+BigInt(i),caps[i]||0);
  return s;
}
function execute(code){fs.writeFileSync(step,'#!reverse-pc-machine-v1\n'+code);return run('run',step,state,state);}
function reset(){fs.copyFileSync(path.join(build,'controller.rvp'),state);}
function selected(){const m=memory();return [m(A+64n),m(A+65n)];}
function ask(text,obs){
  execute(observation(obs)+'halt\n');const output=run('app-turn',image,state,state,'es',text).trim();
  const action=selected(),m=memory();transcript.push({input:text,output,action,steps:m(M+19n),roles:[m(M+32n),m(M+33n),m(M+34n)]});
  execute(consume);return action;
}
function ack(success,obs){
  const output=execute(put(M+80n,success?1:0)+observation(obs)+feedback);
  const action=selected();execute(consume);return {action,text:Buffer.from(output.split('\n').filter(l=>l.startsWith('{')).map(l=>JSON.parse(l)).filter(x=>x.register==='r14').map(x=>x.value)).toString()};
}
function equal(a,b){assert.deepEqual(a,b);checks++;}
reset();equal(ask('maximiza calculadora',{present:{8:1}}),[1,2]);
let m=memory();equal([m(M+19n),m(M+32n),m(M+33n),m(M+34n)],[2,1,1,1]);
equal(ack(true,{present:{8:1,2:1},active:2}).action,[4,2]);
equal(ack(true,{present:{8:1,2:1},maximized:{2:1},active:2}),{action:[0,0],text:'Acción completada.'});
equal(memory()(M+16n),3);
// A success flag with no observed window must not advance the plan.
reset();equal(ask('maximiza calculadora',{present:{8:1}}),[1,2]);
equal(ack(true,{present:{8:1}}),{action:[0,0],text:'No se completó la acción.'});
equal(memory()(M+16n),4);
reset();equal(ask('ejecuta programa 1',{present:{8:1},caps:{16:2}}),[1,16]);
equal(ack(true,{present:{8:1,16:1},caps:{16:2},active:16}).action,[10,16]);
equal(ack(true,{present:{8:1,16:1},caps:{16:2},active:16}).text,'Acción completada.');
// Removing/replacing a program or making it conversational blocks step 2.
for(const caps of [{},{16:1}]){
  reset();equal(ask('ejecuta programa 1',{present:{8:1},caps:{16:2}}),[1,16]);
  equal(ack(true,{present:{8:1,16:1},caps,active:16}).action,[0,0]);
  equal(memory()(M+16n),4);
}
for(const text of ['ejecuta programa 17','no ejecuta programa 1','run program 1 if','¿ejecuta programa 1?','borra programa 1']){
  reset();equal(ask(text,{present:{8:1},caps:{16:2}}),[0,0]);
}
reset();equal(ask('ejecuta programa 1',{present:{8:1},caps:{16:1}}),[0,0]);
reset();equal(ask('ejecuta programa 1',{present:{8:1}}),[0,0]);
reset();equal(ask('maximiza calculadora',{present:{8:1},count:12}),[0,0]);
reset();equal(ask('ejecuta programa 1',{present:{8:1,16:1},caps:{16:2}}),[10,16]);
// An executable change between actions is caught by the verifier's capability.
reset();ask('ejecuta programa 1',{present:{8:1},caps:{16:2}});
equal(ack(false,{present:{8:1},caps:{16:2}}).action,[0,0]);
// Corrupt an explorer proposal: the independent verifier rejects a new target.
reset();execute(observation({present:{8:1}})+'halt\n');
run('app-turn',image,state,state,'es','maximiza calculadora');
execute(put(M+16n,1)+put(M+131n,1)+body(path.join(bbyq,'ai/xi/w4u/verify.xi')));
equal(selected(),[0,0]);equal(memory()(M+34n),2);
// The reusable W4u RXE produces the same prerequisite plan without language.
reset();execute(observation({present:{8:1}})+put(A+64n,4)+put(A+65n,2)+put(A+67n,1)+'halt\n');
run('run',path.join(build,'W4u.rxe'),state,state);
equal(selected(),[1,2]);equal(memory()(M+19n),2);
// Control-1 -> W4u preserves learned user knowledge and quantum/core bytes.
fs.copyFileSync(path.join(build,'control-v1.rvp'),state);
equal(run('app-turn',image,state,state,'es','aprende que w4uprueba es clasea').trim(),'Aprendí esa afirmación del diálogo.');
const old=fs.readFileSync(state),core=b=>b.subarray(29,29+Number(b.readBigUInt64LE(5)));
execute(put(8400000000000000010n,91)+'halt\n');
run('run',path.join(build,'upgrade-control.rxe'),state,state);
equal(memory()(8400000000000000010n),91);equal(core(fs.readFileSync(state)),core(old));
equal(run('app-turn',image,state,state,'es','qué requiere w4uprueba').trim(),'Según lo aprendido: W4uprueba requiere agua.');
equal(ask('hola',{present:{8:1}}),[0,0]);
const manifest=JSON.parse(fs.readFileSync(path.join(build,'manifest.json')));
equal(manifest.agents.engine,'W4u');
fs.writeFileSync(path.join(build,'w4u-results.json'),JSON.stringify({checks,engine:'W4u',roles:3,max_steps:2,transcript,scope:'Real XI/RXE; bounded planning, independent verification, migration and failure injection. Host observations here are fixtures.'},null,2)+'\n');
console.log('W4U_NATIVE_PASS',checks);
