// Supplemental read-only inspection of original HAL executables and real PTYs.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,statSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {mountedFilePath,userMemoryReader} from './android_live_property.mjs';
const OEM='/apex/com.google.cf.oemlock/bin/hw/android.hardware.oemlock-service.remote';
const SENSORS='/vendor/bin/hw/android.hardware.sensors-service.multihal';
const DEFAULT_PEER='/data/local/tmp/reverseandroid-peer-e11a.elf';
function inspect(api,peerPath){
  const {read,type,members,resolveType,types,tasks,task,translate,physical,phys64,m}=api;
  const layout=name=>Object.fromEntries(Object.entries(members(type(name))).map(([key,v])=>{const t=resolveType(types[v.type]);return [key,{offset:v.offset,count:t.kind===3?api.kernel.readUInt32LE(t.at+20):null}];}));
  const ml=members(type('mm_struct')),fl=members(type('file')),il=members(type('inode'));
  const fs=members(type('files_struct')),ft=members(type('fdtable')),cred=members(type('cred'));
  const processes=[],errors=[];
  for(const t of tasks){
    if(t.exit_state!==0||!t.has_user_mm)continue;
    const pointer=BigInt(t.task);if(Number(read(pointer+BigInt(task.tgid.offset),4))!==t.pid)continue;
    try{
      const mm=read(pointer+BigInt(task.mm.offset)),exe=read(mm+BigInt(ml.exe_file.offset));
      const path=mountedFilePath(exe,{read,layout});if(![OEM,SENSORS,peerPath].includes(path))continue;
      const cp=read(pointer+BigInt(task.cred.offset));
      const credentials=Object.fromEntries(['uid','euid','gid','egid'].map(k=>[k,Number(read(cp+BigInt(cred[k].offset),4))]));
      const start=read(mm+BigInt(ml.arg_start.offset)),end=read(mm+BigInt(ml.arg_end.offset));assert.ok(end>=start&&end-start<=32768n);
      const user=userMemoryReader({pgdPhysical:translate(read(mm+BigInt(ml.pgd.offset))),userAddressBits:64-Number(m(11)&63n),physicalByte:physical,physical64:phys64});
      const process={pid:t.pid,name:t.name,task:t.task,exe_path:path,argv:user.read(start,Number(end-start)).toString().split('\0').filter(Boolean),credentials,state:t.state,exit_state:t.exit_state,files:[]};
      const files=read(pointer+BigInt(task.files.offset)),table=read(files+BigInt(fs.fdt.offset));
      const max=Number(read(table+BigInt(ft.max_fds.offset),4)),array=read(table+BigInt(ft.fd.offset));assert.ok(max>0&&max<=4096);
      for(let fd=0;fd<max;fd++){
        const fp=read(array+BigInt(fd*8));if(!fp)continue;
        const ip=read(fp+BigInt(fl.f_inode.offset));if(!ip)continue;
        if(Number(read(ip+BigInt(il.i_mode.offset),2)&0xf000n)!==0x2000)continue;
        const entry={fd,file_pointer:'0x'+fp.toString(16),path:mountedFilePath(fp,{read,layout}),rdev:'0x'+read(ip+BigInt(il.i_rdev.offset),4).toString(16)};
        if(/^\/dev\/pts\/(?:[0-9]+|ptmx)$/.test(entry.path)||entry.path==='/dev/ptmx'){
          try{
            const privateLayout=members(type('tty_file_private')),ttyLayout=members(type('tty_struct'));
            const priv=read(fp+BigInt(fl.private_data.offset)),tty=read(priv+BigInt(privateLayout.tty.offset));
            assert.equal(read(priv+BigInt(privateLayout.file.offset)),fp,'tty file backlink');
            const linked=read(tty+BigInt(ttyLayout.link.offset));assert.ok(linked>=0xffff000000000000n);
            assert.equal(read(linked+BigInt(ttyLayout.link.offset)),tty,'PTY reciprocal backlink');
            const index=Number(read(tty+BigInt(ttyLayout.index.offset),4));assert.ok(index>=0&&index<4096);
            assert.equal(Number(read(linked+BigInt(ttyLayout.index.offset),4)),index);
            entry.tty={pointer:'0x'+tty.toString(16),linked_pointer:'0x'+linked.toString(16),index,file_and_reciprocal_PTY_backlinks_verified:true};
          }catch(error){entry.tty_error=error.message;}
        }
        process.files.push(entry);
      }
      processes.push(process);
    }catch(error){errors.push({pid:t.pid,error:error.message});}
  }
  const peer=processes.filter(p=>p.exe_path===peerPath&&p.credentials.uid===0&&p.credentials.euid===0);
  const oem=processes.filter(p=>p.exe_path===OEM&&p.credentials.uid===1064),sensors=processes.filter(p=>p.exe_path===SENSORS&&p.credentials.uid===1000);
  const pairs=[];
  if(peer.length===1)for(const hal of [...oem,...sensors])for(const slave of hal.files){
    if(!slave.tty)continue;
    for(const master of peer[0].files){
      if(!master.tty||!['/dev/pts/ptmx','/dev/ptmx'].includes(master.path))continue;
      if(master.tty.pointer===slave.tty.linked_pointer&&master.tty.linked_pointer===slave.tty.pointer)
        pairs.push({HAL_pid:hal.pid,HAL_executable:hal.exe_path,HAL_fd:slave.fd,peer_pid:peer[0].pid,peer_fd:master.fd,PTY_index:slave.tty.index,actual_kernel_PTY_backlinks_verified:true});
    }
  }
  const count=exe=>new Set(pairs.filter(p=>p.HAL_executable===exe).map(p=>p.PTY_index)).size;
  return {schema:'reverseandroid-original-peripheral-HAL-PTY-readonly-v1',read_only:true,processes,inspection_errors:errors,original_oemlock_executable_alive:oem.length===1,original_sensors_executable_alive:sensors.length===1,real_AArch64_peer_alive:peer.length===1,paired_FDs:pairs,OEM_channel_connected_to_actual_peer:count(OEM)===1,two_separate_sensor_channels_connected_to_actual_peer:count(SENSORS)===2,boot_or_UI_completion_claimed:false};
}
export async function probePeripherals(path,sha,peerPath=DEFAULT_PEER){
  assert.ok(/^\/data\/local\/tmp\/reverseandroid-peer-(e11a|abi2)\.elf$/.test(peerPath));
  assert.ok(statSync(path).size<=2147483648);const url=new URL('./android_kernel_observer.mjs',import.meta.url),source=read(url,'utf8');
  const pin=JSON.parse(read(new URL('../state/upstream-snapshot.json',import.meta.url))).files.find(f=>f.path==='tools/android_kernel_observer.mjs');
  assert.equal(createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex'),pin.git_blob_sha1);
  const marker=" check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');";assert.equal(source.split(marker).length,2);
  const hook=' options?.extraReadOnlyProbe?.({read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest,btfString:str});\n';
  const module=source.replace(marker,hook+marker).replace(/from (['"])(\.\/[^'"]+)\1/g,(_,q,s)=>'from '+q+new URL(s,url).href+q);
  const {observeAndroidKernel}=await import('data:text/javascript;base64,'+Buffer.from(module).toString('base64'));
  const bytes=read(path);let result;
  const evidence=observeAndroidKernel(bytes,sha,{maxSnapshotBytes:2147483648,liveProperties:true,extraReadOnlyProbe:api=>{result=inspect(api,peerPath);}});
  assert.equal(evidence.input_unchanged,true);assert.ok(result);return {...result,expected_peer_executable:peerPath,checkpoint_sha256:sha,instructions:evidence.instructions,input_unchanged:true,original_observer_source_git_blob_sha1:pin.git_blob_sha1};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
  const [path,sha,output,peerPath]=process.argv.slice(2);assert.ok(path&&sha&&output);const result=await probePeripherals(path,sha,peerPath);write(output,JSON.stringify(result,null,2)+'\n',{flag:'wx'});console.log(JSON.stringify({checkpoint_sha256:sha,original_oemlock_executable_alive:result.original_oemlock_executable_alive,original_sensors_executable_alive:result.original_sensors_executable_alive,real_AArch64_peer_alive:result.real_AArch64_peer_alive,paired_FDs:result.paired_FDs}));
}catch(error){console.error(error.stack);process.exitCode=1;}}
