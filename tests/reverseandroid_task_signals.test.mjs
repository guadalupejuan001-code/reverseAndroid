import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectTaskSignals} from '../tools/reverseandroid-task-signals.mjs';

function fixture(){
  const bytes=new Map(),types=[null],kernel=Buffer.alloc(64);
  const add=t=>(types.push(t),types.length-1);
  const u32=add({kind:1,type:4}),u64=add({kind:1,type:8});
  const struct=(type,fields)=>add({kind:4,type,fields});
  const pointer=type=>add({kind:2,type});
  const array=add({kind:3,at:0});kernel.writeUInt32LE(u64,12);kernel.writeUInt32LE(1,20);
  const sigset=struct(8,{sig:{offset:0,type:array}});
  const pending=struct(8,{signal:{offset:0,type:sigset}});
  const signal=struct(32,{flags:{offset:0,type:u32},group_exit_code:{offset:4,type:u32},
    group_stop_count:{offset:8,type:u32},shared_pending:{offset:16,type:pending}});
  const info=struct(128,{si_signo:{offset:0,type:u32},si_errno:{offset:4,type:u32},si_code:{offset:8,type:u32}});
  const task={pid:{offset:0,type:u32},tgid:{offset:4,type:u32},comm:{offset:8,type:u64},
    ptrace:{offset:24,type:u32},jobctl:{offset:32,type:u64},exit_code:{offset:40,type:u32},exit_signal:{offset:44,type:u32},
    parent:{offset:48,type:pointer(0)},real_parent:{offset:56,type:pointer(0)},
    blocked:{offset:64,type:sigset},pending:{offset:72,type:pending},
    signal:{offset:80,type:pointer(signal)},last_siginfo:{offset:88,type:pointer(info)}};
  const leader=0xffff000000010000n,tracer=leader+4096n,owner=leader+8192n,siginfo=leader+12288n;
  const write=(at,value,n=8)=>{for(let i=0;i<n;i++)bytes.set((at+BigInt(i)).toString(),Number(value>>BigInt(i*8)&255n));};
  for(const at of [leader,tracer,owner,siginfo])for(let i=0;i<128;i++)write(at+BigInt(i),0n,1);
  write(leader,3386n,4);write(leader+4n,3386n,4);write(tracer,4111n,4);write(tracer+4n,4111n,4);
  write(leader+24n,1n,4);write(leader+32n,0x8000000000000000n);write(leader+44n,17n,4);
  write(leader+48n,tracer);write(leader+56n,leader);write(leader+64n,0x8000000000000001n);
  write(leader+72n,0x100n);write(leader+80n,owner);write(leader+88n,siginfo);
  write(owner+16n,0x200n);write(siginfo,35n,4);write(siginfo+8n,0xfffffffan,4);
  const api={kernel,types,task,resolveType:t=>t,members:t=>t.fields,
    text:at=>at===tracer+8n?'crash_dump64':'system_server',
    read:(at,n=8)=>{let value=0n;for(let i=0;i<n;i++){const b=bytes.get((at+BigInt(i)).toString());assert.notEqual(b,undefined,'Unmapped fixture address');value|=BigInt(b)<<BigInt(i*8);}return value;}};
  return {api,leader,tracer,siginfo,write,bytes};
}
test('BTF widths preserve high bits and identify actual tracer and signal masks',()=>{
  const f=fixture(),r=inspectTaskSignals(f.api,f.leader);
  assert.equal(r.parent.pid,4111);assert.equal(r.parent.name,'crash_dump64');assert.equal(r.real_parent.pid,3386);
  assert.equal(r.jobctl_raw,'9223372036854775808');
  assert.equal(r.exit_signal_raw,'17');
  assert.deepEqual(r.blocked_words,['0x8000000000000001']);assert.deepEqual(r.pending_words,['0x100']);assert.deepEqual(r.shared_pending_words,['0x200']);
  assert.equal(r.last_siginfo.si_signo,35);assert.equal(r.last_siginfo.si_code,-6);assert.equal(r.last_siginfo.bytes_le_hex.length,256);
  assert.equal(f.api.read(f.leader+48n),f.tracer,'Observation did not write memory');
});
test('null siginfo is reported without dereference',()=>{
  const f=fixture();f.write(f.leader+88n,0n);const r=inspectTaskSignals(f.api,f.leader);
  assert.equal(r.last_siginfo_pointer,'0x0');assert.equal(r.last_siginfo,undefined);
});
test('invalid task, parent and siginfo pointers fail instead of inventing identity',()=>{
  const f=fixture();assert.throws(()=>inspectTaskSignals(f.api,1n),/Invalid task/);
  f.write(f.leader+48n,4n);assert.throws(()=>inspectTaskSignals(f.api,f.leader),/related task/);
  f.write(f.leader+48n,f.tracer);f.write(f.leader+88n,8n);assert.throws(()=>inspectTaskSignals(f.api,f.leader),/siginfo pointer/);
});
test('incomplete memory and non-byte BTF offsets are rejected',()=>{
  const f=fixture();f.bytes.delete((f.siginfo+127n).toString());assert.throws(()=>inspectTaskSignals(f.api,f.leader),/Unmapped/);
  const g=fixture();g.api.task.ptrace.offset=24.5;assert.throws(()=>inspectTaskSignals(g.api,g.leader),/bitfield/);
});
