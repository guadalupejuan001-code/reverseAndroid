import assert from 'node:assert/strict';

const kernel=p=>p>=0xffff000000000000n;
const hex=p=>'0x'+p.toString(16);

// Read the authenticated kernel's BTF layouts. No scheduling operation is
// performed; paths, shares and quotas are observations, not requested values.
export function inspectTaskScheduling(api,pointer){
  const {read,text,types,type,members,resolveType,task}=api;
  const fields=name=>members(type(name));
  const ptr=(base,layout,key)=>{
    const field=layout[key];assert.ok(field,`Missing BTF pointer ${key}`);
    assert.equal(resolveType(types[field.type]).kind,2);
    return read(base+BigInt(field.offset));
  };
  const integer=(base,layout,key)=>{
    const field=layout[key];assert.ok(field,`Missing BTF integer ${key}`);
    const t=resolveType(types[field.type]);assert.equal(t.kind,1);assert.ok([1,2,4,8].includes(t.type));
    return read(base+BigInt(field.offset),t.type).toString();
  };
  const groupFields=fields('task_group'),cssFields=fields('cgroup_subsys_state');
  const cgroupFields=fields('cgroup'),knFields=fields('kernfs_node');
  const se=members(types[task.se.type]);
  const rq=ptr(pointer+BigInt(task.se.offset),se,'cfs_rq');assert.ok(kernel(rq));
  const groupFromRq=ptr(rq,fields('cfs_rq'),'tg');assert.ok(kernel(groupFromRq));
  const group=task.sched_task_group?ptr(pointer,task,'sched_task_group'):groupFromRq;
  assert.equal(group,groupFromRq,'Task and CFS runqueue disagree about the group');
  const groupPath=cgroup=>{
    let node=ptr(cgroup,cgroupFields,'kn');const seen=new Set(),names=[];
    while(node!==0n){
      assert.ok(kernel(node)&&!seen.has(node.toString())&&seen.size<64,'Invalid kernfs ancestry');seen.add(node.toString());
      const at=ptr(node,knFields,'name');assert.ok(kernel(at));
      const name=text(at,256);assert.ok(/^[A-Za-z0-9_.:@+/-]*$/.test(name));
      if(name&&name!=='/')names.push(name);
      node=ptr(node,knFields,'parent');
    }
    return '/'+names.reverse().join('/');
  };
  const chain=[],seen=new Set();
  for(let at=group;at!==0n;at=ptr(at,groupFields,'parent')){
    assert.ok(kernel(at)&&!seen.has(at.toString())&&seen.size<32,'Invalid scheduler-group ancestry');seen.add(at.toString());
    assert.ok(groupFields.css);
    const cgroup=ptr(at+BigInt(groupFields.css.offset),cssFields,'cgroup');assert.ok(kernel(cgroup));
    const row={task_group:hex(at),cgroup:hex(cgroup),path:groupPath(cgroup),shares_raw:integer(at,groupFields,'shares')};
    if(groupFields.cfs_bandwidth){
      const bandwidth=members(types[groupFields.cfs_bandwidth.type]);
      const base=at+BigInt(groupFields.cfs_bandwidth.offset);
      for(const key of ['quota','period'])if(bandwidth[key])row[key+'_raw']=integer(base,bandwidth,key);
    }
    chain.push(row);
  }
  const result={read_only:true,task_and_runqueue_group_match:true,ancestry_verified:true,groups:chain};
  if(task.cred)try{
    const cred=ptr(pointer,task,'cred');assert.ok(kernel(cred));const layout=fields('cred');result.credentials={};
    for(const key of ['uid','euid','gid','egid']){
      const field=layout[key];assert.ok(field);const id=members(types[field.type]);assert.ok(id.val&&id.val.offset===0);
      result.credentials[key]=integer(cred+BigInt(field.offset),id,'val');
    }
  }catch(e){result.credentials_unverified=e.message;}
  return result;
}
