// Supplemental read-only thread/stack evidence. The pinned observer and CPU remain untouched.
import assert from 'node:assert/strict';
import {readFileSync,statSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';
import {enumerateMapleVmas,mountedFilePath,userMemoryReader} from './android_live_property.mjs';
import {memoryElfSymbols} from './reverseandroid-elf-symbols.mjs';

const hx=x=>'0x'+x.toString(16), kernelPointer=x=>x>=0xffff000000000000n;
export function enumerateThreadPointers({leader,read,task,signal}) {
  const owner=read(leader+BigInt(task.signal.offset));assert.ok(kernelPointer(owner));
  const head=owner+BigInt(signal.thread_head.offset), out=[],seen=new Set();
  for(let link=read(head);link!==head;link=read(link)) {
    assert.ok(kernelPointer(link)&&!seen.has(link.toString())&&seen.size<2048,'Invalid thread-list cycle');
    seen.add(link.toString());
    const next=read(link),prev=read(link+8n);
    assert.equal(read(next+8n),link,'Thread forward backlink');
    assert.equal(read(prev),link,'Thread reverse backlink');
    const pointer=link-BigInt(task.thread_node.offset);
    assert.equal(read(pointer+BigInt(task.group_leader.offset)),leader,'Foreign thread leader');
    assert.equal(read(pointer+BigInt(task.signal.offset)),owner,'Foreign thread signal');
    out.push(pointer);
  }
  assert.ok(out.includes(leader),'Leader missing from its own thread list');
  return out;
}

function inspectThreads(api) {
  const {read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest}=api;
  const layout=name=>Object.fromEntries(Object.entries(members(type(name))).map(([k,v])=>{
    const t=resolveType(types[v.type]);return [k,{offset:v.offset,count:t.kind===3?kernel.readUInt32LE(t.at+20):null}];
  }));
  const context=members(types[members(types[task.thread.type]).cpu_context.type]);
  const contextOffset=task.thread.offset+members(types[task.thread.type]).cpu_context.offset;
  const pt=members(type('pt_regs')),ptSize=type('pt_regs').type,mmFields=members(type('mm_struct'));
  const unsigned=(p,key,n=4)=>read(p+BigInt(task[key].offset),n);
  const selected=tasks.filter(t=>t.zygote_startup||/bootanim|surfaceflinger|^(sh|su|printf|id|cat|toybox|renice)$/.test(t.name));
  const result={schema:'reverseandroid-thread-probe-v1',read_only:true,groups:[],kernel_layout_pinned:true};
  for(const leaderItem of selected) {
    const leader=BigInt(leaderItem.task),group={pid:leaderItem.pid,name:leaderItem.name,threads:[]};
    result.groups.push(group);
    try {
      const pointers=enumerateThreadPointers({leader,read,task,signal:members(type('signal_struct'))});
      const mm=unsigned(leader,'mm',8),pgd=read(mm+BigInt(mmFields.pgd.offset));
      const memory=userMemoryReader({pgdPhysical:translate(pgd),userAddressBits:64-Number(m(11)&63n),physicalByte:physical,physical64:phys64});
      const areas=enumerateMapleVmas({mm,read,layout});
      const paths=new Map(),symbolCache=new Map();
      const pathFor=pointer=>{const key=pointer.toString();if(!paths.has(key)){try{paths.set(key,mountedFilePath(pointer,{read,layout}));}catch(e){paths.set(key,null);}}return paths.get(key);};
      const mapping=address=>{
        const v=areas.find(x=>address>=x.start&&address<x.end);if(!v)return null;
        let file=null;
        if(v.file)file=pathFor(v.file);
        const result={address:hx(address),start:hx(v.start),end:hx(v.end),flags:hx(v.flags),file,
          file_offset:v.file?(v.filePageOffset*4096n+address-v.start).toString():null};
        if(file){
          if(!symbolCache.has(file))try{symbolCache.set(file,memoryElfSymbols({memory,areas,pathFor,file}));}catch(e){symbolCache.set(file,{error:e.message});}
          const symbols=symbolCache.get(file);
          if(symbols.error)result.dynamic_symbols_unavailable=symbols.error;
          else result.dynamic_symbol=symbols.lookup(address);
        }
        return result;
      };
      for(const p of pointers) {
        const current=p===currentTask, liveKernel=current&&m(4)===1n;
        const sp=liveKernel?u(3):read(p+BigInt(contextOffset+context.sp.offset));
        const fp=liveKernel?u(37):read(p+BigInt(contextOffset+context.fp.offset));
        const pc=liveKernel?u(2):read(p+BigInt(contextOffset+context.pc.offset));
        const item={pid:Number(unsigned(p,'pid')),tgid:Number(unsigned(p,'tgid')),name:text(p+BigInt(task.comm.offset),16),task:hx(p),
          state:Number(unsigned(p,'__state')),exit_state:Number(unsigned(p,'exit_state')),on_rq:Number(unsigned(p,'on_rq')),on_cpu:Number(unsigned(p,'on_cpu')),
          priority:Number(unsigned(p,'static_prio')),current,context_may_be_stale:current&&m(4)===0n,
          kernel_pc:nearest(pc),kernel_stack:[],accounting:{}};
        assert.equal(item.tgid,leaderItem.pid);
        for(const k of ['utime','stime','nvcsw','nivcsw'])if(task[k])item.accounting[k]=unsigned(p,k,8).toString();
        const se=members(types[task.se.type]);
        for(const k of ['sum_exec_runtime','vruntime','exec_start'])if(se[k])item.accounting[k]=read(p+BigInt(task.se.offset+se[k].offset)).toString();
        let kernelFrame=fp;const visited=new Set();
        for(let i=0;i<24&&kernelPointer(kernelFrame)&&!visited.has(kernelFrame.toString());i++) {
          try{visited.add(kernelFrame.toString());const prev=read(kernelFrame),lr=read(kernelFrame+8n);item.kernel_stack.push({fp:hx(kernelFrame),lr:hx(lr),symbol:nearest(lr)});kernelFrame=prev;}catch(e){item.kernel_unwind_stop=e.message;break;}
        }
        const base=unsigned(p,'stack',8),candidates=[];
        if(!current)for(const stackBytes of [16384,32768,8192,65536]) {
          if(sp<base||sp>=base+BigInt(stackBytes)||ptSize>stackBytes)continue;
          try {
            const at=base+BigInt(stackBytes-ptSize),pstate=read(at+BigInt(pt.pstate.offset));
            const userPc=read(at+BigInt(pt.pc.offset)),userSp=read(at+BigInt(pt.sp.offset));
            const pcMap=mapping(userPc),spMap=mapping(userSp);
            if((pstate&15n)!==0n||!pcMap||!(BigInt(pcMap.flags)&4n)||!spMap)continue;
            const regs=Array.from({length:31},(_,i)=>read(at+BigInt(pt.regs.offset+i*8)));
            const number=Number(read(at+BigInt(pt.syscallno.offset),4));
            const saved={stack_bytes:stackBytes,syscall:number,pc:pcMap,sp:hx(userSp),fp:hx(regs[29]),lr:mapping(regs[30]),args:regs.slice(0,6).map(hx),registers:regs.map(hx),pstate:hx(pstate),user_stack:[]};
            let userFrame=regs[29];const frames=new Set();
            for(let i=0;i<32&&userFrame>=userSp&&userFrame-userSp<1048576n&&(userFrame&7n)===0n&&!frames.has(userFrame.toString());i++) {
              try{frames.add(userFrame.toString());const b=memory.read(userFrame,16),prev=b.readBigUInt64LE(0),lr=b.readBigUInt64LE(8);saved.user_stack.push({fp:hx(userFrame),return_pc:mapping(lr)});if(prev<=userFrame)break;userFrame=prev;}catch(e){saved.unwind_stop=e.message;break;}
            }
            if(number===98)try{
              const raw=pt.orig_x0?read(at+BigInt(pt.orig_x0.offset)):regs[0];
              const address=(m(11)&(1n<<37n))?raw&0x00ffffffffffffffn:raw;
              saved.futex={raw_tagged_address:hx(raw),address:hx(address),TBI0_enabled:Boolean(m(11)&(1n<<37n)),operation:Number(regs[1]),expected:Number(regs[2]&0xffffffffn),observed:memory.read(address,4).readUInt32LE(0),mapping:mapping(address)};
            }catch(e){saved.futex_read_error=e.message;}
            try{saved.pc_code_le_hex=memory.read(userPc,64).toString('hex');}catch(e){saved.pc_code_read_error=e.message;}
            candidates.push(saved);
          } catch(e) {item.saved_frame_read_errors??=[];item.saved_frame_read_errors.push({stack_bytes:stackBytes,error:e.message});}
        }
        if(current&&m(4)===0n)item.live_user_pc=mapping(u(2));
        else if(candidates.length===1)item.saved_user_frame=candidates[0];
        else item.saved_user_frame_unverified={candidate_count:candidates.length};
        group.threads.push(item);
      }
      group.thread_list_verified=true;group.thread_count=group.threads.length;group.user_vma_count=areas.length;
    } catch(e) {group.error=e.stack;}
  }
  return result;
}

export async function probeThreads(path,sha) {
  const originalUrl=new URL('./android_kernel_observer.mjs',import.meta.url),source=readFileSync(originalUrl,'utf8');
  const manifest=JSON.parse(readFileSync(new URL('../state/upstream-snapshot.json',import.meta.url)));
  const pin=manifest.files.find(x=>x.path==='tools/android_kernel_observer.mjs');assert.ok(pin);
  const blob=createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex');assert.equal(blob,pin.git_blob_sha1);
  const marker=" check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');";
  assert.equal(source.split(marker).length,2,'Unique read-only observer completion marker required');
  const hook=' options?.extraReadOnlyProbe?.({read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest});\n';
  const patched=source.replace(marker,hook+marker).replace(/from (['"])(\.\/[^'"]+)\1/g,(_,quote,spec)=>'from '+quote+new URL(spec,originalUrl).href+quote);
  const {observeAndroidKernel}=await import('data:text/javascript;base64,'+Buffer.from(patched).toString('base64'));
  assert.ok(statSync(path).size<=2147483648);
  let threads;const bytes=readFileSync(path);
  const proof=observeAndroidKernel(bytes,sha,{maxSnapshotBytes:2147483648,liveProperties:true,zygoteProbe:true,schedulerProbe:true,extraReadOnlyProbe:api=>{threads=inspectThreads(api);}});
  assert.ok(threads);assert.equal(proof.input_unchanged,true);
  return {...threads,checkpoint_sha256:proof.checkpoint_sha256,instructions:proof.instructions,input_unchanged:true,
    observer_source_git_blob_sha1:blob,original_observer_file_unchanged:true,original_CPU_and_codecs_unchanged:true};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href) {
  const [path,sha,output]=process.argv.slice(2);assert.ok(path&&sha&&output);
  const result=await probeThreads(path,sha);writeFileSync(output,JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify({instructions:result.instructions,groups:result.groups.map(x=>({pid:x.pid,name:x.name,thread_count:x.thread_count,error:x.error})),input_unchanged:true}));
}
