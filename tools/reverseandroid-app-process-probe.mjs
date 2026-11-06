// Supplemental, read-only app identity evidence from the original kernel/BTF.
import assert from 'node:assert/strict';
import {readFileSync,statSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {mountedFilePath,userMemoryReader} from './android_live_property.mjs';
import {inspectTaskScheduling} from './reverseandroid-scheduler-group.mjs';

export function isSettingsProcess(row) {
  return row.identity_verified === true && row.exit_state === 0 && row.leader_verified === true
    && row.exe_path === '/system/bin/app_process64'
    && row.argv?.length === 1 && row.argv[0] === 'com.android.settings'
    && Number.isSafeInteger(row.pid) && row.pid > 1
    && Number.isSafeInteger(row.parent_pid) && row.parent_pid > 1
    && /^\d+$/.test(row.credentials?.uid ?? '')
    && row.credentials.uid === row.credentials.euid && Number(row.credentials.uid) >= 1000;
}

function inspectApps(api) {
  const {read,types,type,members,task,tasks,translate,physical,phys64,m}=api;
  const fields=members(type('mm_struct'));
  for (const key of ['arg_start','arg_end','pgd','exe_file']) assert.ok(fields[key]);
  const layout=name=>Object.fromEntries(Object.entries(members(type(name))).map(([k,v])=>[k,{offset:v.offset}]));
  const rows=[], errors=[];
  for (const item of tasks.filter(x=>x.pid>1 && x.has_user_mm && x.exit_state===0)) {
    const pointer=BigInt(item.task);
    if (read(pointer+BigInt(task.group_leader.offset))!==pointer) continue;
    try {
      const mm=read(pointer+BigInt(task.mm.offset)); assert.ok(mm>=0xffff000000000000n);
      const file=read(mm+BigInt(fields.exe_file.offset)); assert.ok(file>=0xffff000000000000n);
      const exe=mountedFilePath(file,{read,layout});
      if (exe!=='/system/bin/app_process64') continue;
      const start=read(mm+BigInt(fields.arg_start.offset)), end=read(mm+BigInt(fields.arg_end.offset));
      assert.ok(start>0n && end>start && end-start<=4096n);
      const pgd=read(mm+BigInt(fields.pgd.offset)); assert.ok(pgd>=0xffff000000000000n);
      const memory=userMemoryReader({pgdPhysical:translate(pgd),userAddressBits:64-Number(m(11)&63n),physicalByte:physical,physical64:phys64});
      const raw=memory.read(start,Number(end-start)); assert.equal(raw.at(-1),0);
      const argv=raw.toString('utf8').split('\0').filter(Boolean);
      assert.ok(argv.length>0 && argv.length<=64 && argv.every(x=>/^[\x20-\x7e]{1,512}$/.test(x)));
      const parent=read(pointer+BigInt(task.real_parent.offset)); assert.ok(parent>=0xffff000000000000n);
      const credentials=inspectTaskScheduling(api,pointer).credentials; assert.ok(credentials);
      rows.push({pid:item.pid,comm:item.name,exit_state:item.exit_state,leader_verified:true,
        exe_path:exe,argv,parent_pid:Number(read(parent+BigInt(task.pid.offset),4)),credentials,identity_verified:true});
    } catch(error) { errors.push({pid:item.pid,comm:item.name,identity_unverified:error.message}); }
  }
  return {schema:'reverseandroid-app-identity-probe-v1',read_only:true,processes:rows,unverified:errors,
    settings_processes:rows.filter(isSettingsProcess),guest_CPU_instructions_executed:0};
}

export async function probeApps(path,sha) {
  const originalUrl=new URL('./android_kernel_observer.mjs',import.meta.url),source=readFileSync(originalUrl,'utf8');
  const manifest=JSON.parse(readFileSync(new URL('../state/upstream-snapshot.json',import.meta.url)));
  const pin=manifest.files.find(x=>x.path==='tools/android_kernel_observer.mjs'); assert.ok(pin);
  const blob=createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex');
  assert.equal(blob,pin.git_blob_sha1);
  const marker=" check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');";
  assert.equal(source.split(marker).length,2);
  const hook=' options?.extraReadOnlyProbe?.({read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest});\n';
  const module=source.replace(marker,hook+marker).replace(/from (['"])(\.\/[^'"]+)\1/g,(_,q,spec)=>'from '+q+new URL(spec,originalUrl).href+q);
  const {observeAndroidKernel}=await import('data:text/javascript;base64,'+Buffer.from(module).toString('base64'));
  assert.ok(statSync(path).size<=2147483648); let apps;
  const proof=observeAndroidKernel(readFileSync(path),sha,{maxSnapshotBytes:2147483648,liveProperties:true,zygoteProbe:true,
    schedulerProbe:true,extraReadOnlyProbe:api=>{apps=inspectApps(api);}});
  assert.ok(apps); assert.equal(proof.input_unchanged,true);
  return {...apps,checkpoint_sha256:proof.checkpoint_sha256,instructions:proof.instructions,input_unchanged:true,
    observer_source_git_blob_sha1:blob,original_observer_file_unchanged:true,original_CPU_and_codecs_unchanged:true};
}

if(process.argv[1] && import.meta.url===pathToFileURL(resolve(process.argv[1])).href) {
  const [path,sha,output]=process.argv.slice(2); assert.ok(path&&sha&&output);
  const result=await probeApps(path,sha); writeFileSync(output,JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify({instructions:result.instructions,settings_processes:result.settings_processes,input_unchanged:true}));
}
