import test from 'node:test';
import assert from 'node:assert/strict';
import {enumerateThreadPointers} from '../tools/reverseandroid-thread-probe.mjs';
function fixture(){
  const memory=new Map(),leader=0xffff000000010000n,other=leader+4096n,signalPointer=0xffff000000030000n;
  const task={signal:{offset:0},thread_node:{offset:16},group_leader:{offset:32}},signal={thread_head:{offset:16}};
  const head=signalPointer+16n,first=leader+16n,last=other+16n;
  const write=(a,v)=>memory.set(a.toString(),v);
  for(const p of [leader,other]){write(p,signalPointer);write(p+32n,leader);}
  write(head,first);write(head+8n,last);write(first,last);write(first+8n,head);write(last,head);write(last+8n,first);
  const read=a=>{assert.ok(memory.has(a.toString()),'Unmapped fixture address');return memory.get(a.toString());};
  return {leader,other,read,write,task,signal,last,first};
}
test('verified signal thread list includes leader and worker once',()=>{
  const f=fixture();assert.deepEqual(enumerateThreadPointers(f),[f.leader,f.other]);
});
test('foreign thread leader is rejected',()=>{
  const f=fixture();f.write(f.other+32n,f.other);assert.throws(()=>enumerateThreadPointers(f),/Foreign thread leader/);
});
test('foreign signal and inconsistent cyclic links are rejected',()=>{
  const f=fixture();f.write(f.other,0xffff000000050000n);assert.throws(()=>enumerateThreadPointers(f),/Foreign thread signal/);
  const g=fixture();g.write(g.last,g.first);assert.throws(()=>enumerateThreadPointers(g),/backlink|cycle/);
});
