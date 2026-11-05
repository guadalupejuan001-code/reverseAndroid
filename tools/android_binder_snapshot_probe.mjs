// Binder read-only queue diagnostics on the authenticated *paused* Android GKI.
// Uses BTF offsets from that exact kernel; no Linux syscalls into the guest,
// no Binder transactions, no guest state mutations or fabricated service data.
import assert from 'node:assert/strict';
const MASK=0xffffffffffffffffn;
const ptr=p=>typeof p==='bigint'&&p>=0xffff000000000000n&&p<=MASK&&(p&7n)===0n;
const hex=p=>'0x'+p.toString(16);
function fields(layouts,key,names){
  const f=layouts[key]?.fields;
  assert.ok(f&&names.every(n=>Number.isSafeInteger(f[n])&&f[n]>=0),key+' BTF layout incomplete');
  return f;
}
export function inspectBinderQueues({read,globalAddress,layouts,tasks,maxProcs=128,maxThreads=1024}){
  assert.equal(typeof read,'function');
  assert.ok(ptr(globalAddress),'Invalid binder_procs symbol');
  assert.ok(Number.isInteger(maxProcs)&&maxProcs>=1&&maxProcs<=512);
  assert.ok(Number.isInteger(maxThreads)&&maxThreads>=1&&maxThreads<=2048);
  const p=fields(layouts,'binder_proc',['proc_node','threads','pid','tsk','is_dead','todo','waiting_threads','outstanding_txns','requested_threads','requested_threads_started','max_threads']);
  const t=fields(layouts,'binder_thread',['rb_node','pid','looper','transaction_stack','todo','process_todo','is_dead']);
  const rb=fields(layouts,'rb_node',['rb_left','rb_right']),root=fields(layouts,'rb_root',['rb_node']);
  const hl=fields(layouts,'hlist_node',['next']),list=fields(layouts,'list_head',['next','prev']);
  const tx=fields(layouts,'binder_transaction',['debug_id','from_pid','from_tid','to_proc','code','flags','need_reply']);
  const work=fields(layouts,'binder_work',['entry','type']);
  const rd=(address,n=8)=>{assert.ok(ptr(address&~7n),'Untrusted Binder kernel memory pointer');return read(address,n);};
  const num=(a,n=4)=>Number(rd(a,n));
  const listRead=(head,kind)=>{
    const seen=new Set();let node=rd(head+BigInt(list.next));const types=[];
    while(node!==head){
      assert.ok(ptr(node),'Bad Binder list pointer');
      assert.ok(!seen.has(hex(node))&&seen.size<maxThreads,'Binder list cycle or bound');
      seen.add(hex(node));
      if(kind==='work'){const wk=node-BigInt(work.entry);types.push(num(wk+BigInt(work.type)));}
      node=rd(node+BigInt(list.next));
    }
    return {count:seen.size,...(kind==='work'?{types}:{} )};
  };
  const taskName=pid=>tasks.find(x=>x.pid===pid)?.name??null;
  const describeTransaction=(address)=>{
    if(address===0n)return null;
    assert.ok(ptr(address),'Invalid Binder transaction pointer');
    const target=rd(address+BigInt(tx.to_proc));
    const toPid=target===0n?null:num(target+BigInt(p.pid));
    return {pointer:hex(address),debug_id:num(address+BigInt(tx.debug_id)),
      from_pid:num(address+BigInt(tx.from_pid)),
      from_tid:num(address+BigInt(tx.from_tid)),
      to_pid:toPid,code:num(address+BigInt(tx.code)),
      flags:num(address+BigInt(tx.flags)),need_reply:num(address+BigInt(tx.need_reply),1)!==0};
  };
  const describeThreads=proc=>{
    const seen=new Set(),output=[],stack=[rd(proc+BigInt(p.threads+root.rb_node))];
    while(stack.length){
      const n=stack.pop();if(!n)continue;
      assert.ok(ptr(n),'Invalid Binder thread RB pointer');
      assert.ok(!seen.has(hex(n))&&seen.size<maxThreads,'Binder RB cycle or bound');
      seen.add(hex(n));
      const base=n-BigInt(t.rb_node),pid=num(base+BigInt(t.pid));
      assert.ok(pid>0,'Bad Binder thread PID');
      const looper=num(base+BigInt(t.looper));
      output.push({pid,name:taskName(pid),looper,loop_waiting:!!(looper&0x10),
        process_todo:num(base+BigInt(t.process_todo),1)!==0,
        is_dead:num(base+BigInt(t.is_dead),1)!==0,
        work:listRead(base+BigInt(t.todo),'work'),
        transaction:describeTransaction(rd(base+BigInt(t.transaction_stack)))
      });
      stack.push(rd(n+BigInt(rb.rb_left)),rd(n+BigInt(rb.rb_right)));
    }
    return output.sort((a,b)=>a.pid-b.pid);
  };
  const seen=new Set(),processes=[];let next=rd(globalAddress);
  while(next!==0n){
    assert.ok(ptr(next),'Invalid Binder process HLIST pointer');
    assert.ok(!seen.has(hex(next))&&processes.length<maxProcs,'Binder process HLIST cycle or bound');
    seen.add(hex(next));
    const base=next-BigInt(p.proc_node),pid=num(base+BigInt(p.pid));
    assert.ok(pid>0,'Bad Binder process PID');
    const threads=describeThreads(base);
    processes.push({pid,name:taskName(pid),is_dead:num(base+BigInt(p.is_dead),1)!==0,
      outstanding_txns:num(base+BigInt(p.outstanding_txns)),
      max_threads:num(base+BigInt(p.max_threads)),
      requested_threads:num(base+BigInt(p.requested_threads)),
      requested_threads_started:num(base+BigInt(p.requested_threads_started)),
      waiting_threads:listRead(base+BigInt(p.waiting_threads),'waiting'),
      work:listRead(base+BigInt(p.todo),'work'),threads});
    next=rd(next+BigInt(hl.next));
  }
  processes.sort((a,b)=>a.pid-b.pid);
  const focused=processes.filter(x=>/odsign|keystore|keymint|android.hardwar|servicemanager|hwservicemanager/.test(x.name??''));
  const pending=processes.filter(x=>x.outstanding_txns>0||x.work.count>0||x.threads.some(t=>t.transaction||t.work.count>0));
  return {schema:'reverse-binder-live-queues-v1',source:'authenticated_kernel_btf_and_binder_procs',read_only:true,
    global_address:hex(globalAddress),process_count:processes.length,
    focused_processes:focused,other_pending:pending.filter(x=>!focused.includes(x)),
    all_processes:processes};
}
