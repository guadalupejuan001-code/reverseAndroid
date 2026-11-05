// Browser test I/O fixture. The decisions, learning and execution use real XI.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),cp=require('node:child_process');
module.exports=function nativeHost({root,build,runtime,probe,directory}){
  fs.mkdirSync(directory,{recursive:true});const A=8200000000000000000n,M=8300000000000000000n;
  const image=path.join(build,'BBY-Q.rxe'),agent=path.join(directory,'agent.rvp'),session=path.join(directory,'session.rvp'),step=path.join(directory,'event.xi'),program=path.join(directory,'probe.rxe');
  const run=(...args)=>cp.execFileSync(runtime,args,{encoding:'utf8',maxBuffer:32*1024*1024,stdio:['ignore','pipe','pipe']});
  const sha=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
  const inspect=p=>JSON.parse(cp.execFileSync(probe,[p],{encoding:'utf8'}));
  const write=code=>fs.writeFileSync(step,'#!reverse-pc-machine-v1\n'+code);
  const body=p=>fs.readFileSync(p,'utf8').split('\n').slice(1).join('\n');
  const put=(a,v)=>`store8 ${a} ${v}\n`;
  fs.copyFileSync(path.join(build,'controller.rvp'),agent);run('init',session);
  write('load8 r0 9001\nadd r0 1\nstore8 9001 r0\nprint r0\nhalt\n');run('compile',step,program,'Prueba interna');
  const meta=JSON.parse(run('inspect-rxe',image)),identity=sha(image);
  const apps=[{path:'Aplicaciones/BBY-Q.rxe',identity,metadata:meta,bytes:fs.statSync(image).size},{path:'Aplicaciones/Prueba.rxe',identity:sha(program),metadata:JSON.parse(run('inspect-rxe',program)),bytes:fs.statSync(program).size}];
  let history=[],pending=null,language=1,profile=null,turns=0,runs=0,experiments=0,output='';
  function memory(){const b=fs.readFileSync(agent),n=Number(b.readBigUInt64LE(5)),m=new Map();for(const row of b.subarray(29+n).toString().split('\n')){const [k,a,v]=row.split(' ');if(k==='O')m.set(a,Number(v));}return o=>m.get(String(M+BigInt(o)))||0;}
  function trace(){const at=memory();return {engine:'W4u',status:at(16),goal:at(17),target:at(18),steps:at(19),completed:at(20),planner:at(32),explorer:at(33),verifier:at(34)};}
  function observe(d){
    let code=put(A+16n,1)+put(A+17n,d.active)+put(A+18n,d.count)+put(M+710n,d.space)+put(M+711n,d.minimizedCount);
    for(let i=0;i<32;i++)code+=put(A+32n+BigInt(i),d.present[i]||0)+put(M+640n+BigInt(i),d.minimized[i]||0)+put(M+672n+BigInt(i),d.maximized[i]||0)+put(M+512n+BigInt(i),0);
    for(const app of d.programs)code+=put(M+512n+BigInt(app.target),app.capability);
    return code;
  }
  function selected(view){write(`load8 r0 ${A+64n}\nprint r0\nload8 r1 ${A+65n}\nprint r1\n`+body(path.join(root,'bbyq-interface/xi/consume.xi')));const v=run('run',step,agent,agent).split('\n').filter(s=>s.startsWith('{')).map(s=>JSON.parse(s).value);return v[0]?{op:v[0],target:v[1],ticket:crypto.randomUUID(),view}:null;}
  const snapshot=()=>({ok:true,...apps[0],desktop_control:true,history:structuredClone(history),language,agents:trace()});
  async function request(operation,raw=''){
    if(operation==='inspect')return {ok:true,mode:'native-test-transport',core:inspect(session),android:{model:'Prueba de interfaz',android:'fixture',hardware:'fixture',soc:null,logical_processors:1,ram_total_bytes:0,ram_available_bytes:0,process_pss_kib:0,process_cpu_ms:0},package:'com.reversepc.control.workbench',state_bytes:fs.statSync(session).size,profile_bytes:profile?.length||0,captured_at_ms:Date.now(),build:{source_commit:'test fixture; native XI backend'}};
    if(operation==='saveProfile'){profile=raw;return {ok:true};}
    if(operation==='listApps')return {ok:true,apps,rejected:0};
    if(operation==='execute'){write(raw);const text=run('run',step,session,session);return {ok:true,output:text,elapsed_ns:1,source:'native XI'};}
    if(operation==='experiment'){
      const state=path.join(directory,'experiment.rvp');run('init',state);write(raw);const text=run('run',step,state,state);experiments++;
      return {ok:true,output:text,core:inspect(state),isolated:true};
    }
    const p=JSON.parse(raw||'{}');
    if(operation==='openApp')return p.path===apps[0].path?snapshot():{ok:true,...apps[1],output};
    if(operation==='runApp'){if(p.identity!==apps[1].identity)throw Error('Wrong executable');output=run('run',program,session,session);runs++;return {ok:true,output};}
    if(operation==='askApp'){
      language=p.language;write(observe(p.desktop)+'halt\n');run('run',step,agent,agent);
      const answer=run('app-turn',image,agent,agent,language===1?'es':'en',p.text).trim();turns++;
      history.push({role:'user',text:p.text},{role:'assistant',text:answer});pending=selected(p.desktop.view);
      return {...snapshot(),...(pending?{control:pending}:{})};
    }
    if(operation==='ackControl'){
      if(!pending||pending.ticket!==p.ticket)throw Error('Duplicate/foreign acknowledgement');
      write(put(7400000000000000003n,language)+put(M+80n,p.success?1:0)+observe(p.desktop)+body(path.join(root,'reverse-os-android/app/src/main/assets/bbyq-interface/feedback.xi')));
      const lines=run('run',step,agent,agent).split('\n').filter(s=>s.startsWith('{')).map(s=>JSON.parse(s));history.at(-1).text=Buffer.from(lines.filter(s=>s.register==='r14').map(s=>s.value)).toString('utf8');pending=selected(p.desktop.view);
      return {...snapshot(),...(pending?{control:pending}:{})};
    }
    throw Error('Unsupported test operation '+operation);
  }
  return {request,profile:()=>profile,sessionHash:()=>sha(session),stats:()=>({turns,runs,experiments,core:inspect(session),history:structuredClone(history)})};
};
