import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectTaskScheduling} from '../tools/reverseandroid-scheduler-group.mjs';

function fixture(){
  const start=0xffff000000100000n,mem=new Map(),strings=new Map();
  const at=n=>start+BigInt(n),put=(p,v)=>mem.set(p.toString(),v);
  const types=[null,{kind:2},{kind:1,type:8},{kind:4,name:'se'}];
  const member=(offset,type=1)=>({offset,type});
  const layouts={task_group:{parent:member(0),shares:member(8,2),css:member(16,3)},
    cgroup_subsys_state:{cgroup:member(0)},cgroup:{kn:member(0)},
    kernfs_node:{parent:member(0),name:member(8)},cfs_rq:{tg:member(0)},se:{cfs_rq:member(0)}};
  const task={sched_task_group:member(0),se:member(16,3)};
  const leader=at(0),group=at(256),root=at(512),rq=at(768);
  put(leader,group);put(leader+16n,rq);put(rq,group);
  for(const [tg,cg,kn,parent,nameAddress,name] of [[group,at(1024),at(1280),root,at(1792),'system'],[root,at(2048),at(2304),0n,at(2560),'']]){
    put(tg,parent);put(tg+8n,tg===group?1024n:2048n);put(tg+16n,cg);put(cg,kn);
    put(kn,tg===group?at(2304):0n);put(kn+8n,nameAddress);strings.set(nameAddress.toString(),name);
  }
  const api={task,types,read:p=>{assert.ok(mem.has(p.toString()),'Unmapped fixture read');return mem.get(p.toString());},text:p=>strings.get(p.toString()),type:name=>({name}),members:t=>layouts[t.name],resolveType:t=>t};
  return {api,leader,group,root,rq,put};
}
test('scheduler group and kernfs ancestry are read through verified task and runqueue anchors',()=>{
  const f=fixture(),result=inspectTaskScheduling(f.api,f.leader);
  assert.equal(result.task_and_runqueue_group_match,true);
  assert.deepEqual(result.groups.map(g=>[g.path,g.shares_raw]),[['/system','1024'],['/','2048']]);
});
test('a foreign runqueue cannot supply another task group',()=>{
  const f=fixture();f.put(f.rq,f.root);assert.throws(()=>inspectTaskScheduling(f.api,f.leader),/disagree/);
});
test('a cyclic scheduler ancestry is rejected without unbounded reads',()=>{
  const f=fixture();f.put(f.root,f.group);assert.throws(()=>inspectTaskScheduling(f.api,f.leader),/ancestry/);
});
