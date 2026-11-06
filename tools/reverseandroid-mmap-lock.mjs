// BTF-derived, bounded, read-only mmap semaphore observations.
import assert from 'node:assert/strict';
const hx=value=>'0x'+value.toString(16),kernel=value=>value>=0xffff000000000000n;

export function inspectMmapLock(api,mm) {
  const {read,text,types,type,members,resolveType,task}=api;
  assert.ok(kernel(mm));
  const mmFields=members(type('mm_struct'));
  const fields=members(resolveType(types[mmFields.mmap_lock.type]));
  const at=mm+BigInt(mmFields.mmap_lock.offset);
  const atomic=field=>{
    const inner=resolveType(types[field.type]);assert.equal(inner.type,8,'Expected an eight-byte atomic long');
    const counter=members(inner).counter;assert.ok(counter&&counter.offset===0,'Expected BTF atomic counter');
    return read(at+BigInt(field.offset+counter.offset));
  };
  const count=atomic(fields.count),owner=atomic(fields.owner);
  const taskAt=pointer=>{
    assert.ok(kernel(pointer),'Invalid task pointer');
    const get=(name,n=4)=>read(pointer+BigInt(task[name].offset),n);
    const pid=Number(get('pid')),tgid=Number(get('tgid'));assert.ok(pid>=0&&pid<16777216&&tgid>=0&&tgid<16777216);
    const leader=get('group_leader',8);assert.ok(kernel(leader));assert.equal(Number(read(leader+BigInt(task.pid.offset),4)),tgid,'Task leader/TGID mismatch');
    const name=text(pointer+BigInt(task.comm.offset),16);assert.ok(name.length>0&&/^[\x20-\x7e]+$/.test(name));
    return {task:hx(pointer),pid,tgid,name,state:Number(get('__state')),exit_state:Number(get('exit_state')),
      on_rq:Number(get('on_rq')),on_cpu:Number(get('on_cpu')),static_priority:Number(get('static_prio')),
      mm:hx(get('mm',8)),group_leader:hx(leader)};
  };
  const result={read_only:true,kernel_BTF_layout_verified:true,mm:hx(mm),mmap_lock:hx(at),
    raw_count:hx(count),raw_owner:hx(owner),candidate_owner_flags_low2:Number(owner&3n),
    reader_owner_flag_bit0:Boolean(owner&1n),writer_locked_count_bit0:Boolean(count&1n),
    owner_with_reader_flag_is_only_a_hint_not_proof_of_current_or_sole_holder:true,
    observed_layout_offsets:Object.fromEntries(['count','owner','wait_list'].map(key=>[key,fields[key]?.offset])),waiters:[]};
  const pointer=owner&~3n;
  if(kernel(pointer))try{result.owner_task_or_reader_hint=taskAt(pointer);}catch(error){result.owner_task_unverified=error.message;}
  try {
    const waiter=members(type('rwsem_waiter'));assert.ok(waiter.list&&waiter.task&&waiter.type);
    const head=at+BigInt(fields.wait_list.offset),seen=new Set();
    for(let link=read(head);link!==head;link=read(link)) {
      assert.ok(kernel(link)&&!seen.has(link.toString())&&seen.size<2048,'Invalid bounded semaphore waiter list');seen.add(link.toString());
      const next=read(link),previous=read(link+8n);assert.equal(read(next+8n),link,'Waiter forward backlink');assert.equal(read(previous),link,'Waiter reverse backlink');
      const base=link-BigInt(waiter.list.offset);
      result.waiters.push({...taskAt(read(base+BigInt(waiter.task.offset))),waiter_type_raw:Number(read(base+BigInt(waiter.type.offset),4))});
    }
    result.complete_waiter_list_backlinks_verified=true;
  }catch(error){result.waiters_unverified=error.message;}
  return result;
}
