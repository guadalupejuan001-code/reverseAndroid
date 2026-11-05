import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

// Build-time transport and declarative interface bindings. Interpretation and
// execution of the command grammar remain in the XI sources, over one RVPC2.
const root=path.dirname(fileURLToPath(import.meta.url)),repo=path.dirname(root);
const [runtimeArg,bbyqArg,outArg,legacyArg,...flags]=process.argv.slice(2);
if(!runtimeArg||!bbyqArg||!outArg)throw Error('Usage: node bbyq-interface/build.mjs RUNTIME BBYQ_SOURCE OUTPUT');
const runtime=path.resolve(runtimeArg),bbyq=path.resolve(bbyqArg),out=path.resolve(outArg);
fs.mkdirSync(out,{recursive:true});
const A=8200000000000000000n;
const digest=b=>crypto.createHash('sha256').update(b).digest('hex');
const exec=(...args)=>execFileSync(runtime,args,{maxBuffer:32*1024*1024,encoding:'utf8'});
const source=p=>fs.readFileSync(p,'utf8');
const store=(a,v)=>`store8 ${a} ${v}\n`;
const spec=JSON.parse(source(path.join(root,'data/interface.json')));
function bindings(spec){
const forms=spec.patterns.flatMap(p=>p.forms.map(text=>({...p,text})));
const names=spec.targets.flatMap(t=>t.aliases.map(text=>({...t,text})));
const words=[...new Set([...forms,...names].flatMap(x=>x.text.split(' ')).filter(x=>x!=='{T}'))].sort();
if(words.length>128||names.length>128||forms.length>64)throw Error('Interface table capacity exceeded');
const vocabulary=new Map(words.map((w,i)=>[w,i+1]));
let data='#!reverse-pc-machine-v1\nload8 r0 '+A+'\ncmp r0 0\njne invalid\n';
for(const [offset,value]of [[0,73],[1,67],[2,1],[3,1],[4,words.length],[5,names.length],[6,forms.length]])data+=store(A+BigInt(offset),value);
for(const [i,word]of words.entries()){
  const bytes=Buffer.from(word);if(bytes.length>32)throw Error('Word too long');
  const a=A+5000n+BigInt(i*40);data+=store(a,i+1)+store(a+1n,bytes.length);
  for(const [j,b]of bytes.entries())data+=store(a+2n+BigInt(j),b);
}
for(const [i,t]of names.entries()){
  const tokens=t.text.split(' ');if(tokens.length>4)throw Error('Target alias too long');
  const a=A+15000n+BigInt(i*16);data+=store(a,tokens.length)+store(a+1n,t.id)+store(a+2n,t.kind);
  tokens.forEach((w,j)=>{data+=store(a+4n+BigInt(j),vocabulary.get(w));});
}
for(const [i,p]of forms.entries()){
  const tokens=p.text.split(' '),variables=tokens.filter(t=>t==='{T}').length;
  if(tokens.length>8||variables>(p.kind?1:0)||variables!==(p.kind?1:0))throw Error('Invalid interface frame');
  const a=A+20000n+BigInt(i*32);data+=store(a,tokens.length)+store(a+1n,p.op)+store(a+2n,p.kind)+store(a+3n,variables);
  tokens.forEach((w,j)=>{data+=store(a+8n+BigInt(j),w==='{T}'?255:vocabulary.get(w));});
}
for(const [i,pair]of spec.replies.entries())for(const [l,text]of pair.entries()){
  const bytes=Buffer.from(text),a=A+30000n+BigInt((i*2+l)*128);
  if(bytes.length>127)throw Error('Reply too long');
  data+=store(a,bytes.length);for(const [j,b]of bytes.entries())data+=store(a+1n+BigInt(j),b);
}
data+='halt\nlabel invalid\ndiv r0 0\n';
return {data,words,names,forms};
}
let {data,words,names,forms}=bindings(spec);
const M=8300000000000000000n;
data=data.replace('halt\nlabel invalid',store(M,77)+store(M+1n,1)+'halt\nlabel invalid');
fs.writeFileSync(path.join(out,'interface-bindings.xi'),data);
const model=JSON.parse(source(path.join(bbyq,'ai/checkpoints/e022-manifest.json')));
for(const [file,expected]of Object.entries({
  'ai/xi/knowledge/read.xi':'9f9f95cef821a63a164f081784e2a9c45e8f1c40561800c09291fe847fd9f3c4',
  'ai/xi/lingua/clause.xi':'923827af0d87b34223adf210f6301a0c1f9813a4bd9d89dcc1d846c439128613'
}))if(digest(fs.readFileSync(path.join(bbyq,file)))!==expected)throw Error('Unreviewed parser source: '+file);
const seed=zlib.gunzipSync(Buffer.from(source(path.join(bbyq,'ai/checkpoints',model.file)),'base64'));
if(digest(seed)!=='33a2490be0d7a9252222991da1cd376e7cfb4ecb0014987c65d1b8b24278f20f'||seed.length!==500410)throw Error('Unreviewed E022 seed');
fs.writeFileSync(path.join(out,'e022.rvp'),seed);
exec('run',path.join(out,'interface-bindings.xi'),path.join(out,'e022.rvp'),path.join(out,'controller.rvp'));
function link(text,prefix,exit){
  return text.split('\n').filter(l=>!l.startsWith('#!')).map(line=>{
    let m=line.match(/^(\s*)(label|jmp|je|jne|jge|jle|jgt|jlt)\s+(\S+)(.*)$/);
    if(m)return m[1]+m[2]+' '+prefix+m[3]+m[4];
    return /^\s*halt(?:\s|$)/.test(line)?'jmp '+exit:line;
  }).join('\n')+'\n';
}
const linked=execFileSync('bash',[path.join(bbyq,'ai/xi/knowledge/link.sh'),'conversation'],{encoding:'utf8',maxBuffer:4*1024*1024});
const agents=path.join(bbyq,'ai/xi/w4u');
const agentSource=name=>source(path.join(agents,name+'.xi'));
// Reusable typed-goal engine. BBY Q links exactly these modules into its turn.
const engine='#!reverse-pc-machine-v1\n'+link(agentSource('plan'),'engine_plan_','engine_explore')+
  'label engine_explore\n'+link(agentSource('explore'),'engine_explore_','engine_verify')+
  'label engine_verify\n'+link(agentSource('verify'),'engine_verify_','engine_finish')+
  'label engine_finish\nload8 r0 '+(A+64n)+'\nprint r0\nload8 r1 '+(A+65n)+'\nprint r1\nhalt\n';
