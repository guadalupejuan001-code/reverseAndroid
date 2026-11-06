// Supplemental read-only inspection of an existing process mmap semaphore.
// The imported observer, machine, original snapshot and kernel remain unchanged.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {enumerateThreadPointers} from './reverseandroid-thread-probe.mjs';
import {inspectTaskScheduling} from './reverseandroid-scheduler-group.mjs';

const [path,sha,pidText,output]=process.argv.slice(2),pid=Number(pidText);
assert.ok(path&&output&&Number.isSafeInteger(pid)&&pid>0);assert.match(sha,/^[0-9a-f]{64}$/);
try {
  const original=new URL('./android_kernel_observer.mjs',import.meta.url),source=readFileSync(original,'utf8');
  const manifest=JSON.parse(readFileSync(new URL('../state/upstream-snapshot.json',import.meta.url)));
  const pin=manifest.files.find(x=>x.path==='tools/android_kernel_observer.mjs');assert.ok(pin);
  const blob=createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex');assert.equal(blob,pin.git_blob_sha1);
  const marker=" check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');";
  assert.equal(source.split(marker).length,2);
  const hooked=source.replace(marker,' options?.extraReadOnlyProbe?.({read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest});\n'+marker)
    .replace(/from (['"])(\.\/[^'"]+)\1/g,(_,quote,spec)=>'from '+quote+new URL(spec,original).href+quote);
  const {observeAndroidKernel}=await import('data:text/javascript;base64,'+Buffer.from(hooked).toString('base64'));
  let result;
  const proof=observeAndroidKernel(readFileSync(path),sha,{maxSnapshotBytes:2147483648,liveProperties:true,zygoteProbe:true,extraReadOnlyProbe:api=>{
    const {read,text,task,tasks,type,members,types}=api,leader=tasks.find(t=>t.pid===pid);assert.ok(leader&&leader.exit_state===0);
    const ptr=BigInt(leader.task),mm=read(ptr+BigInt(task.mm.offset));assert.ok(mm>=0xffff000000000000n);
    const mmFields=members(type('mm_struct')),sem=members(type('rw_semaphore')),at=mm+BigInt(mmFields.mmap_lock.offset);
    const atomic=field=>{const fields=members(types[sem[field].type]);return read(at+BigInt(sem[field].offset+fields.counter.offset));};
    const owner=atomic('owner'),count=atomic('count'),all=new Map(),groups=[];
    const describe=p=>{
      const t={task:'0x'+p.toString(16),pid:Number(read(p+BigInt(task.pid.offset),4)),tgid:Number(read(p+BigInt(task.tgid.offset),4)),name:text(p+BigInt(task.comm.offset),16),state:Number(read(p+BigInt(task.__state.offset),4)),exit_state:Number(read(p+BigInt(task.exit_state.offset),4)),static_priority:Number(read(p+BigInt(task.static_prio.offset),4)),on_rq:Number(read(p+BigInt(task.on_rq.offset),4)),on_cpu:Number(read(p+BigInt(task.on_cpu.offset),4))};
      t.nice=t.static_priority-120;
      try{t.scheduling=inspectTaskScheduling(api,p);}catch(e){t.scheduling_unverified=e.message;}
      return t;
    };
    for(const l of tasks.filter(t=>t.pid>0&&t.exit_state===0)){
      try{
        const ps=enumerateThreadPointers({leader:BigInt(l.task),read,task,signal:members(type('signal_struct'))});
        for(const p of ps)all.set(p.toString(),describe(p));
      }catch(e){groups.push({pid:l.pid,error:e.message});}
    }
    const candidate=all.get((owner&~7n).toString())??null;
    result={schema:'reverseandroid-readonly-mmap-lock-v1',read_only:true,pid,leader_task:leader.task,mm:'0x'+mm.toString(16),mmap_lock:'0x'+at.toString(16),raw_count:'0x'+count.toString(16),raw_owner:'0x'+owner.toString(16),owner_low_flags:Number(owner&7n),owner_record_task:candidate,owner_is_not_assumed_to_be_the_only_reader:true,all_thread_list_errors:groups,runnable_threads:[...all.values()].filter(t=>t.state===0&&t.exit_state===0),btf_layout_verified:true,thread_owner_membership_verified:candidate!==null};
  }});
  assert.ok(result);assert.equal(proof.input_unchanged,true);
  result={...result,checkpoint_sha256:sha,instructions:proof.instructions,input_unchanged:true,observer_source_git_blob_sha1:blob,original_observer_file_unchanged:true};
  writeFileSync(output,JSON.stringify(result,null,2)+'\n',{flag:'wx'});
  console.log(JSON.stringify({instructions:result.instructions,pid,mmap_lock:result.mmap_lock,raw_count:result.raw_count,owner_record_task:result.owner_record_task,runnable_thread_count:result.runnable_threads.length,input_unchanged:true}));
}catch(error){console.error(error.message);process.exitCode=1;}
