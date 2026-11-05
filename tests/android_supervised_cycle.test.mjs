// These are actual HOST subprocess tests. They do NOT emulate or boot Android.
import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync,readFileSync,writeFileSync,statSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {superviseNativeCycle} from '../tools/android_supervised_cycle.mjs';
const initial=6350795356;
const save=`process.stderr.write('REVERSE_PCI_SAVED '+JSON.stringify({instructions:${initial},generation:1})+'\\n');`;
async function run(t,code,options={}){
  const dir=options.outputDirectory??mkdtempSync(join(tmpdir(),'reverse-supervisor-'));t.after(()=>rmSync(dir,{recursive:true,force:true}));
  const result=await superviseNativeCycle({command:process.execPath,args:['-e',code],outputDirectory:dir,
    startupMs:1000,noProgressMs:200,wallMs:2000,graceMs:100,intervalMs:10,...options});
  assert.deepEqual(JSON.parse(readFileSync(join(dir,'process-outcome.json'))),result);
  assert.equal(result.android_boot_verified,false);assert.equal(result.checkpoint_bytes_modified_by_supervisor,false);
  return {result,dir};
}
test('completed host process is not certified as an Android boot',async t=>{
  const {result}=await run(t,"console.log('host fixture only')");assert.equal(result.status,'process_completed');
});
test('nonzero child status remains failure',async t=>{const {result}=await run(t,'process.exitCode=9');assert.equal(result.exit_code,9);assert.equal(result.status,'process_failed');});
test('missing native binary fails without pretending to execute',async t=>{const {result}=await run(t,'',{command:'/this-path-does-not-exist/reverse-native-cycle'});assert.equal(result.status,'spawn_failed');assert.equal(result.reported_instruction_delta,0);});
test('silent startup cannot wait indefinitely',async t=>{const {result}=await run(t,'setInterval(()=>{},1000)',{startupMs:200});assert.equal(result.trigger,'host_watchdog_startup');assert.equal(result.status,'host_stopped');});
test('stationary heartbeat is not instruction progress',async t=>{
  const {result}=await run(t,save+`setInterval(()=>{${save}},20)`);
  assert.equal(result.trigger,'host_watchdog_no_progress');assert.equal(result.reported_instruction_delta,0);
});
test('increasing heartbeat still obeys an absolute wall bound',async t=>{
  const code=save+`let n=${initial};setInterval(()=>process.stderr.write('REVERSE_PCI_CYCLE_HEARTBEAT '+JSON.stringify({instructions:++n})+'\\n'),20);`;
  const {result}=await run(t,code,{wallMs:450});assert.equal(result.trigger,'host_watchdog_wall');assert.ok(result.reported_instruction_delta>0);
});
test('TERM allows an orderly child exit',async t=>{
  const {result,dir}=await run(t,`process.on('SIGTERM',()=>{console.log('orderly shutdown');process.exit(0);});`+save+'setInterval(()=>{},1000)');
  assert.equal(result.forced_kill,false);assert.equal(result.trigger,'host_watchdog_no_progress');
  assert.match(readFileSync(join(dir,'native-stdout.log'),'utf8'),/orderly shutdown/);
});
test('unresponsive child is killed after the grace period',async t=>{
  const {result}=await run(t,`process.on('SIGTERM',()=>{});`+save+'setInterval(()=>{},1000)');
  assert.equal(result.forced_kill,true);assert.equal(result.exit_signal,'SIGKILL');
});
test('logs remain bounded even with oversized output',async t=>{
  const {result,dir}=await run(t,"process.stdout.write('x'.repeat(100000));process.stderr.write('y'.repeat(100000));",{maxLogBytes:1024});
  assert.equal(result.status,'process_completed');for(const key of ['stdout','stderr']){assert.equal(result.logs[key].bytes_stored,1024);assert.equal(result.logs[key].truncated,true);}
  assert.equal(statSync(join(dir,'progress.log')).size,1024);
});
test('preexisting checkpoint is not touched while a process hangs',async t=>{
  const dir=mkdtempSync(join(tmpdir(),'reverse-preserve-'));t.after(()=>rmSync(dir,{recursive:true,force:true}));
  const file=join(dir,'current.rvg'),sentinel=Buffer.from('Explicit synthetic host fixture; not an RVG');writeFileSync(file,sentinel);
  const {result}=await run(t,save+'setInterval(()=>{},1000)',{outputDirectory:dir});
  assert.equal(result.trigger,'host_watchdog_no_progress');assert.deepEqual(readFileSync(file),sentinel);
});
test('invalid durations fail before spawning a process',async()=>{
  for(const value of [0,-1,NaN,Infinity,1.5])await assert.rejects(superviseNativeCycle({command:process.execPath,outputDirectory:'/unused',wallMs:value}),/invalid wallMs/);
});
test('live host status is emitted while waiting without claiming a boot',async t=>{
  const statuses=[];const {result}=await run(t,save+'setInterval(()=>{},1000)',{onStatus:status=>statuses.push(status)});
  assert.ok(statuses.length>0);assert.equal(statuses[0].android_boot_verified,false);assert.equal(result.trigger,'host_watchdog_no_progress');
});