fs.writeFileSync(path.join(out,'W4u.xi'),engine);
exec('compile',path.join(out,'W4u.xi'),path.join(out,'W4u.rxe'),'W4u 0.1 work engine');
const conversation='#!reverse-pc-machine-v1\n'+link(source(path.join(bbyq,'ai/xi/knowledge/read.xi')),'ui_read_','ui_parse')+
  'label ui_parse\n'+link(source(path.join(root,'xi/interpret.xi')),'actor_','agent_plan')+
  'label agent_plan\n'+link(agentSource('plan'),'planner_','agent_explore')+
  'label agent_explore\n'+link(agentSource('explore'),'explorer_','agent_verify')+
  'label agent_verify\n'+link(agentSource('verify'),'verifier_','ui_route')+
  'label ui_route\nload8 r0 '+(A+67n)+'\ncmp r0 0\nje bbyq_conversation\n'+
  // UI commands own this turn's referent. Keep learned facts, topics and
  // accounts, but never explain an old factual answer as the UI action.
  'set r0 4\nlabel ui_reset_legacy_context\nset r7 7700000000000000000\nadd r7 r0\nstore8 $r7 0\nadd r0 1\ncmp r0 16\njlt ui_reset_legacy_context\n'+
  'set r0 12\nlabel ui_reset_factual_context\nset r7 7800000000000000000\nadd r7 r0\nstore8 $r7 0\nadd r0 1\ncmp r0 20\njlt ui_reset_factual_context\n'+
  link(source(path.join(root,'xi/speak.xi')),'ui_speak_','ui_clean')+
  'label ui_clean\n'+link(source(path.join(bbyq,'ai/xi/knowledge/clean.xi')),'ui_clean_','ui_finish')+
  'label ui_finish\nhalt\nlabel bbyq_conversation\n'+linked.split('\n').slice(1).join('\n');
