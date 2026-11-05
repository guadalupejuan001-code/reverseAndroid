// Supplemental read-only evidence for the real boot property and lights HAL.
// The original observer remains unchanged and still hashes the entire input.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,statSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {enumerateMapleVmas,mountedFilePath,userMemoryReader} from './android_live_property.mjs';
import {inspectBootPropertyArea} from './android_boot_property.mjs';
const TARGETS=['ro.boot.vsock_lights_port','init.svc.vendor.light-cuttlefish','init.svc_debug_pid.vendor.light-cuttlefish'];
function inspect(api){
  const {read,type,members,resolveType,types,tasks,task,translate,physical,phys64,m}=api;
  const layout=name=>Object.fromEntries(Object.entries(members(type(name))).map(([key,v])=>{const t=resolveType(types[v.type]);return [key,{offset:v.offset,count:t.kind===3?api.kernel.readUInt32LE(t.at+20):null}];}));
  const leader=tasks.find(t=>t.pid===1);assert.ok(leader&&leader.name==='init'&&leader.exit_state===0&&leader.has_user_mm);
  const mm=read(BigInt(leader.task)+BigInt(task.mm.offset)),ml=members(type('mm_struct'));
  const user=pointer=>userMemoryReader({pgdPhysical:translate(read(pointer+BigInt(ml.pgd.offset))),userAddressBits:64-Number(m(11)&63n),physicalByte:physical,physical64:phys64});
  const memory=user(mm),areas=enumerateMapleVmas({mm,read,layout}),observations=[];
  for(const area of areas){
    if(!area.file||area.filePageOffset||!(area.flags&1n)||!(area.flags&8n))continue;
    let file;try{file=mountedFilePath(area.file,{read,layout});}catch{continue;}
    if(!/^\/dev\/__properties__\/u:object_r:[a-zA-Z0-9_]+:s0$/.test(file))continue;
    const first=memory.read(area.start,4096);
    for(const name of TARGETS){
      let value=inspectBootPropertyArea(first,{diagnostics:true,name});
      if(value?.status==='needs_virtual_page_mapping'){
        const n=Math.ceil((first.readUInt32LE(0)+128)/4096)*4096;assert.ok(n>=4096&&n<=1048576&&area.start+BigInt(n)<=area.end);
        value=inspectBootPropertyArea(memory.read(area.start,n),{diagnostics:true,name});
      }
      if(value?.status==='property_observed')observations.push({...value,file,pid:1,task:leader.task,vma:'0x'+area.start.toString(16),vma_shared:true,live_pid1_mapping_verified:true});
    }
  }
  const samples=name=>observations.filter(o=>o.name===name),ports=samples(TARGETS[0]),states=samples(TARGETS[1]),pids=samples(TARGETS[2]);
  const candidates=pids.length===1&&/^[1-9][0-9]*$/.test(pids[0].value)?tasks.filter(t=>t.pid===Number(pids[0].value)):[];
  let process=null;
  if(candidates.length===1){
    const t=candidates[0],pointer=BigInt(t.task),pmm=read(pointer+BigInt(task.mm.offset));process={pid:t.pid,name:t.name,state:t.state,exit_state:t.exit_state,has_user_mm:t.has_user_mm};
    if(pmm){try{
      const executable=read(pmm+BigInt(ml.exe_file.offset));process.exe_path=mountedFilePath(executable,{read,layout});
      const start=read(pmm+BigInt(ml.arg_start.offset)),end=read(pmm+BigInt(ml.arg_end.offset));assert.ok(end>=start&&end-start<=32768n);
      process.argv=user(pmm).read(start,Number(end-start)).toString().split('\0').filter(Boolean);
    }catch(error){process.identity_read_error=error.message;}}
  }
  return {schema:'reverseandroid-lights-live-profile-v1',read_only:true,live_pid1_property_observations:observations,
    configured_port_verified:ports.length===1&&ports[0].value==='6900',init_reports_service_running:states.length===1&&states[0].value==='running',
    service_process:process,android_boot_completion_proved_by_this_probe:false};
}
export async function probeLights(path,sha){
  assert.ok(statSync(path).size<=2147483648);const url=new URL('./android_kernel_observer.mjs',import.meta.url),source=read(url,'utf8');
  const pin=JSON.parse(read(new URL('../state/upstream-snapshot.json',import.meta.url))).files.find(f=>f.path==='tools/android_kernel_observer.mjs');
  assert.equal(createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex'),pin.git_blob_sha1);
  const marker=" check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');";assert.equal(source.split(marker).length,2);
  const hook=' options?.extraReadOnlyProbe?.({read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest});\n';
  const module=source.replace(marker,hook+marker).replace(/from (['"])(\.\/[^'"]+)\1/g,(_,q,s)=>'from '+q+new URL(s,url).href+q);
  const {observeAndroidKernel}=await import('data:text/javascript;base64,'+Buffer.from(module).toString('base64'));
  const bytes=read(path);let result;
  const evidence=observeAndroidKernel(bytes,sha,{maxSnapshotBytes:2147483648,liveProperties:true,extraReadOnlyProbe:api=>{result=inspect(api);}});
  assert.equal(evidence.input_unchanged,true);assert.ok(result);return {...result,checkpoint_sha256:sha,instructions:evidence.instructions,input_unchanged:true};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
  const [path,sha,output]=process.argv.slice(2);assert.ok(path&&sha&&output);const result=await probeLights(path,sha);write(output,JSON.stringify(result,null,2)+'\n',{flag:'wx'});console.log(JSON.stringify({checkpoint_sha256:sha,configured_port_verified:result.configured_port_verified,init_reports_service_running:result.init_reports_service_running,pid:result.service_process?.pid??null}));
}catch(error){console.error(error.message);process.exitCode=1;}}
