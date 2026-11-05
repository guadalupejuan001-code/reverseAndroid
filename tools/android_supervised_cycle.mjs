#!/usr/bin/env node
// Host supervision only. This module never interprets an instruction or edits
// a checkpoint. Process success is deliberately NOT an Android boot milestone.
import {spawn} from 'node:child_process';
import {mkdirSync,openSync,writeSync,closeSync,writeFileSync,renameSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {performance} from 'node:perf_hooks';

const bounded=(n,min,max,name)=>{
  if(!Number.isSafeInteger(n)||n<min||n>max)throw Error('Supervisor: invalid '+name);
  return n;
};
export async function superviseNativeCycle({command,args=[],cwd=process.cwd(),outputDirectory,
  initialInstructions=6350795356,startupMs=120000,noProgressMs=45000,wallMs=360000,
  graceMs=30000,intervalMs=200,maxLogBytes=8*1024*1024,onStatus=()=>{}}={}) {
  if(typeof command!=='string'||!command||!Array.isArray(args)||args.some(a=>typeof a!=='string')||
    typeof outputDirectory!=='string'||!outputDirectory)throw Error('Supervisor: command, args and outputDirectory are required');
  if(typeof onStatus!=='function')throw Error('Supervisor: onStatus must be a function');
  bounded(initialInstructions,0,Number.MAX_SAFE_INTEGER,'initialInstructions');
  for(const [key,n] of Object.entries({startupMs,noProgressMs,wallMs,graceMs}))bounded(n,1,3600000,key);
  bounded(intervalMs,5,1000,'intervalMs');bounded(maxLogBytes,128,64*1024*1024,'maxLogBytes');
  mkdirSync(outputDirectory,{recursive:true});
  const logs={};
  try {
    for(const [key,name] of [['stdout','native-stdout.log'],['stderr','progress.log']])
      logs[key]={fd:openSync(join(outputDirectory,name),'wx',0o600),seen:0,stored:0};
  }catch(error){for(const log of Object.values(logs))closeSync(log.fd);throw error;}
  const capture=(key,chunk)=>{
    const log=logs[key];log.seen+=chunk.length;
    const n=Math.min(chunk.length,maxLogBytes-log.stored);
    let at=0;while(at<n){const wrote=writeSync(log.fd,chunk,at,n-at);if(wrote<=0)throw Error('Supervisor: incomplete log write');at+=wrote;}
    log.stored+=n;
  };
  const started=performance.now();let lastProgress=started,best=initialInstructions;
  let markerSeen=false,trigger=null,forceKilled=false,spawnError=null,child,closed=false;
  let lastNotice=-Infinity;
  let timer,killTimer,line='',droppingLine=false,ioError=null;
  const grouped=process.platform!=='win32';
  const send=signal=>{
    if(!child?.pid)return;
    try{if(grouped)process.kill(-child.pid,signal);else child.kill(signal);}
    catch(error){if(error.code!=='ESRCH')ioError=error.message;}
  };
  const stop=reason=>{
    if(trigger||closed)return;
    trigger=reason;send('SIGTERM');
    killTimer=setTimeout(()=>{if(!closed){forceKilled=true;send('SIGKILL');}},graceMs);
  };
  const marker=text=>{
    const match=/^REVERSE_PCI_(?:CYCLE_HEARTBEAT|SAVED) (\{.*\})$/.exec(text);
    if(!match)return;
    try{
      const event=JSON.parse(match[1]),n=event.instructions;
      if(!Number.isSafeInteger(n)||n<initialInstructions)return;
      const now=performance.now();
      if(!markerSeen){markerSeen=true;lastProgress=now;}
      if(n>best){best=n;lastProgress=now;}
    }catch{ /* Unstructured diagnostic output never counts as progress. */ }
  };
  const parse=chunk=>{
    // A bounded parser must still drain all stderr, including oversized lines.
    const text=chunk.toString('utf8');
    for(const character of text){
      if(character==='\n'){if(!droppingLine)marker(line.replace(/\r$/,''));line='';droppingLine=false;}
      else if(!droppingLine){if(line.length>=65536){line='';droppingLine=true;}else line+=character;}
    }
  };
  const onTerm=()=>stop('host_sigterm');const onInt=()=>stop('host_sigint');
  let result;
  try {
    result=await new Promise((resolveResult,reject)=>{
      try{child=spawn(command,args,{cwd,detached:grouped,stdio:['ignore','pipe','pipe'],shell:false});}
      catch(error){reject(error);return;}
      process.on('SIGTERM',onTerm);process.on('SIGINT',onInt);
      for(const key of ['stdout','stderr'])child[key].on('data',chunk=>{
        try{capture(key,chunk);if(key==='stderr')parse(chunk);}
        catch(error){ioError=error.message;stop('host_log_error');}
      });
      child.on('error',error=>{spawnError=error.message;});
      timer=setInterval(()=>{
        const now=performance.now();
        if(now-lastNotice>=2000){
          lastNotice=now;
          try{onStatus({phase:trigger?'stopping':markerSeen?'native_activity_reported':'waiting_for_native_start',
            elapsed_ms:Math.round(now-started),last_reported_instructions:best,
            no_progress_ms:Math.round(now-lastProgress),android_boot_verified:false});}
          catch(error){ioError=error.message;stop('host_status_error');}
        }
        if(now-started>=wallMs)stop('host_watchdog_wall');
        else if(!markerSeen&&now-started>=startupMs)stop('host_watchdog_startup');
        else if(markerSeen&&now-lastProgress>=noProgressMs)stop('host_watchdog_no_progress');
      },intervalMs);
      child.once('close',(code,signal)=>{
        closed=true;clearInterval(timer);clearTimeout(killTimer);
        if(line&&!droppingLine)marker(line);
        resolveResult({schema:'reverse-c08-host-supervision-v1',
          status:spawnError?'spawn_failed':trigger?'host_stopped':code===0&&!ioError?'process_completed':'process_failed',
          trigger,exit_code:code,exit_signal:signal,forced_kill:forceKilled,
          elapsed_ms:Math.round(performance.now()-started),startup_marker_seen:markerSeen,
          last_reported_instructions:best,reported_instruction_delta:best-initialInstructions,
          error:spawnError??ioError,
          logs:Object.fromEntries(Object.entries(logs).map(([key,value])=>[key,{bytes_seen:value.seen,bytes_stored:value.stored,truncated:value.seen>value.stored}])),
          checkpoint_bytes_modified_by_supervisor:false,
          android_boot_verified:false,android_display_verified:false,native_android_games_verified:false});
      });
    });
  }finally{
    clearInterval(timer);clearTimeout(killTimer);
    process.off('SIGTERM',onTerm);process.off('SIGINT',onInt);
    for(const log of Object.values(logs))closeSync(log.fd);
  }
  const temporary=join(outputDirectory,'process-outcome.json.tmp');
  writeFileSync(temporary,JSON.stringify(result,null,2)+'\n',{flag:'wx',mode:0o600});
  renameSync(temporary,join(outputDirectory,'process-outcome.json'));
  return result;
}

if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const [root,separator,command,...args]=process.argv.slice(2);
    if(separator!=='--')throw Error('Usage: node tools/android_supervised_cycle.mjs output-directory -- executable arguments...');
    const result=await superviseNativeCycle({command,args,outputDirectory:root,
      onStatus:status=>process.stderr.write('REVERSE_HOST_SUPERVISOR '+JSON.stringify(status)+'\n')});
    console.log(JSON.stringify(result));
    if(result.status!=='process_completed')process.exitCode=2;
  }catch(error){console.error(error.message);process.exitCode=1;}
}