fs.writeFileSync(path.join(out,'conversation.xi'),conversation);
exec('compile',path.join(out,'conversation.xi'),path.join(out,'code.rxe'),'BBY Q interface XI');
const hostSourceRef=execFileSync('git',['-C',repo,'rev-parse','HEAD'],{encoding:'utf8'}).trim();
const frozen=JSON.parse(source(path.join(root,'data/w4u-v1-manifest.json')));
if(digest(fs.readFileSync(path.join(out,'code.rxe')))!==frozen.bytecode_sha256||digest(fs.readFileSync(path.join(out,'controller.rvp')))!==frozen.controller_sha256||digest(Buffer.from(conversation))!==frozen.linked_source_sha256)throw Error('W4u engine changed: version and migrate it before replacing an installed identity');
const sourceRef=frozen.interface_source_commit;
const info=JSON.parse(exec('pack-app',path.join(out,'code.rxe'),path.join(out,'controller.rvp'),path.join(out,'BBY-Q.rxe'),'org.reverse.bbyq','BBY Q','E023-W4u-1',sourceRef));

if(digest(fs.readFileSync(path.join(out,'BBY-Q.rxe')))!==frozen.executable_sha256)throw Error('W4u executable identity drift');
// A guarded migration adds only the E016 -> E022/interface delta. Every changed
// byte must still have its old or already-migrated value; unrelated user bytes,
// quantum core and bindings are retained. Conflict aborts the whole runtime call.
function snapshot(b){
  if(b.subarray(0,5).toString()!=='RVPC2')throw Error('Invalid RVPC2');
  const n=Number(b.readBigUInt64LE(5)),core=b.subarray(29,29+n),rows=b.subarray(29+n).toString().split('\n').filter(Boolean);
  if(!rows[0].startsWith('MXIM2 ')||rows[1]!=='MXIM1 0')throw Error('Unsupported logical snapshot');
  const memory=new Map(),bindings=[];
  for(const row of rows.slice(2)){const [kind,a,v]=row.split(' ');if(kind==='O')memory.set(a,Number(v));else bindings.push(row);}
  return {core,memory,bindings};
}
const legacy=zlib.gunzipSync(Buffer.from(source(path.join(bbyq,'ai/checkpoints/BBYQ_E016_Conversation_ES_EN.rvp.gz.b64')),'base64'));
if(digest(legacy)!=='55992a06df9ec9cad6889f3dabbdbc179d2626cfd32667efec1f74b78660e732')throw Error('Unreviewed E016');
const current=fs.readFileSync(path.join(out,'controller.rvp'));
function migration(previous,name){
  const old=snapshot(previous),next=snapshot(current);
  if(!old.core.equals(next.core)||JSON.stringify(old.bindings)!==JSON.stringify(next.bindings))throw Error('Migration would replace the owner core/bindings');
  let migration='#!reverse-pc-machine-v1\n',changed=0;
  for(const a of [...new Set([...old.memory.keys(),...next.memory.keys()])].sort((a,b)=>BigInt(a)<BigInt(b)?-1:1)){
    const was=old.memory.get(a)||0,value=next.memory.get(a)||0;if(was===value)continue;
    const label='byte_'+changed++;migration+=`load8 r0 ${a}\ncmp r0 ${was}\nje ${label}\ncmp r0 ${value}\njne conflict\nlabel ${label}\n`+store(a,value);
  }
  migration+='halt\nlabel conflict\ndiv r0 0\n';
  fs.writeFileSync(path.join(out,name+'.xi'),migration);
  exec('compile',path.join(out,name+'.xi'),path.join(out,name+'.rxe'),'BBY Q guarded upgrade');
  return changed;
}
const changed=migration(legacy,'upgrade-e016');
const oldConfig=JSON.parse(source(path.join(root,'data/control-v1.json')));
const oldInfo=JSON.parse(source(path.join(root,'data/control-v1-manifest.json')));
fs.writeFileSync(path.join(out,'control-v1-bindings.xi'),bindings(oldConfig).data);
exec('run',path.join(out,'control-v1-bindings.xi'),path.join(out,'e022.rvp'),path.join(out,'control-v1.rvp'));
const oldControl=fs.readFileSync(path.join(out,'control-v1.rvp'));
if(digest(oldControl)!==oldInfo.controller_sha256)throw Error('Unreviewed Control-1 seed');
const controlChanged=migration(oldControl,'upgrade-control');
fs.writeFileSync(path.join(out,'legacy-e016.rvp'),legacy);
const manifest={format:1,base_bbyq_commit:execFileSync('git',['-C',bbyq,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),interface_source_commit:sourceRef,host_source_commit:hostSourceRef,agent_image_reused:true,
  checkpoint_sha256:digest(seed),controller_sha256:digest(current),controller_bytes:current.length,executable_sha256:digest(fs.readFileSync(path.join(out,'BBY-Q.rxe'))),
  linked_source_sha256:digest(Buffer.from(conversation)),grammar_sha256:digest(Buffer.from(JSON.stringify(spec))),
  bytecode_sha256:digest(fs.readFileSync(path.join(out,'code.rxe'))),
  interface_words:words.length,target_aliases:names.length,command_frames:forms.length,grammar:spec.grammar,
  prior_training_replayed:false,migration_bytes_changed:changed,control_migration_bytes_changed:controlChanged,legacy_control_sha256:oldInfo.executable_sha256,legacy_control_sha256s:JSON.parse(source(path.join(root,'data/control-v1-identities.json'))).identities.map(x=>x.sha256),
  agents:{engine:'W4u',version:'0.1',artifact_sha256:digest(fs.readFileSync(path.join(out,'W4u.rxe'))),architecture:'bounded cooperative XI roles; sequential over one RVPC2',max_steps:2,planner_sha256:digest(Buffer.from(agentSource('plan'))),explorer_sha256:digest(Buffer.from(agentSource('explore'))),verifier_sha256:digest(Buffer.from(agentSource('verify'))),advance_sha256:digest(Buffer.from(agentSource('advance')))},metadata:info};
manifest.legacy_executable_sha256='2e189e11a9ceda2d4dfc2299e61892929dc696411d595b8e3c138c8134c2802f';
if(legacyArg&&legacyArg!=='-'){
  const oldImage=fs.readFileSync(path.resolve(legacyArg));
  if(digest(oldImage)!==manifest.legacy_executable_sha256)throw Error('Unreviewed E016 executable');
  fs.writeFileSync(path.join(out,'legacy-BBY-Q.rxe'),oldImage);
}
fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');
if(flags.includes('--install-assets')){
  const assets=path.join(repo,'reverse-os-android/app/src/main/assets'),target=path.join(assets,'bbyq-interface');fs.mkdirSync(target,{recursive:true});
  fs.mkdirSync(path.join(assets,'native-apps'),{recursive:true});
  fs.copyFileSync(path.join(out,'BBY-Q.rxe'),path.join(assets,'native-apps/BBY-Q.rxe'));
  fs.copyFileSync(path.join(out,'upgrade-e016.rxe'),path.join(target,'upgrade-e016.rxe'));
  fs.copyFileSync(path.join(out,'upgrade-control.rxe'),path.join(target,'upgrade-control.rxe'));
  fs.copyFileSync(path.join(out,'W4u.rxe'),path.join(target,'W4u.rxe'));
  fs.copyFileSync(path.join(root,'xi/consume.xi'),path.join(target,'consume.xi'));
  const feedback='#!reverse-pc-machine-v1\n'+link(agentSource('advance'),'advance_','feedback_verify')+
    'label feedback_verify\n'+link(agentSource('verify'),'verify_next_','feedback_speak')+
    'label feedback_speak\n'+link(source(path.join(root,'xi/speak.xi')),'feedback_','w4u_feedback_exit')+
    'label w4u_feedback_exit\nhalt\n';
  fs.writeFileSync(path.join(target,'feedback.xi'),feedback);
  fs.copyFileSync(path.join(out,'manifest.json'),path.join(target,'manifest.json'));
}
console.log(JSON.stringify(manifest,null,2));
