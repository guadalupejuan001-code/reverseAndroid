// C91D3 supplemental read-only live property probe for selected Android boot properties.
import '../tools/reverseandroid-host-transport-preload.mjs';
import assert from 'node:assert/strict';
import {readFileSync,statSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {enumerateMapleVmas,mountedFilePath,userMemoryReader} from '../tools/android_live_property.mjs';
import {inspectBootPropertyArea} from '../tools/android_boot_property.mjs';
import {HOST_RVG_CAP} from '../tools/reverseandroid-host-rvg-reader.mjs';

const DEFAULTS=[
  'sys.boot_completed','dev.bootcomplete','sys.cppreopt','init.svc.cppreopts',
  'init.svc.bootanim','init.svc.zygote','init.svc.zygote_secondary',
  'init.svc.surfaceflinger','init.svc.servicemanager','init.svc.hwservicemanager',
  'init.svc.vold','init.svc.installd','apexd.status'
];

function inspect(api,names){
  const {read,type,members,resolveType,types,tasks,task,translate,physical,phys64,m}=api;
  const layout=name=>Object.fromEntries(Object.entries(members(type(name))).map(([key,v])=>{
    const t=resolveType(types[v.type]);
    return [key,{offset:v.offset,count:t.kind===3?api.kernel.readUInt32LE(t.at+20):null}];
  }));
  const leaders=tasks.filter(t=>t.pid===1&&t.exit_state===0);assert.equal(leaders.length,1,'Missing unique live PID1');
  const leader=leaders[0],mmLayout=members(type('mm_struct'));
  const mm=read(BigInt(leader.task)+BigInt(task.mm.offset));assert.ok(mm>=0xffff000000000000n,'PID1 has no user mm');
  const pgd=read(mm+BigInt(mmLayout.pgd.offset));
  const memory=userMemoryReader({pgdPhysical:translate(pgd),userAddressBits:64-Number(m(11)&63n),physicalByte:physical,physical64:phys64});
  const areas=enumerateMapleVmas({mm,read,layout}),observations=[],mappings=[];
  for(const area of areas){
    if(!area.file||area.filePageOffset||!(area.flags&1n)||!(area.flags&8n))continue;
    let path;try{path=mountedFilePath(area.file,{read,layout});}catch{continue;}
    if(!/^\/dev\/__properties__\/u:object_r:[a-zA-Z0-9_]+:s0$/.test(path))continue;
    mappings.push({path,start:'0x'+area.start.toString(16),end:'0x'+area.end.toString(16)});
    const first=memory.read(area.start,4096);
    for(const name of names){
      let sample=inspectBootPropertyArea(first,{diagnostics:true,name});
      if(sample?.status==='needs_virtual_page_mapping'){
        const n=Math.ceil((first.readUInt32LE(0)+128)/4096)*4096;
        assert.ok(n>=4096&&n<=1048576&&area.start+BigInt(n)<=area.end,'Property area exceeds VMA');
        sample=inspectBootPropertyArea(memory.read(area.start,n),{diagnostics:true,name});
      }
      if(sample?.status==='property_observed')observations.push({file:path,vma:'0x'+area.start.toString(16),...sample});
    }
  }
  const properties={};
  for(const name of names){
    const rows=observations.filter(x=>x.name===name),values=[...new Set(rows.map(x=>x.value))];
    properties[name]={status:rows.length?(values.length===1?'observed':'ambiguous'):'not_observed',values,observations:rows};
  }
  return {schema:'reverseandroid-c91d3-selected-live-properties-v1',read_only:true,pid1:leader.pid,property_mappings:mappings.length,names,properties};
}

async function probe(path,sha,names){
  assert.match(sha,/^[0-9a-f]{64}$/);assert.ok(statSync(path).size<=HOST_RVG_CAP,'Oversized RVG');
  const originalUrl=new URL('../tools/android_kernel_observer.mjs',import.meta.url),source=readFileSync(originalUrl,'utf8');
  const manifest=JSON.parse(readFileSync(new URL('../state/upstream-snapshot.json',import.meta.url)));
  const pin=manifest.files.find(x=>x.path==='tools/android_kernel_observer.mjs');assert.ok(pin);
  const blob=createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex');assert.equal(blob,pin.git_blob_sha1);
  const marker=" check(createHash('sha256').update(bytes).digest('hex')===originalSha,'observation mutated input');";
  assert.equal(source.split(marker).length,2,'Unique read-only observer completion marker required');
  const hook=' options?.extraReadOnlyProbe?.({read,text,types,type,members,resolveType,kernel,tasks,task,currentTask,u,m,translate,physical,phys64,nearest,btfString:str});\n';
  const patched=source.replace(marker,hook+marker).replace(/from (['"])(\.\/[^'"]+)\1/g,(_,q,s)=>'from '+q+new URL(s,originalUrl).href+q);
  const {observeAndroidKernel}=await import('data:text/javascript;base64,'+Buffer.from(patched).toString('base64'));
  let selected;const bytes=readFileSync(path);
  const proof=observeAndroidKernel(bytes,sha,{maxSnapshotBytes:HOST_RVG_CAP,liveProperties:true,extraReadOnlyProbe:api=>{selected=inspect(api,names);}});
  assert.ok(selected);assert.equal(proof.input_unchanged,true);
  return {...selected,checkpoint_sha256:sha,instructions:proof.instructions,input_unchanged:true,original_observer_source_git_blob_sha1:blob};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const [path,sha,output,...names]=process.argv.slice(2);assert.ok(path&&sha&&output);
    const targets=names.length?names:DEFAULTS;assert.ok(targets.length>=1&&targets.length<=64&&targets.every(x=>/^[a-zA-Z0-9._-]{1,90}$/.test(x)));
    const result=await probe(path,sha,targets);writeFileSync(output,JSON.stringify(result,null,2)+'\n',{flag:'wx'});
    console.log(JSON.stringify({checkpoint_sha256:sha,properties:Object.fromEntries(Object.entries(result.properties).map(([k,v])=>[k,v.values]))}));
  }catch(error){console.error(error.stack);process.exitCode=1;}
}
