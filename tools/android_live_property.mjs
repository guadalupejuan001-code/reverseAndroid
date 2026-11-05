// Read-only PID1 VMA/property mapping from the byte-pinned kernel's BTF layout.
// Maple node tags/end rules: Linux v6.12 include/linux/maple_tree.h, lib/maple_tree.c.
import assert from 'node:assert/strict';import {inspectBootPropertyArea} from './android_boot_property.mjs';
export const ODSIGN_BOOT_PATH_PROPERTIES=Object.freeze(['odsign.key.done','odsign.verification.done','init.svc.odsign','init.svc.keystore2','init.svc.android.hardware.security.keymint-service','init.svc.zygote','init.svc_debug_pid.zygote','init.svc.zygote_secondary','apexd.status']);
const MASK=0xffffffffffffffffn,PA=0x0000fffffffff000n,hex=x=>'0x'+x.toString(16);
export function enumerateMapleVmas({mm,read,layout,maxVmas=8192}){
 const m=layout('mm_struct'),tree=mm+BigInt(m.mm_mt.offset),t=layout('maple_tree'),v=layout('vm_area_struct'),nodeLayout=layout('maple_node');
 const count=Number(read(mm+BigInt(m.map_count.offset),4));assert.ok(count>=1&&count<=maxVmas,'VMA count outside bound');
 const root=read(tree+BigInt(t.ma_root.offset)),seen=new Set(),areas=[];
 const add=(pointer,lo,hi)=>{
  assert.ok(pointer>=0xffff000000000000n&&(pointer&7n)===0n,'Invalid VMA pointer');
  const start=read(pointer+BigInt(v.vm_start.offset)),end=read(pointer+BigInt(v.vm_end.offset));
  assert.equal(read(pointer+BigInt(v.vm_mm.offset)),mm,'VMA belongs to another mm');assert.equal(start,lo,'VMA lower bound');assert.equal(end-1n,hi,'VMA upper bound');assert.ok(end>start&&areas.length<maxVmas);
  areas.push({pointer,start,end,file:read(pointer+BigInt(v.vm_file.offset)),flags:read(pointer+BigInt(v.vm_flags.offset)),filePageOffset:read(pointer+BigInt(v.vm_pgoff.offset))});
 };
 const walk=(entry,lo,hi,parent,depth)=>{
  assert.ok(depth<=16&&seen.size<4096,'Maple traversal bound');const node=entry&~255n,type=Number((entry>>3n)&15n);
  assert.ok(node>=0xffff000000000000n&&!seen.has(node.toString()),'Invalid or repeated maple node');seen.add(node.toString());
  const p=read(node+BigInt(nodeLayout.parent.offset));assert.equal(p&~255n,parent===null?tree&~255n:parent,'Maple parent identity');
  if(parent===null){assert.equal(p&~1n,tree);assert.equal(p&1n,1n);}else assert.equal(p&1n,0n);
  assert.ok(type>=1&&type<=3,'Unsupported maple node type');const shape=layout(type===3?'maple_arange_64':'maple_range_64');
  const slots=shape.slot.count;assert.equal(slots,type===3?10:16,'Maple slot layout');assert.equal(shape.pivot.count,slots-1,'Maple pivot layout');
  const last=read(node+BigInt(shape.pivot.offset+(slots-2)*8));
  const end=type===3||last===0n?Number(read(node+BigInt(shape.meta.offset),1)):last===hi?slots-2:slots-1;
  assert.ok(end>=0&&end<slots,'Maple data end');let lower=lo;
  for(let i=0;i<=end;i++){
   const upper=i===end?hi:read(node+BigInt(shape.pivot.offset+i*8));assert.ok(upper>=lower&&upper<=hi,'Invalid maple pivots');
   const child=read(node+BigInt(shape.slot.offset+i*8));if(child){if(type===1)add(child,lower,upper);else walk(child,lower,upper,node,depth+1);}lower=upper+1n;
  }
 };
 assert.ok(root!==0n,'Empty process maple tree');
 if((root&3n)===2n)walk(root,0n,MASK,null,0);else add(root,0n,0n);
 assert.equal(areas.length,count,'Process VMA count changed or tree is transitional');areas.sort((a,b)=>a.start<b.start?-1:a.start>b.start?1:0);
 for(let i=1;i<areas.length;i++)assert.ok(areas[i-1].end<=areas[i].start,'Overlapping process VMAs');return areas;
}
export function mountedFilePath(file,{read,layout}){
  assert.ok(file>=0xffff000000000000n&&file<=MASK,'Invalid kernel file pointer');
 const f=layout('file'),path=layout('path'),d=layout('dentry'),q=layout('qstr'),vf=layout('vfsmount'),mount=layout('mount');
 let dentry=read(file+BigInt(f.f_path.offset+path.dentry.offset)),mnt=read(file+BigInt(f.f_path.offset+path.mnt.offset));const parts=[],seen=new Set();
 for(let depth=0;depth<128;depth++){
  assert.ok(dentry>=0xffff000000000000n&&mnt>=0xffff000000000000n,'Invalid kernel path pointer');
  const key=dentry+':'+mnt;assert.ok(!seen.has(key),'Dentry/mount cycle');seen.add(key);
  const root=read(mnt+BigInt(vf.mnt_root.offset));
  if(dentry===root){const current=mnt-BigInt(mount.mnt.offset),parent=read(current+BigInt(mount.mnt_parent.offset));if(parent===current)return '/'+parts.reverse().join('/');dentry=read(current+BigInt(mount.mnt_mountpoint.offset));mnt=parent+BigInt(mount.mnt.offset);continue;}
  const len=Number(read(dentry+BigInt(d.d_name.offset+q.len.offset),4)),name=read(dentry+BigInt(d.d_name.offset+q.name.offset));assert.ok(len>0&&len<=255,'Invalid dentry name');let text='';
  for(let i=0;i<len;i++){const b=Number(read(name+BigInt(i),1));assert.ok(b>=32&&b<=126&&b!==47,'Noncanonical path component');text+=String.fromCharCode(b);}parts.push(text);
  const parent=read(dentry+BigInt(d.d_parent.offset));assert.notEqual(parent,dentry,'Dentry root differs from mount root');dentry=parent;
 }
 throw Error('Mounted path depth bound');
}
export function userMemoryReader({pgdPhysical,userAddressBits,physicalByte,physical64}){
 assert.ok(Number.isInteger(userAddressBits)&&userAddressBits>=25&&userAddressBits<=48);assert.equal(pgdPhysical&4095n,0n);
 const start=4-Math.ceil((userAddressBits-12)/9),cache=new Map();
 const translate=address=>{
  assert.ok(address>=0n&&address<(1n<<BigInt(userAddressBits)),'User address outside canonical range');const page=address>>12n;if(cache.has(page))return cache.get(page)|(address&4095n);
  let table=pgdPhysical,noUser=false;
  for(let level=start;level<4;level++){
   const shift=BigInt(12+(3-level)*9),d=physical64(table+((address>>shift)&511n)*8n);assert.ok(d&1n,'User page not present');
   if(level<3&&(d&2n)){noUser||=Boolean(d&(1n<<61n));table=d&PA;continue;}
   const mask=(1n<<shift)-1n;assert.ok(!noUser&&level>0&&(level<3||Boolean(d&2n))&&!(d&(mask&~4095n))&&(d&(1n<<10n))&&(d&64n),'Invalid or inaccessible user page');
   const at=(d&PA&~mask)|(address&mask);assert.ok(cache.size<262144,'User translation cache bound');cache.set(page,at&~4095n);return at;
  }throw Error('User translation failed');
 };
 return {translate,read(address,length){assert.ok(Number.isSafeInteger(length)&&length>0&&length<=1048576);const out=Buffer.alloc(length);for(let i=0;i<length;){const at=address+BigInt(i),p=translate(at),n=Math.min(length-i,4096-Number(at&4095n));for(let j=0;j<n;j++)out[i+j]=physicalByte(p+BigInt(j));i+=n;}return out;}};
}
export function observePid1BootProperty({tasks,read,layout,translateKernel,physicalByte,physical64,userAddressBits}){
 const result={schema:'reverse-live-pid1-property-v1',read_only:true,live_property_mapping_verified:false,boot_completed_property_live:false,observations:[],boot_chain_property_observations:[],boot_chain_property_status_counts:{},android_framework_verified:false,android_display_verified:false};
 try{
  const leaders=tasks.filter(t=>t.pid===1);assert.equal(leaders.length,1,'Missing unique PID1');const leader=leaders[0],task=layout('task_struct'),mmLayout=layout('mm_struct'),mm=read(BigInt(leader.task)+BigInt(task.mm.offset));assert.ok(mm>=0xffff000000000000n,'PID1 has no valid live mm');
  const pgd=read(mm+BigInt(mmLayout.pgd.offset));assert.ok(pgd>=0xffff000000000000n,'Invalid PID1 pgd');const memory=userMemoryReader({pgdPhysical:translateKernel(pgd),userAddressBits,physicalByte,physical64});
  const areas=enumerateMapleVmas({mm,read,layout}),statusCounts={};Object.assign(result,{pid:1,task:leader.task,mm:hex(mm),vma_count:areas.length,property_mappings:0,status_counts:statusCounts});
  for(const area of areas){
   if(!area.file||area.filePageOffset||!(area.flags&1n)||!(area.flags&8n))continue;let path;
   try{path=mountedFilePath(area.file,{read,layout});}catch{result.path_read_errors=(result.path_read_errors??0)+1;continue;}
   if(!/^\/dev\/__properties__\/u:object_r:[a-zA-Z0-9_]+:s0$/.test(path))continue;
   result.property_mappings++;try{
    const first=memory.read(area.start,4096);let property=inspectBootPropertyArea(first,{diagnostics:true});
    if(property?.status==='needs_virtual_page_mapping'){
     const n=Math.ceil((first.readUInt32LE(0)+128)/4096)*4096;assert.ok(n>=4096&&n<=1048576&&area.start+BigInt(n)<=area.end,'Property area exceeds its live VMA');property=inspectBootPropertyArea(memory.read(area.start,n),{diagnostics:true});
    }
    const status=property?.status??'invalid_property_header';statusCounts[status]=(statusCounts[status]??0)+1;
    if(property?.diagnostic_error&&(result.invalid_area_samples??=[]).length<8)result.invalid_area_samples.push({file:path,status,error:property.diagnostic_error});
    if(status==='property_observed')result.observations.push({file:path,vma:hex(area.start),physical_header:hex(memory.translate(area.start)),vma_shared:true,live_pid1_mapping_verified:true,...property});
    // Each target is queried through Bionic's structured trie in PID1's
    // verified file-backed shared VMA, never by raw byte-string scanning.
    // These observations do NOT by themselves prove Zygote or successful boot.
    for(const name of ODSIGN_BOOT_PATH_PROPERTIES){
      let sample=inspectBootPropertyArea(first,{diagnostics:true,name});
      if(sample?.status==='needs_virtual_page_mapping'){
        const n=Math.ceil((first.readUInt32LE(0)+128)/4096)*4096;
        assert.ok(n>=4096&&n<=1048576&&area.start+BigInt(n)<=area.end,'Property target exceeds live VMA');
        sample=inspectBootPropertyArea(memory.read(area.start,n),{diagnostics:true,name});
      }
      const seenStatus=sample?.status??'invalid_property_header';
      const counts=result.boot_chain_property_status_counts[name]??={};
      counts[seenStatus]=(counts[seenStatus]??0)+1;
      if(seenStatus==='property_observed')result.boot_chain_property_observations.push({
        file:path,vma:hex(area.start),vma_shared:true,live_pid1_mapping_verified:true,...sample
      });
    }
   }catch(e){result.mapping_read_errors=(result.mapping_read_errors??0)+1;}
  }
  result.live_property_mapping_verified=result.observations.length>0;const values=new Set(result.observations.map(x=>x.value));result.ambiguous_property_values=values.size>1;
  result.boot_completed_property_live=result.live_property_mapping_verified&&!result.ambiguous_property_values&&values.has('1');
 }catch(e){result.read_error=e.message;}
 return result;
}
