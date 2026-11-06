import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectMmapLock} from '../tools/reverseandroid-mmap-lock.mjs';

function fixture() {
  const words=new Map(),base=0xffff000000000000n,mm=base+0x20000n,owner=base+0x10000n,waiting=base+0x40000n,link=base+0x30000n,head=mm+48n;
  const put=(at,value)=>words.set(at.toString(),BigInt(value));
  const names=new Map();
  const task=Object.fromEntries(Object.entries({pid:0,tgid:4,group_leader:8,__state:16,exit_state:20,on_rq:24,on_cpu:28,static_prio:32,mm:40,comm:48}).map(([key,offset])=>[key,{offset}]));
  for(const [at,pid,name] of [[owner,34,'khugepaged'],[waiting,8085,'package-parsing']]){
    for(const key of ['pid','tgid'])put(at+BigInt(task[key].offset),pid);
    put(at+8n,at);put(at+16n,pid===34?0:258);put(at+20n,0);put(at+24n,pid===34?1:0);put(at+28n,0);put(at+32n,pid===34?139:120);put(at+40n,pid===34?0:mm);names.set((at+48n).toString(),name);
  }
  put(mm+32n,1);put(mm+40n,owner);put(head,link);put(head+8n,link);put(link,head);put(link+8n,head);put(link+16n,waiting);put(link+24n,1);
  const atomic={type:8,fields:{counter:{offset:0}}};
  const layouts={mm_struct:{fields:{mmap_lock:{offset:32,type:1}}},rwsem_waiter:{fields:{list:{offset:0},task:{offset:16},type:{offset:24}}}};
  const types=[null,{fields:{count:{offset:0,type:2},owner:{offset:8,type:2},wait_list:{offset:16}}},atomic];
  const api={read(at){assert.ok(words.has(at.toString()),'Unmapped kernel word');return words.get(at.toString());},text(at){return names.get(at.toString());},types,type(name){assert.ok(layouts[name]);return layouts[name];},members(t){return t.fields;},resolveType(t){return t;},task};
  return {words,put,api,mm,owner,head,link};
}

test('reads a BTF-derived owner and full linked waiter list without writing memory',()=>{
  const {words,api,mm}=fixture(),before=new Map(words),r=inspectMmapLock(api,mm);
  assert.equal(r.raw_count,'0x1');assert.equal(r.owner_task_or_reader_hint.pid,34);
  assert.equal(r.writer_locked_count_bit0,true);assert.equal(r.complete_waiter_list_backlinks_verified,true);
  assert.deepEqual(r.waiters.map(t=>t.pid),[8085]);assert.deepEqual(words,before);
});

test('a reader hint never establishes the current sole holder, including an empty semaphore',()=>{
  const {put,api,mm,owner,head}=fixture();put(mm+32n,256);put(mm+40n,owner|1n);
  let r=inspectMmapLock(api,mm);assert.equal(r.reader_owner_flag_bit0,true);assert.equal(r.owner_with_reader_flag_is_only_a_hint_not_proof_of_current_or_sole_holder,true);
  put(mm+32n,0);put(mm+40n,1);put(head,head);put(head+8n,head);
  r=inspectMmapLock(api,mm);assert.equal(r.owner_task_or_reader_hint,undefined);assert.deepEqual(r.waiters,[]);
});

test('corrupt waiter backlinks remain explicitly unverified',()=>{
  const {put,api,mm,link}=fixture();put(link+8n,link);
  const r=inspectMmapLock(api,mm);assert.notEqual(r.complete_waiter_list_backlinks_verified,true);assert.match(r.waiters_unverified,/backlink/);
});
