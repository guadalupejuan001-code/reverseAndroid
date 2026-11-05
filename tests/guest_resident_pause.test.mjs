import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';
const source=readFileSync(new URL('../reverse-os-android/app/src/main/assets/workbench/guest.js',import.meta.url),'utf8');
const deferred=()=>{let resolve,reject;const promise=new Promise((a,b)=>{resolve=a;reject=b;});return {promise,resolve,reject};};
const tick=async()=>{for(let i=0;i<12;i++)await Promise.resolve();};
function context(request,extra={}){
 const c=vm.createContext({nativeAvailable:true,nativeRequest:request,renderWindowContent(){},
  state:{space:0,windows:[{app:'xi',xiTab:'arm64',min:false,space:0}]},
  document:{hidden:false,body:{dataset:{}},addEventListener(){}},setTimeout,clearTimeout,...extra});
 c.window=c;vm.runInContext(source,c);vm.runInContext("guestUi.snapshot={loaded:true,guest:{halted:false,instructions:0,display_width:0}}",c);return c;
}
const state=(instructions,extra={})=>({ok:true,loaded:true,guest:{instructions,halted:false,display_width:0},...extra});
test('pause waits for the accepted block and drains repeated lifecycle requests',async()=>{
 const run=deferred(),pause=deferred(),calls=[];let pauses=0;
 const c=context((op)=>{calls.push(op);if(op==='guestRunContinuous')return run.promise;if(op==='guestPause')return ++pauses===1?pause.promise:Promise.resolve({ok:true,paused:true});throw Error('Unexpected '+op);});
 const executing=vm.runInContext("guestUi.running=true;guestOperation('guestRunContinuous')",c);
 await tick();await vm.runInContext('ReverseGuest.pause()',c);assert.deepEqual(calls,['guestRunContinuous']);
 run.resolve(state(250000,{checkpoint_dirty:true}));await tick();assert.deepEqual(calls,['guestRunContinuous','guestPause']);
 await vm.runInContext('ReverseGuest.pause()',c);
 pause.resolve(state(250000,{paused:true,checkpoint_dirty:false,resident:false}));await executing;
 const ui=vm.runInContext('guestUi',c);assert.equal(ui.busy,false);assert.equal(ui.pausePending,false);assert.equal(ui.running,false);assert.equal(ui.snapshot.guest.instructions,250000);assert.equal(pauses,2);
});
test('flushing on stop preserves the original execution error',async()=>{
 const c=context(async op=>{if(op==='guestRunContinuous')throw Error('Unsupported AArch64 instruction');return state(0,{paused:true});});
 vm.runInContext('guestUi.pausePending=true',c);await vm.runInContext("guestOperation('guestRunContinuous')",c);
 assert.match(vm.runInContext('guestUi.error',c),/Unsupported AArch64/);assert.equal(vm.runInContext('guestUi.busy',c),false);
});
test('a paused result does not reopen the guest just to request its frame',async()=>{
 const gate=deferred(),calls=[];const c=context(op=>{calls.push(op);if(op==='guestRunContinuous')return gate.promise;return Promise.resolve({...state(1,{paused:true}),guest:{instructions:1,display_width:32,display_frame:2}});});
 const executing=vm.runInContext("guestOperation('guestRunContinuous')",c);await tick();await vm.runInContext('ReverseGuest.pause()',c);
 gate.resolve({...state(1),guest:{instructions:1,display_width:32,display_frame:2}});await executing;assert.deepEqual(calls,['guestRunContinuous','guestPause']);
});
test('pause between blocks prevents another CPU request',async()=>{
 let wake;const calls=[];const c=context(async op=>{calls.push(op);return state(250000,op==='guestPause'?{paused:true}:{checkpoint_dirty:true});},{setTimeout(fn){wake=fn;return 1;}});
 const executing=vm.runInContext('continueGuest()',c);await tick();assert.equal(typeof wake,'function');
 await vm.runInContext('ReverseGuest.pause()',c);wake();await executing;
 assert.equal(calls.filter(x=>x==='guestRunContinuous').length,1);assert.equal(vm.runInContext('guestUi.pausePending',c),false);
});
test('preview pause does not leave an unfulfillable native request pending',async()=>{
 const c=context(()=>{throw Error('Should not request native');},{nativeAvailable:false});await vm.runInContext('ReverseGuest.pause()',c);assert.equal(vm.runInContext('guestUi.pausePending',c),false);
});
test('foreground inspection waits for the pause publication and never executes CPU',async()=>{
 const gate=deferred(),calls=[];const c=context(op=>{calls.push(op);return op==='guestPause'?gate.promise:Promise.resolve(state(250000,{checkpoint_dirty:false,resident:true}));});
 const pausing=vm.runInContext('ReverseGuest.pause()',c);await tick();await vm.runInContext('ReverseGuest.resume()',c);
 assert.deepEqual(calls,['guestPause']);gate.resolve(state(250000,{paused:true,checkpoint_dirty:false,resident:false}));await pausing;
 assert.deepEqual(calls,['guestPause','guestInspect']);assert.equal(vm.runInContext('guestUi.snapshot.guest.instructions',c),250000);assert.equal(vm.runInContext('guestUi.resumePending',c),false);
});
