#!/usr/bin/env node
// Read-only diagnostic for the byte-pinned Android GKI below. Never restores,
// executes, mutates or substitutes the XI/Ether authority.
import {readFileSync,statSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {rvgMemoryLines} from './rvg_memory_lines.mjs';
import {observePid1BootProperty,mountedFilePath,enumerateMapleVmas,userMemoryReader} from './android_live_property.mjs';
import {inspectBinderQueues} from './android_binder_snapshot_probe.mjs';
const KERNEL='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8';
const VBASE=0xffffffc080000000n, RAM=0x40000000n;
const MASK=0xffffffffffffffffn, PA=0x0000fffffffff000n;
const check=(ok,msg)=>{if(!ok)throw Error('Android observer: '+msg);};
const hex=n=>'0x'+n.toString(16);
const extras={1:4,2:0,3:12,4:12,5:12,6:8,7:0,8:0,9:0,10:0,11:0,12:0,13:8,14:4,15:12,16:0,17:4,18:0,19:12};
export function observeAndroidKernel(bytes,expectedCheckpointSha,options){
 const originalSha=createHash('sha256').update(bytes).digest('hex');
 const provenance=auditGkiRvg(bytes,KERNEL,expectedCheckpointSha,options);
 check(['RVI2','RVI3'].includes(provenance.rvi_version)&&provenance.bootargs.split(' ').includes('nokaslr'),'observer profile requires the pinned nonrelocated Android boot');
 const u=i=>bytes.readBigUInt64LE(8+i*8), fields=provenance.rvg_version==='RVG3'?44:43;
 const imageAt=8+fields*8, imageBytes=Number(u(39)),coreBytes=Number(u(40)),memoryBytes=Number(u(41));
 const imageHeader=provenance.rvi_version==='RVI3'?48:40;
 const kernel=bytes.subarray(imageAt+imageHeader,imageAt+imageHeader+provenance.kernel_bytes);
 const rdAt=imageAt+imageHeader+provenance.kernel_bytes+provenance.dtb_bytes;
 const rd=bytes.subarray(rdAt,rdAt+provenance.initrd_bytes);
 const dtb=bytes.subarray(rdAt-provenance.dtb_bytes,rdAt);
 const kernelAddress=RAM+kernel.readBigUInt64LE(8);
 const machineAt=imageAt+imageBytes+coreBytes+memoryBytes;
 const m=i=>bytes.readBigUInt64LE(machineAt+i*8);
 const sctlr=m(10),tcr=m(11),ttbr0=m(12),ttbr1=m(13);
 check(((tcr>>14n)&3n)===0n&&((tcr>>30n)&3n)===2n,'observer supports only the native 4-KiB translation granules');
 const pageMap=new Map();
 const lines=rvgMemoryLines(bytes.subarray(imageAt+imageBytes+coreBytes,machineAt));
 check(/^MXIM2 \d+$/.test(lines.next().value)&&lines.next().value==='MXIM3 0','requires original adaptive zero-background snapshot');
 for(const line of lines){
  if(!line)continue;
  const [kind,address,bitmap,values]=line.split(' ');
  check(kind==='P','unsupported large-address or quantum-bound record');
  const page=Number(address),mask=Buffer.from(bitmap,'hex'),data=Buffer.from(values,'hex');
  check(Number.isSafeInteger(page)&&mask.length===512&&!pageMap.has(page),'invalid or duplicate page');
  const dense=Buffer.alloc(4096);let j=0;
  for(let i=0;i<4096;i++)if(mask[i>>3]&(1<<(i&7)))dense[i]=data[j++];
  check(j===data.length,'page occupancy extent mismatch');
  pageMap.set(page,dense);
 }
 const physical=addr=>{
  check(addr>=RAM&&addr<RAM+u(0),'physical address outside guest RAM');
  const p=Number(addr),delta=pageMap.get(Math.floor(p/4096))?.[p%4096]??0;
  let backing=0;
  if(addr>=kernelAddress&&addr<kernelAddress+BigInt(kernel.length))backing=kernel[Number(addr-kernelAddress)];
  else if(addr>=RAM+u(0)-2097152n&&addr<RAM+u(0)-2097152n+BigInt(dtb.length))backing=dtb[Number(addr-(RAM+u(0)-2097152n))];
  else if(addr>=RAM+u(0)/2n&&addr<RAM+u(0)/2n+BigInt(rd.length))backing=rd[Number(addr-(RAM+u(0)/2n))];
  return backing^delta;
 };
 const phys64=a=>{let n=0n;for(let i=0;i<8;i++)n|=BigInt(physical(a+BigInt(i)))<<BigInt(8*i);return n;};
 const translate=address=>{
  if(!(sctlr&1n))return address;
  const upper=Boolean((address>>55n)&1n),size=64-Number((tcr>>BigInt(upper?16:0))&63n);
  check(size>=25&&size<=48,'unsupported virtual-address width');
  let canonical=address;
  if(tcr&(1n<<BigInt(upper?38:37)))canonical=upper?address|0xff00000000000000n:address&0x00ffffffffffffffn;
  const vaMask=(1n<<BigInt(size))-1n;
  check(upper?((canonical|vaMask)&MASK)===MASK:(canonical&~vaMask)===0n,'noncanonical virtual address');
  const start=4-Math.ceil((size-12)/9);
  check(!(tcr&(1n<<BigInt(upper?23:7))),'disabled translation region');
  let table=(upper?ttbr1:ttbr0)&PA;
  for(let level=start;level<4;level++){
   const shift=BigInt(12+(3-level)*9),d=phys64(table+((canonical>>shift)&511n)*8n);
   check(Boolean(d&1n),'unmapped virtual address');
   if(level<3&&(d&2n)){table=d&PA;continue;}
   const mask=(1n<<shift)-1n;
   check(level>0&&(level<3||Boolean(d&2n))&&!(d&(mask&~4095n))&&Boolean(d&(1n<<10n)),'invalid translation descriptor');
   return (d&PA&~mask)|(canonical&mask);
  }
  throw Error('Android observer: translation failed');
 };
 const read=(a,n=8)=>{let v=0n;for(let i=0;i<n;i++)v|=BigInt(physical(translate(a+BigInt(i))))<<BigInt(i*8);return v;};
 const text=(a,max)=>{let s='';for(let i=0;i<max;i++){const v=Number(read(a+BigInt(i),1));if(!v)break;s+=String.fromCharCode(v);}return s;};
 // These offsets are accepted ONLY after the entire immutable kernel hash
 // matches. Validate all names, marker extents and sorted symbol offsets.
 const profile={countAt:22268536,namesAt:22268544,namesEnd:24361064,markersAt:24361064,tokenTable:24363112,tokenIndex:24364024,offsetsAt:24364536,btfAt:27670332};
 const count=kernel.readUInt32LE(profile.countAt);
 check(count===130889,'symbol count mismatch');
 const tokens=Array.from({length:256},(_,i)=>{const p=profile.tokenTable+kernel.readUInt16LE(profile.tokenIndex+2*i);const end=kernel.indexOf(0,p);check(end>=p&&end<profile.tokenIndex,'token extent');return kernel.subarray(p,end).toString('ascii');});
 const symbols=[];let at=profile.namesAt,last=0;
 for(let i=0;i<count;i++){
  if(i%256===0)check(kernel.readUInt32LE(profile.markersAt+4*(i/256))===at-profile.namesAt,'symbol marker mismatch');
  let n=kernel[at++];if(n&128)n=(n&127)|(kernel[at++]<<7);
  check(n>0&&at+n<=profile.namesEnd,'symbol name extent');
  const full=[...kernel.subarray(at,at+n)].map(x=>tokens[x]).join('');at+=n;
  const offset=kernel.readUInt32LE(profile.offsetsAt+4*i);
  check(offset>=last,'symbol offsets not sorted');last=offset;
  symbols.push({name:full.slice(1),address:VBASE+BigInt(offset),index:i});
 }
 check(at===profile.namesEnd&&symbols[0].name==='_text','symbol layout mismatch');
 const symbol=name=>{const matches=symbols.filter(s=>s.name===name);check(matches.length===1,'ambiguous symbol '+name);return matches[0].address;};
 const nearest=pc=>{let lo=0,hi=symbols.length;while(lo<hi){let mid=(lo+hi)>>1;if(symbols[mid].address<=pc)lo=mid+1;else hi=mid;}if(!lo)return null;const s=symbols[lo-1];return s.name+'+'+hex(pc-s.address);};
 const btf=profile.btfAt;
 check(kernel.readUInt16LE(btf)===0xeb9f&&kernel.readUInt32LE(btf+4)===24,'BTF header');
 const typeStart=btf+24+kernel.readUInt32LE(btf+8),typeEnd=typeStart+kernel.readUInt32LE(btf+12);
 const strStart=btf+24+kernel.readUInt32LE(btf+16),strEnd=strStart+kernel.readUInt32LE(btf+20);
 const str=off=>{const p=strStart+off,end=kernel.indexOf(0,p);check(p<strEnd&&end>=p&&end<strEnd,'BTF string extent');return kernel.subarray(p,end).toString();};
 const types=[null];
 for(let p=typeStart;p<typeEnd;){
  const info=kernel.readUInt32LE(p+4),kind=(info>>>24)&31,vlen=info&65535;
  check(kind in extras,'unknown BTF kind');
  const length=12+((kind===4||kind===5||kind===6||kind===13||kind===15||kind===19)?extras[kind]*vlen:extras[kind]);
  check(p+length<=typeEnd,'BTF type extent');
  types.push({name:str(kernel.readUInt32LE(p)),kind,vlen,info,type:kernel.readUInt32LE(p+8),at:p});
  p+=length;
 }
 const typeReference=name=>{
  const list=types.filter(t=>t?.kind===4&&t.name===name);
  const layout=t=>JSON.stringify([t.type,Array.from({length:t.vlen},(_,i)=>{
   const p=t.at+12+i*12;return [str(kernel.readUInt32LE(p)),kernel.readUInt32LE(p+8)];
  })]);
  check(list.length>0&&list.every(t=>layout(t)===layout(list[0])),'inconsistent BTF struct '+name);
  return list[0];
 };
 // Cache only validated immutable BTF metadata within this observation.
 const typeCache=new Map();
 const type=name=>{
  const cached=process.env.REVERSE_PC_FAST_OBSERVER==='1';
  if(cached&&typeCache.has(name))return typeCache.get(name);
  const value=typeReference(name);if(cached)typeCache.set(name,value);return value;
 };
 const resolveType=t=>{let n=0;while([8,9,10,11,18].includes(t.kind)){check(n++<16,'BTF alias cycle');t=types[t.type];}return t;};
 const members=(input,base=0,depth=0)=>{
  const t=resolveType(input);check(depth<8&&[4,5].includes(t.kind),'unsupported BTF member container');
  const out={};for(let i=0;i<t.vlen;i++){
   const p=t.at+12+12*i,bits=kernel.readUInt32LE(p+8),bitOffset=t.info>>>31?bits&0xffffff:bits;
   const name=str(kernel.readUInt32LE(p)),id=kernel.readUInt32LE(p+4),offset=base+bitOffset/8;
   if(name)out[name]={offset,type:id};
   else Object.assign(out,members(types[id],offset,depth+1));
  }return out;
 };
 const binderLayoutDebug=options?.binderDebug?(()=>{
    // Read-only BTF metadata from the exact pinned kernel: no task, RAM or Binder state writes.
    const targets=['binder_proc','binder_thread','binder_transaction','binder_work','hlist_head','hlist_node','rb_root','rb_node','list_head'];
    const layouts={};
    for(const name of targets){
      try{
        const t=type(name),fields=members(t);
        layouts[name]={size:t.type,fields:Object.fromEntries(Object.entries(fields).map(([field,member])=>[field,member.offset]))};
      }catch(e){layouts[name]={unavailable:e.message};}
    }
    return {schema:'reverse-android-binder-layout-v1',
      candidate_symbols:symbols.filter(x=>['binder_procs','binder_dead_nodes','binder_deferred_list','binder_context_mgr_node'].includes(x.name)).map(x=>({name:x.name,address:hex(x.address)})),
      layouts,read_only:true};
  })():undefined;
 const task=members(type('task_struct')),thread=members(types[task.thread.type]),context=members(types[thread.cpu_context.type]);
 const layout=Object.fromEntries(['tasks','pid','comm','__state','stack'].map(n=>[n,task[n].offset]));
 const contextOffset=task.thread.offset+thread.cpu_context.offset;
 const init=symbol('init_task'),head=init+BigInt(layout.tasks),tasks=[];let node=head;
 // EL0 uses SP_EL0 as the user stack. Linux entry.S retrieves the task
 // from the per-CPU __entry_task instead; validate it against the task list.
 const inKernel=m(4)===1n;let currentTask=null,currentTaskError=null;const currentLookup={};
 const currentTaskSource=inKernel?'sp_el0':'__entry_task+TPIDR_EL1';
 try{currentLookup.entry_task_symbol=hex(symbol('__entry_task'));currentLookup.tpidr_el1=hex(m(25));currentLookup.per_cpu_offset=hex(read(symbol('__per_cpu_offset')));currentTask=inKernel?m(8):read((symbol('__entry_task')+m(25))&MASK);currentLookup.candidate=hex(currentTask);}
 catch(error){currentTaskError=error.message;}
 const ptregs=members(type('pt_regs'));
 const seen=new Set();
 do{
  check(!seen.has(node.toString())&&tasks.length<512,'task-list cycle or limit');seen.add(node.toString());
  const pointer=node-BigInt(layout.tasks),pid=Number(read(pointer+BigInt(layout.pid),4));
  const current=pointer===currentTask,liveKernel=current&&inKernel;
  const fp=liveKernel?u(37):read(pointer+BigInt(contextOffset+context.fp.offset)),sp=liveKernel?u(3):read(pointer+BigInt(contextOffset+context.sp.offset)),pc=liveKernel?u(2):read(pointer+BigInt(contextOffset+context.pc.offset));
  const item={pid,name:text(pointer+BigInt(layout.comm),16),task:hex(pointer),state:Number(read(pointer+BigInt(layout.__state),4)),running_on_cpu:current,context_pc:hex(pc),context_sp:hex(sp),context_kind:liveKernel?'live_kernel_registers':'saved_kernel_context',saved_context_may_be_stale:current&&!inKernel,live_user_pc:current&&!inKernel?hex(u(2)):null,stack:[]};
  if(options?.liveProperties){
   check(resolveType(types[task.exit_state.type]).type===4&&resolveType(types[task.mm.type]).kind===2,'task lifecycle BTF layout');
   item.exit_state=Number(read(pointer+BigInt(task.exit_state.offset),4));
   const mm=read(pointer+BigInt(task.mm.offset));check(mm===0n||mm>=0xffff000000000000n,'invalid task mm');
   item.has_user_mm=mm!==0n;
  }
  // Read the saved userspace syscall frame of sleeping processes.
  // A kernel call frame named ioctl alone does not establish Binder IPC.
  if(options?.binderDebug&&item.has_user_mm&&!current&&/^(odsign|keystore2|android\.hardwar|servicemanager|hwservicemanage)$/.test(item.name)){
   try{
    check(task.stack&&ptregs.regs&&ptregs.syscallno&&ptregs.pc,'ARM64 syscall BTF layout incomplete');
    const base=read(pointer+BigInt(task.stack.offset));
    check(base>=0xffff000000000000n,'Invalid task stack base');
    const size=type('pt_regs').type;
    const layouts=[16384,32768,8192,65536],candidates=[];
    for(const threadSize of layouts){
     if(sp<base||sp>=base+BigInt(threadSize)||size>threadSize)continue;
     try{
      const at=base+BigInt(threadSize-size),syscall=Number(read(at+BigInt(ptregs.syscallno.offset),4));
      const userPc=read(at+BigInt(ptregs.pc.offset));
      const arg=i=>read(at+BigInt(ptregs.regs.offset+i*8));
      candidates.push({thread_stack_bytes:threadSize,number:syscall,user_pc:hex(userPc),fd:Number(arg(0)),request:hex(arg(1)),third_argument:hex(arg(2))});
     }catch(e){candidates.push({thread_stack_bytes:threadSize,error:e.message});}
    }
    const ioctl=candidates.find(x=>x.number===29&&x.fd>=0&&x.fd<65536);
    item.guest_syscall=ioctl??{unverified:true,candidates};
    if(ioctl&&task.files){
     try{
      const files=read(pointer+BigInt(task.files.offset)),filesLayout=members(type('files_struct')),
       fdtableLayout=members(type('fdtable'));
      const table=read(files+BigInt(filesLayout.fdt.offset)),max=Number(read(table+BigInt(fdtableLayout.max_fds.offset),4));
      check(ioctl.fd<max&&max<=1048576,'IOCTL fd out of bounds');
      const entries=read(table+BigInt(fdtableLayout.fd.offset));
      const file=read(entries+BigInt(8*ioctl.fd));
      check(file>=0xffff000000000000n,'IOCTL fd missing file');
      item.guest_syscall.file=mountedFilePath(file,{read,layout:name=>Object.fromEntries(Object.entries(members(type(name))).map(([k,v])=>[k,{offset:v.offset}]))});
      item.guest_syscall.file_pointer=hex(file);
      if(item.guest_syscall.file==='/dev/binderfs/binder'){
       // Binder's binder_open attaches the binder_proc to the kernel struct file.
       // Unlike binder_procs global enumeration, this is a per-fd direct pointer
       // with independently checked file identity and BTF field offsets.
       const fl=members(type('file')),bp=members(type('binder_proc'));
       check(fl.private_data&&bp.pid&&bp.proc_node&&bp.threads&&bp.todo,'Binder private_data BTF fields unavailable');
       const priv=read(file+BigInt(fl.private_data.offset));
       check(priv>=0xffff000000000000n&&(priv&7n)===0n,'Binder file private_data invalid');
       const binderPid=Number(read(priv+BigInt(bp.pid.offset),4));
       item.guest_syscall.binder_private={
         pointer:hex(priv),pid:binderPid,fd_owner_pid:pid,
         plausible_binder_proc_pid:binderPid>0&&binderPid<=4194304,
         file_private_data_field_offset:fl.private_data.offset,
         proc_node_next:hex(read(priv+BigInt(bp.proc_node.offset))),
         process_threads_root:hex(read(priv+BigInt(bp.threads.offset))),
         todo_head_next:hex(read(priv+BigInt(bp.todo.offset))),
         outstanding_txns:Number(read(priv+BigInt(bp.outstanding_txns.offset),4)),
         is_dead:Number(read(priv+BigInt(bp.is_dead.offset),1))!==0,
         context_pointer:bp.context?hex(read(priv+BigInt(bp.context.offset))):null,
         source:'verified_binder_fd_private_data',
         read_only:true
       };
      }
     }catch(e){item.guest_syscall.fd_path_error=e.message;}
    }
   }catch(e){item.guest_syscall={unverified:true,error:e.message};}
  }
  // C40: actual app_process64 / Zygote argv, backed by the CURRENT guest
  // mm_struct, process page table and pinned GKI BTF. Never infer argv from
  // a process name or reuse the stale startup console as live process data.
  if(options?.zygoteProbe&&item.has_user_mm&&item.exit_state===0&&
      /^(app_process64|zygote64|zygote|system_server|surfaceflinger|main)$/.test(item.name)){
   item.zygote_startup={read_only:true,pid,comm:item.name,argv_verified:false};
   try{
    const mm=read(pointer+BigInt(task.mm.offset)),mml=members(type('mm_struct'));
    check(mml.arg_start&&mml.arg_end&&mml.pgd&&mml.exe_file&&task.real_parent,
      'C40 requires verified GKI mm argv, pgd, exe and real_parent fields');
    const parent=read(pointer+BigInt(task.real_parent.offset));
    const parentPid=Number(read(parent+BigInt(task.pid.offset),4));
    const file=read(mm+BigInt(mml.exe_file.offset));
    check(file>=0xffff000000000000n,'C40 executable pointer invalid');
    const layoutOf=name=>Object.fromEntries(Object.entries(members(type(name))).map(([key,member])=>[key,{offset:member.offset}]));
    const exe=mountedFilePath(file,{read,layout:layoutOf});
    const start=read(mm+BigInt(mml.arg_start.offset)),end=read(mm+BigInt(mml.arg_end.offset));
    check(start>0n&&end>start&&end-start<=4096n,'C40 command line extent invalid');
    const pgd=read(mm+BigInt(mml.pgd.offset));
    check(pgd>=0xffff000000000000n,'C40 user page directory pointer invalid');
    const memory=userMemoryReader({pgdPhysical:translate(pgd),
      userAddressBits:64-Number(tcr&63n),physicalByte:physical,physical64:phys64});
    const raw=memory.read(start,Number(end-start));
    check(raw[raw.length-1]===0,'C40 argv is not NUL terminated');
    const args=raw.toString('utf8').split('\0').filter(Boolean);
    check(args.length>0&&args.length<=64&&args.every(x=>x.length>0&&x.length<=512&&
      /^[\x20-\x7e]+$/.test(x)),'C40 command line failed lexical validation');
    item.zygote_startup={read_only:true,pid,comm:item.name,exe_path:exe,
      parent_pid:parentPid,argv:args,argv_verified:true,
      launch_signature_verified:exe==='/system/bin/app_process64'&&
        args[0]==='/system/bin/app_process64'&&
        args.includes('-Xzygote')&&args.includes('--zygote')&&
        args.includes('--start-system-server')&&args.includes('--socket-name=zygote')&&
        parentPid===1};
   }catch(error){item.zygote_startup.read_error=error.message;}
  }
  for(const name of ['on_rq','on_cpu'])if(task[name])item[name]=Number(read(pointer+BigInt(task[name].offset),4));
  item.accounting={};
  for(const name of ['nvcsw','nivcsw','utime','stime'])if(task[name])item.accounting[name]=read(pointer+BigInt(task[name].offset)).toString();
  const se=members(types[task.se.type]);
  for(const name of ['sum_exec_runtime','vruntime','exec_start'])if(se[name])item.accounting[name]=read(pointer+BigInt(task.se.offset+se[name].offset)).toString();
  let frame=fp;const frames=new Set();for(let i=0;i<40&&frame;i++){
   if(frames.has(frame.toString())){item.unwind_stop='repeated_frame';break;}frames.add(frame.toString());
   try{
    const prev=read(frame),lr=read(frame+8n),entry={fp:hex(frame),lr:hex(lr),symbol:nearest(lr)};
    if(entry.symbol?.startsWith('el1h_64_irq+')){
     const regs=prev-BigInt(ptregs.stackframe.offset),pc=read(regs+BigInt(ptregs.pc.offset));
     // The entry stub links to pt_regs.stackframe, whose artificial LR is
     // ELR (not saved X30). Validate both links before interpreting registers.
     if(pc===read(prev+8n)&&read(regs+29n*8n)===read(prev)){
      entry.interrupted_pc=hex(pc);entry.interrupted_symbol=nearest(pc);
      entry.interrupted_registers=Array.from({length:31},(_,i)=>hex(read(regs+BigInt(i*8))));
      if(pid===1&&(entry.interrupted_symbol?.startsWith('btf_find_by_name_kind+')||entry.interrupted_symbol?.startsWith('strcmp+'))){
       try{entry.btf_query=text(BigInt(entry.interrupted_registers[20]),128);}catch(error){entry.btf_query_read_error=error.message;}
      }
     }else entry.irq_frame_unverified=true;
    }
    item.stack.push(entry);if(!lr)break;frame=prev;
   }catch(e){item.unwind_stop=e.message;break;}
  }
  tasks.push(item);const next=read(node);
  check(read(next+8n)===node,'task-list back link mismatch');node=next;
 }while(node!==head);
 let currentTaskVerification=currentTask!==null&&tasks.some(t=>t.task===hex(currentTask))?'process_list':null;
 // init_task.tasks enumerates process leaders. A running pthread must also
 // be validated in its leader's signal.thread_head, never guessed from PID.
 if(currentTask!==null&&!currentTaskVerification){
  try{
   const leader=read(currentTask+BigInt(task.group_leader.offset)),leaderItem=tasks.find(t=>t.task===hex(leader));check(leaderItem,'current thread has no verified process leader');
   const signalPointer=read(leader+BigInt(task.signal.offset));check(signalPointer===read(currentTask+BigInt(task.signal.offset)),'current thread signal differs from leader');
   check(Number(read(currentTask+BigInt(task.tgid.offset),4))===leaderItem.pid,'current thread group ID differs');
   const signal=members(type('signal_struct')),head=signalPointer+BigInt(signal.thread_head.offset),seenThreads=new Set();let link=read(head),found=false;
   while(link!==head){check(!seenThreads.has(link.toString())&&seenThreads.size<1024,'thread-list cycle or limit');seenThreads.add(link.toString());const next=read(link);check(read(next+8n)===link&&read(read(link+8n))===link,'thread-list back link mismatch');if(link-BigInt(task.thread_node.offset)===currentTask)found=true;link=next;}
   check(found,'current pointer absent from verified thread list');currentTaskVerification='process_thread_list';
  }catch(error){currentTaskError=error.message;currentTask=null;}
 }
 const currentDetails=currentTask===null?null:{pid:Number(read(currentTask+BigInt(layout.pid),4)),name:text(currentTask+BigInt(layout.comm),16),task:hex(currentTask),verification:currentTaskVerification,live_exception_level:Number(m(4)),live_pc:hex(u(2))};

 // Read-only scheduling state of the real pinned GKI. No guest writes.
 const schedPeek=(pointer)=>{
  const output={task:hex(pointer),read_only:true};
  const take=(name,n=4)=>{
   const field=task[name];if(!field)return;
   const value=read(pointer+BigInt(field.offset),n);
   output[name]=n===8?hex(value):Number(value);
  };
  for(const name of ['pid','tgid','policy','prio','static_prio','normal_prio','rt_priority','on_rq','on_cpu'])take(name);
  output.comm=text(pointer+BigInt(layout.comm),16);
  if(task.sched_class){
   const address=read(pointer+BigInt(task.sched_class.offset));output.sched_class=hex(address);
   const exact=symbols.filter(s=>s.address===address);
   if(exact.length===1)output.sched_class_symbol=exact[0].name;
  }
  if(task.group_leader){
   const leader=read(pointer+BigInt(task.group_leader.offset));
   output.group_leader=hex(leader);
   if(leader>=0xffff000000000000n){
    output.group_leader_pid=Number(read(leader+BigInt(task.pid.offset),4));
    output.group_leader_comm=text(leader+BigInt(layout.comm),16);
   }
  }
  try{
   const mm=read(pointer+BigInt(task.mm.offset));
   if(mm){const mml=members(type('mm_struct'));
    if(mml.exe_file){const file=read(mm+BigInt(mml.exe_file.offset));
     if(file)output.exe_path=mountedFilePath(file,{read,layout:name=>
      Object.fromEntries(Object.entries(members(type(name))).map(([key,field])=>[key,{offset:field.offset}]))});
    }
   }
  }catch(e){output.exe_path_error=e.message;}
  return output;
 };
 let schedulerProbe;
 if(options?.schedulerProbe){
  const selected=tasks.filter(t=>['init','odsign','keystore2','servicemanager','hwservicemanager'].includes(t.name)).slice(0,32);
  schedulerProbe={schema:'reverse-android-task-scheduler-readonly-v1',
   current_task_verified:currentTaskVerification==='process_list'||currentTaskVerification==='process_thread_list',
   source_machine_ticks:m(27).toString(),timer_frequency_hz:1000000,
   virtual_timer:{control:Number(m(34)),compare:m(30).toString(),pending:Number(m(34))%2===1&&m(27)>=m(30)},
   virtual_irq:{daif:Number(m(6)),cpu_interface:Number(m(36)),distributor:Number(m(35)),
    enabled_irq0:hex(m(44)),pending_irq0:hex(m(46)),active_irq0:hex(m(48))},
   current:currentTask?schedPeek(currentTask):null,
   observed_service_leaders:selected.map(t=>schedPeek(BigInt(t.task))),read_only:true};
  if(currentTask&&Number(m(4))===0){
   // The current EL0 address is not a kernel symbol: identify its actual
   // file-backed executable VMA before claiming any hot library or function.
   try{
    const mm=read(currentTask+BigInt(task.mm.offset));
    check(mm>=0xffff000000000000n,'Current process has no valid mm');
    const layoutOf=name=>Object.fromEntries(Object.entries(members(type(name))).map(([k,v])=>{
     const t=resolveType(types[v.type]);return [k,{offset:v.offset,count:t.kind===3?kernel.readUInt32LE(t.at+20):null}];
    }));
    const areas=enumerateMapleVmas({mm,read,layout:layoutOf,maxVmas:8192});
    const currentPc=u(2),vma=areas.find(x=>currentPc>=x.start&&currentPc<x.end);
    check(vma,'Current EL0 PC not mapped in process VMA tree');
    const mapped={start:hex(vma.start),end:hex(vma.end),pc:hex(currentPc),
     file_page_offset:vma.filePageOffset.toString(),
     offset_in_vma:(currentPc-vma.start).toString(),is_file_backed:vma.file!==0n};
    if(vma.file){
     mapped.file=mountedFilePath(vma.file,{read,layout:layoutOf});
     mapped.offset_in_file=(vma.filePageOffset*4096n+currentPc-vma.start).toString();
    }
    schedulerProbe.current_pc_mapping=mapped;
   }catch(e){schedulerProbe.current_pc_mapping_error=e.message;}
  }
 }
 const rwsem=members(type('rw_semaphore')),sem=symbol('trace_event_sem');
 const countValue=read(sem+BigInt(rwsem.count.offset)),owner=read(sem+BigInt(rwsem.owner.offset)),ownerTask=tasks.find(t=>t.task===hex(owner&~7n));
 // The known initramfs globals are selected by index to avoid ambiguous
 // generic names such as "state" in the same immutable kernel.
 const globals={};
 for(const [name,index]of [['name_buf',93094],['state',93095],['this_header',93096],['message',93097],
  ['byte_count',93099],['remains',93103],['name_len',93106],['body_len',93107],['next_header',93108],['wfile',93119],['wfile_pos',93120]]){
  const s=symbols[index];check(s.name===name,'initramfs symbol profile mismatch');globals[name]={address:hex(s.address)};
  try{const value=read(s.address);globals[name].value=hex(value);if((name==='message'||name==='name_buf')&&value){try{globals[name].text=text(value,256);}catch(e){globals[name].text_error=e.message;}}}
  catch(error){globals[name].read_error=error.message;}
 }
 const filePointer=globals.wfile.value===undefined?null:BigInt(globals.wfile.value),rootfsFile={pointer:filePointer===null?null:hex(filePointer)};
 if(filePointer&&globals.state.value==='0x5')try{
  const f=members(type('file')),ino=members(type('inode')),path=members(type('path')),
   dent=members(type('dentry')),qstr=members(type('qstr'));
  const inode=read(filePointer+BigInt(f.f_inode.offset));
  rootfsFile.inode=hex(inode);rootfsFile.size=read(inode+BigInt(ino.i_size.offset)).toString();
  rootfsFile.blocks=read(inode+BigInt(ino.i_blocks.offset)).toString();
  const dentry=read(filePointer+BigInt(f.f_path.offset+path.dentry.offset));
  const name=read(dentry+BigInt(dent.d_name.offset+qstr.name.offset));
  rootfsFile.name=text(name,256);rootfsFile.file_pos=read(filePointer+BigInt(f.f_pos.offset)).toString();
 }catch(error){rootfsFile.read_error=error.message;}
 const capacity={logical_ram_bytes:Number(u(0)),page_bytes:4096};
 try{
  const enums=types.filter(t=>t?.kind===6&&t.name==='zone_stat_item');
  check(enums.length>0,'missing zone vmstat enum');
  const entries=Array.from({length:enums[0].vlen},(_,i)=>{const at=enums[0].at+12+i*8;return {name:str(kernel.readUInt32LE(at)),value:kernel.readInt32LE(at+4)};});
  const free=entries.filter(x=>x.name==='NR_FREE_PAGES');check(free.length===1&&free[0].value>=0&&free[0].value<128,'invalid free-page vmstat index');
  const total=read(symbol('_totalram_pages')),reserve=read(symbol('totalreserve_pages')),available=read(symbol('vm_zone_stat')+BigInt(free[0].value*8));
  check(total<=u(0)/4096n&&reserve<=total&&available<=total,'RAM counter sanity check');
  Object.assign(capacity,{managed_pages:Number(total),reserved_pages:Number(reserve),global_free_pages:Number(available),managed_bytes:Number(total*4096n),global_free_bytes:Number(available*4096n),vmstat_per_cpu_batches_may_lag:true,android_framework_capacity_proven:false});
 }catch(e){capacity.read_error=e.message;}
 const liveProperty=options?.liveProperties?observePid1BootProperty({tasks,read,physicalByte:physical,physical64:phys64,translateKernel:translate,userAddressBits:64-Number(tcr&63n),layout:name=>Object.fromEntries(Object.entries(members(type(name))).map(([key,member])=>{const t=resolveType(types[member.type]);return [key,{offset:member.offset,count:t.kind===3?kernel.readUInt32LE(t.at+20):null}];}))}):undefined;
 let binderQueues;
 if(options?.binderDebug){
  try{
   binderQueues=inspectBinderQueues({read,globalAddress:symbol('binder_procs'),layouts:binderLayoutDebug.layouts,tasks});
  }catch(e){binderQueues={schema:'reverse-binder-live-queues-v1',read_only:true,error:e.message};}
 }
 let liveSecurity;
 if(options?.liveProperties){
  liveSecurity={read_only:true,btf_layout_verified:false,selinux_initialized:false,selinux_enforcing:false,policy_present:false};
  try{
   const fields=members(type('selinux_state')),address=symbol('selinux_state');
   const boolean=name=>{const f=fields[name];check(f&&resolveType(types[f.type]).kind===1&&resolveType(types[f.type]).type===1,'SELinux bool BTF layout '+name);const value=read(address+BigInt(f.offset),1);check(value===0n||value===1n,'invalid SELinux bool '+name);return value===1n;};
   check(fields.policy&&resolveType(types[fields.policy.type]).kind===2,'SELinux policy BTF layout');
   const policy=read(address+BigInt(fields.policy.offset));check(policy===0n||policy>=0xffff000000000000n,'invalid SELinux policy pointer');
   Object.assign(liveSecurity,{address:hex(address),btf_layout_verified:true,selinux_initialized:boolean('initialized'),selinux_enforcing:boolean('enforcing'),policy_present:policy!==0n});
  }catch(error){liveSecurity.read_error=error.message;liveSecurity.btf_layout_verified=false;}
 }
const zygoteLaunch=options?.zygoteProbe?{schema:'reverse-c40-zygote-argv-v1',read_only:true,source_checkpoint_sha256:originalSha,processes:tasks.filter(t=>t.zygote_startup).map(t=>({...t.zygote_startup,state:t.state,exit_state:t.exit_state,has_user_mm:t.has_user_mm})),live_process_count:tasks.length}:undefined;
 check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');
 return {...(zygoteLaunch?{zygote_launch:zygoteLaunch}:{}),...(schedulerProbe?{scheduler_probe:schedulerProbe}:{}),...(binderQueues?{binder_live_queues:binderQueues}:{}),...(binderLayoutDebug?{binder_layout_debug:binderLayoutDebug}:{}),...(liveProperty?{live_boot_property:liveProperty,live_kernel_security:liveSecurity}:{}),status:'read_only_kernel_observation',checkpoint_sha256:originalSha,kernel_sha256:KERNEL,instructions:provenance.instructions,pc:hex(u(2)),pc_symbol:nearest(u(2)),next_opcode:hex(read(u(2),4)),physical_pc:hex(translate(u(2))),ticks:m(27).toString(),exceptions:m(28).toString(),current_task:currentTask===null?null:hex(currentTask),current_task_source:currentTaskSource,current_task_lookup:currentLookup,current_task_read_error:currentTaskError,current_pid:currentDetails?.pid??null,current_task_details:currentDetails,capacity,task_layout:layout,task_count:tasks.length,process_leader_count:tasks.length,all_threads_enumerated:false,tasks,trace_event_sem:{address:hex(sem),count:hex(countValue),owner:hex(owner),owner_pid:ownerTask?.pid??null},initramfs:globals,rootfs_file:rootfsFile,android_init_observed:false,android_boot:false,input_unchanged:true};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 try{check(process.argv[2],'usage: node android_kernel_observer.mjs checkpoint.rvg [expected_sha256]');const maxSnapshotBytes=process.argv[4]===undefined?256*1024*1024:Number(process.argv[4]);check(Number.isSafeInteger(maxSnapshotBytes)&&maxSnapshotBytes>=360&&maxSnapshotBytes<=2*1024*1024*1024,'invalid host transport bound');check(statSync(process.argv[2]).size<=maxSnapshotBytes,'oversized checkpoint');console.log(JSON.stringify(observeAndroidKernel(readFileSync(process.argv[2]),process.argv[3],{maxSnapshotBytes,liveProperties:process.argv.includes('--live-properties'),binderDebug:process.argv.includes('--binder-probe'),schedulerProbe:process.argv.includes('--scheduler-probe'),zygoteProbe:process.argv.includes('--zygote-probe')}),null,2));}catch(e){console.error(e.message);process.exitCode=1;}
}
