import assert from 'node:assert/strict';

const kernel=p=>p>=0xffff000000000000n;
const hex=p=>'0x'+p.toString(16);

// Supplemental observation only. Every offset and width comes from the exact
// authenticated kernel's BTF, rather than a host Linux structure definition.
export function inspectTaskSignals(api,pointer){
  const {read,text,types,members,resolveType,task}=api;
  assert.ok(kernel(pointer),'Invalid task pointer');
  const descriptor=(layout,key)=>{
    const field=layout[key];assert.ok(field,`Missing BTF field ${key}`);
    assert.ok(Number.isInteger(field.offset),'Unsupported BTF bitfield');
    return {...field,resolved:resolveType(types[field.type])};
  };
  const integer=(base,layout,key,signed=false)=>{
    const field=descriptor(layout,key),t=field.resolved;
    assert.ok([1,6].includes(t.kind)&&[1,2,4,8].includes(t.type),'Unsupported BTF integer');
    const value=read(base+BigInt(field.offset),t.type);
    return (signed?BigInt.asIntN(t.type*8,value):value).toString();
  };
  const ptr=(base,layout,key)=>{
    const field=descriptor(layout,key);assert.equal(field.resolved.kind,2);
    return read(base+BigInt(field.offset),8);
  };
  const identity=at=>{
    assert.ok(kernel(at),'Invalid related task pointer');
    return {task:hex(at),pid:Number(integer(at,task,'pid')),tgid:Number(integer(at,task,'tgid')),
      name:text(at+BigInt(task.comm.offset),16)};
  };
  const mask=(base,layout,key)=>{
    const field=descriptor(layout,key),fields=members(field.resolved);
    const array=descriptor(fields,'sig');assert.equal(array.resolved.kind,3);
    const element=resolveType(types[api.kernel.readUInt32LE(array.resolved.at+12)]);
    const count=api.kernel.readUInt32LE(array.resolved.at+20);
    assert.equal(element.kind,1);assert.ok([4,8].includes(element.type)&&count>0&&count<=4);
    return Array.from({length:count},(_,i)=>hex(read(base+BigInt(field.offset+array.offset+i*element.type),element.type)));
  };
  const result={read_only:true,kernel_BTF_layout_verified:true};
  for(const key of ['ptrace','jobctl','exit_code','exit_signal'])if(task[key])result[key+'_raw']=integer(pointer,task,key);
  for(const key of ['parent','real_parent'])if(task[key])result[key]=identity(ptr(pointer,task,key));
  for(const key of ['blocked','real_blocked','saved_sigmask'])if(task[key])result[key+'_words']=mask(pointer,task,key);
  if(task.pending){const f=descriptor(task,'pending');result.pending_words=mask(pointer+BigInt(f.offset),members(f.resolved),'signal');}
  if(task.signal){
    const owner=ptr(pointer,task,'signal');assert.ok(kernel(owner),'Invalid signal owner pointer');
    const f=descriptor(task,'signal'),fields=members(resolveType(types[f.resolved.type]));
    result.signal_owner=hex(owner);
    for(const key of ['flags','group_exit_code','group_stop_count','notify_count'])if(fields[key])result[key+'_raw']=integer(owner,fields,key);
    if(fields.shared_pending){const pending=descriptor(fields,'shared_pending');result.shared_pending_words=mask(owner+BigInt(pending.offset),members(pending.resolved),'signal');}
  }
  if(task.last_siginfo){
    const at=ptr(pointer,task,'last_siginfo');result.last_siginfo_pointer=hex(at);
    if(at!==0n){
      assert.ok(kernel(at),'Invalid siginfo pointer');
      const f=descriptor(task,'last_siginfo'),t=resolveType(types[f.resolved.type]),fields=members(t);
      assert.ok(t.type>=12&&t.type<=128,'Unexpected kernel siginfo size');
      result.last_siginfo={};
      for(const key of ['si_signo','si_errno','si_code'])result.last_siginfo[key]=Number(integer(at,fields,key,true));
      result.last_siginfo.bytes_le_hex=Array.from({length:t.type},(_,i)=>Number(read(at+BigInt(i),1)).toString(16).padStart(2,'0')).join('');
    }
  }
  return result;
}
