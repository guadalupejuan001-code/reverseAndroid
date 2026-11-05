// Unit fixtures for the read-only Binder queue inspector, never guest emulation.
import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectBinderQueues} from '../tools/android_binder_snapshot_probe.mjs';
const global=0xffffffc0828abac0n,proc=0xffff800000100000n,thread=0xffff800000110000n,tx=0xffff800000120000n;
const layouts={
 binder_proc:{fields:{proc_node:0,threads:16,pid:64,tsk:72,is_dead:112,todo:160,
 waiting_threads:48,outstanding_txns:108,requested_threads:460,requested_threads_started:464,max_threads:456}},
 binder_thread:{fields:{rb_node:8,pid:48,looper:52,transaction_stack:64,todo:72,process_todo:88,is_dead:464}},
 rb_node:{fields:{rb_left:16,rb_right:8}},rb_root:{fields:{rb_node:0}},
 hlist_node:{fields:{next:0}},list_head:{fields:{next:0,prev:8}},
 binder_transaction:{fields:{debug_id:0,from_pid:48,from_tid:52,to_proc:64,code:104,flags:108,need_reply:88}},
 binder_work:{fields:{entry:0,type:16}}
};
function fixture(){
 const memory=new Map(),put=(a,x,n=8)=>{
  for(let i=0;i<n;i++)memory.set(a+BigInt(i),Number((BigInt(x)>>BigInt(8*i))&255n));
 };
 const read=(a,n=8)=>{let z=0n;for(let i=0;i<n;i++)z|=BigInt(memory.get(a+BigInt(i))??0)<<BigInt(i*8);return z;};
 put(global,proc);put(proc,0);put(proc+16n,thread+8n);put(proc+64n,554,4);put(proc+112n,0,1);
 put(proc+108n,1,4);put(proc+456n,15,4);put(proc+460n,0,4);put(proc+464n,0,4);
 put(proc+48n,proc+48n);put(proc+56n,proc+48n);
 put(proc+160n,proc+160n);put(proc+168n,proc+160n);
 put(thread+8n,0);put(thread+16n,0);put(thread+24n,0);put(thread+48n,554,4);put(thread+52n,0x10,4);
 put(thread+64n,tx);put(thread+72n,thread+72n);put(thread+80n,thread+72n);put(thread+88n,0,1);put(thread+464n,0,1);
 put(tx+0n,21,4);put(tx+48n,554,4);put(tx+52n,554,4);put(tx+64n,proc);put(tx+88n,1,1);put(tx+104n,7,4);put(tx+108n,0,4);
 return {memory,put,read,opts:{read,globalAddress:global,layouts,tasks:[{pid:554,name:'odsign'}]}};
}
test('reads one pending Binder transaction without touching memory',()=>{
 const f=fixture(),before=new Map(f.memory);
 const r=inspectBinderQueues(f.opts);
 assert.equal(r.process_count,1);
 assert.equal(r.focused_processes[0].name,'odsign');
 assert.equal(r.focused_processes[0].outstanding_txns,1);
 assert.equal(r.focused_processes[0].threads[0].loop_waiting,true);
 assert.deepEqual(r.focused_processes[0].threads[0].transaction,{pointer:'0x'+tx.toString(16),debug_id:21,from_pid:554,from_tid:554,to_pid:554,code:7,flags:0,need_reply:true});
 assert.deepEqual(f.memory,before);
});
test('empty Binder process queue is not an error, nor Zygote evidence',()=>{
 const f=fixture();f.put(global,0n);
 const r=inspectBinderQueues(f.opts);assert.equal(r.process_count,0);
 assert.deepEqual(r.focused_processes,[]);
});
test('detects HLIST loops rather than claiming idle',()=>{
 const f=fixture();f.put(proc,proc);
 assert.throws(()=>inspectBinderQueues(f.opts),/cycle/);
});
test('detects kernel pointer corruption',()=>{
 const f=fixture();f.put(global,0x1000n);
 assert.throws(()=>inspectBinderQueues(f.opts),/Binder process HLIST pointer/);
});
test('rejects unsupported BTF layout',()=>{
 const f=fixture(),broken={...layouts,binder_proc:{fields:{pid:64}}};
 assert.throws(()=>inspectBinderQueues({...f.opts,layouts:broken}),/BTF layout incomplete/);
});
