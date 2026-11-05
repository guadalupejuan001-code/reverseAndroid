#!/usr/bin/env node
// Genuine same-build Android partition inputs, streamed with ZIP CRC32/SHA256.
import {mkdir,open,readFile,writeFile,unlink} from 'node:fs/promises';
import {createReadStream,createWriteStream,readFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {createInflateRaw,crc32} from 'node:zlib';
import {createHash} from 'node:crypto';
import {pipeline} from 'node:stream/promises';
import {Transform,PassThrough} from 'node:stream';
import {join,resolve,dirname} from 'node:path';
import {pathToFileURL} from 'node:url';
import {withOfficialArchive} from './aosp_cuttlefish_zip_probe.mjs';
import {bootParts} from './aosp_boot_preflight.mjs';
import {buildAndroidPartitionDisk,REQUIRED} from './android_partition_disk.mjs';
const need=(v,m)=>{if(!v)throw Error('Official Android partitions: '+m);};
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function acquireAndroidPartitions(directory,{userdataOnly=false}={}){
 const dir=resolve(directory);await mkdir(dirname(dir),{recursive:true});await mkdir(dir); // Never reuse a partial previous attempt.
 return withOfficialArchive(async({url,index,readRange})=>{
  need(index.total===1101174958,'unexpected same-build archive extent');
  await writeFile(join(dir,'archive-members.json'),JSON.stringify(index.entries.map(e=>({name:e.name,compressed:e.compressed,bytes:e.uncompressed})),null,2)+'\n');
  const partitions=[],extracted=[];
  for(const label of userdataOnly?['userdata']:REQUIRED){
   const name=label.replace(/_a$/,'')+'.img',entry=index.entries.find(e=>e.name===name),destination=join(dir,name);
   if(!entry&&['metadata','userdata'].includes(label)){
    const data=label==='userdata',size=data?2*1024**3:64*1024*1024,uuid=data?'e0f714d3-c7ba-5759-9170-3463fcda05ae':'a5cd1280-831d-58b0-9205-cbd222e809d2';
    const f=await open(destination,'wx');await f.truncate(size);await f.close();
    const p=spawnSync('mkfs.ext4',['-q','-F','-b','4096','-L',label,'-U',uuid,...(data?['-O','encrypt,quota,project']:[]),'-E','lazy_itable_init=0,lazy_journal_init=0,hash_seed='+uuid,destination],{env:{...process.env,E2FSPROGS_FAKE_TIME:'1793232000'},encoding:'utf8'});
    need(p.status===0,'could not format the new writable metadata partition');
    const h=createHash('sha256');for await(const b of createReadStream(destination))h.update(b);const digest=h.digest('hex');
    partitions.push({name:label,path:name,sha256:digest});extracted.push({name,generated:'new empty ext4 '+label+'; no Android system data invented',bytes:size,sha256:digest});continue;
   }
   need(entry,'required official image is missing: '+name);need(entry.compressed>0&&entry.compressed<=index.total&&entry.uncompressed>0&&entry.uncompressed<=8*1024**3,'member exceeds streaming input limit');
   const h=readRange(entry.localHeaderOffset,entry.localHeaderOffset+29,64).bytes;need(h.readUInt32LE(0)===0x04034b50&&h.readUInt16LE(8)===entry.method,'local ZIP header mismatch');
   const n=h.readUInt16LE(26),extra=h.readUInt16LE(28),local=readRange(entry.localHeaderOffset+30,entry.localHeaderOffset+29+n,65536).bytes;
   need(local.toString()===entry.name,'local ZIP member name mismatch');const begin=entry.localHeaderOffset+30+n+extra,end=begin+entry.compressed-1;
   need(end<index.centralOffset,'member overlaps ZIP central directory');
   const compressed=join(dir,name+'.compressed'),headers=join(dir,name+'.headers');
   const p=spawnSync('curl',['--silent','--show-error','--fail','--location','--max-time','600','--max-filesize',String(entry.compressed),'--range',begin+'-'+end,'--dump-header',headers,'--output',compressed,url],{encoding:'utf8',maxBuffer:4096});
   need(p.status===0,'bounded official member transport failed for '+name);
   const header=(await readFile(headers,'utf8')).trim().split(/\r?\n\r?\n/).at(-1)??'',range=/^content-range:\s*bytes\s+(\d+)-(\d+)\/(\d+)/im.exec(header);
   need(/^HTTP\/[\d.]+\s+206/m.test(header)&&range&&Number(range[1])===begin&&Number(range[2])===end&&Number(range[3])===index.total,'server did not honor exact ZIP member range');
   let size=0,crc=0;const hash=createHash('sha256');const verify=new Transform({transform(b,encoding,done){size+=b.length;if(size>entry.uncompressed){done(Error('ZIP expansion exceeded declared extent'));return;}crc=crc32(b,crc);hash.update(b);done(null,b);}});
   await pipeline(createReadStream(compressed),entry.method===8?createInflateRaw():new PassThrough(),verify,createWriteStream(destination,{flags:'wx'}));
   need(size===entry.uncompressed&&crc===entry.crc32,'official image CRC32/extent mismatch: '+name);const digest=hash.digest('hex');
   partitions.push({name:label,path:name,sha256:digest});extracted.push({name,bytes:size,sha256:digest,zip_crc32:crc});await unlink(compressed);await unlink(headers);
  }
  if(userdataOnly){const report={status:'userdata_input_prepared_requires_new_media',build_id:'16644040',archive_bytes:index.total,extracted,partitions,android_boot_verified:false};await writeFile(join(dir,'userdata-acquisition.json'),JSON.stringify(report,null,2)+'\n');return report;}
  const boot=bootParts(readFileSync(join(dir,'boot.img')));need(sha(boot.kernel.data)==='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8','boot kernel differs from the pinned running GKI');
  const manifest={purpose:'android',build_id:'16644040',target:'aosp_cf_arm64_only_phone-userdebug',partitions};await writeFile(join(dir,'partitions.json'),JSON.stringify(manifest,null,2)+'\n');
  const disk=await buildAndroidPartitionDisk(manifest,join(dir,'android-slot-a.raw'),dir);
  const report={status:'genuine_same_build_partitions_and_gpt_created',build_id:'16644040',archive_bytes:index.total,extracted,disk,android_init_verified:false,android_services_verified:false,android_display_verified:false,avb_verified:false};
  await writeFile(join(dir,'acquisition.json'),JSON.stringify(report,null,2)+'\n');return report;
 });
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{need(process.argv.length===3||(process.argv.length===4&&process.argv[2]==='--userdata-only'),'usage: [--userdata-only] NEW-DIRECTORY');console.log(JSON.stringify(await acquireAndroidPartitions(process.argv.at(-1),{userdataOnly:process.argv[2]==='--userdata-only'}),null,2));}catch(e){console.error(e.message);process.exitCode=1;}}
