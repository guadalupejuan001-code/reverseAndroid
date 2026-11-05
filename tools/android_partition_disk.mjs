#!/usr/bin/env node
// Host tooling only: raw/sparse partition inputs -> reproducible GPT disk.
// Never creates missing Android systems, bypasses AVB, or claims Android boot.
import {open,readFile,stat,unlink,link,copyFile} from 'node:fs/promises';
import {constants as fsConstants} from 'node:fs';
import {createReadStream} from 'node:fs';
import {createHash,randomBytes} from 'node:crypto';
import {crc32} from 'node:zlib';
import {resolve,dirname} from 'node:path';
import {pathToFileURL} from 'node:url';
export const REQUIRED=['boot_a','init_boot_a','metadata','userdata','super','vbmeta_a','vbmeta_system_a','vbmeta_system_dlkm_a','vbmeta_vendor_dlkm_a'];
const need=(x,m)=>{if(!x)throw Error('Android GPT: '+m);};
const sha=b=>createHash('sha256').update(b).digest('hex');
const align=(n,a)=>Math.ceil(n/a)*a;
const MAX=2**40,CHUNK=1024*1024,ZERO=Buffer.alloc(CHUNK);
async function hashFile(path){const h=createHash('sha256');for await(const b of createReadStream(path))h.update(b);return h.digest('hex');}
// Host transport: preserve zero holes when the filesystem cannot reflink.
// Content and logical length remain exact; never overwrite an existing candidate.
export async function copySparseImage(sourcePath,targetPath){
 need(resolve(sourcePath)!==resolve(targetPath),'sparse copy paths overlap');
 const source=await open(sourcePath,'r');let out,owned=false;
 try{
  const before=await source.stat();need(before.isFile()&&before.size>0&&before.size<=MAX,'invalid sparse copy source');
  out=await open(targetPath,'wx');owned=true;await out.truncate(before.size);
  const buffer=Buffer.alloc(CHUNK);
  for(let at=0;at<before.size;){
   const n=Math.min(CHUNK,before.size-at);let got=0;
   while(got<n){const r=await source.read(buffer,got,n-got,at+got);need(r.bytesRead>0,'source changed during sparse copy');got+=r.bytesRead;}
   const chunk=buffer.subarray(0,n);
   if(!chunk.equals(ZERO.subarray(0,n))){let wrote=0;while(wrote<n){const r=await out.write(chunk,wrote,n-wrote,at+wrote);need(r.bytesWritten>0,'short sparse disk copy');wrote+=r.bytesWritten;}}
   at+=n;
  }
  const after=await source.stat();need(after.size===before.size&&after.mtimeMs===before.mtimeMs&&after.ino===before.ino,'source changed during sparse copy');
  await out.sync();await out.close();out=null;
 }catch(e){if(out)await out.close();if(owned)await unlink(targetPath).catch(()=>{});throw e;}
 finally{await source.close();}
}
async function cloneOrCopySparse(source,target){
 try{await copyFile(source,target,fsConstants.COPYFILE_FICLONE_FORCE|fsConstants.COPYFILE_EXCL);}
 catch(error){if(!['ENOTSUP','EOPNOTSUPP','ENOSYS','EXDEV','EINVAL'].includes(error.code))throw error;await copySparseImage(source,target);}
}
async function exact(file,at,n){const b=Buffer.alloc(n);const r=await file.read(b,0,n,at);need(r.bytesRead===n,'truncated source image');return b;}
async function inspect(path){
 const f=await open(path,'r');try{
  const s=await f.stat();need(s.isFile()&&s.size>0&&s.size<=MAX,'invalid image size');
  const h=await exact(f,0,Math.min(28,s.size));
  if(h.length<4||h.readUInt32LE(0)!==0xed26ff3a)return {path,size:s.size,sparse:false,chunks:[{kind:0xcac1,at:0,length:s.size}]};
  need(h.length===28&&h.readUInt16LE(4)===1&&h.readUInt16LE(6)===0,'unsupported Android sparse version');
  const head=h.readUInt16LE(8),ch=h.readUInt16LE(10),block=h.readUInt32LE(12),blocks=h.readUInt32LE(16),count=h.readUInt32LE(20);
  need(head>=28&&head<=4096&&ch>=12&&ch<=4096&&block>=4&&block%4===0&&count<=1000000,'invalid sparse header');
  const size=blocks*block;need(size>0&&size<=MAX,'expanded sparse image exceeds disk limit');
  let at=head,total=0;const chunks=[];
  for(let i=0;i<count;i++){
   const c=await exact(f,at,ch),kind=c.readUInt16LE(0),n=c.readUInt32LE(4)*block,bytes=c.readUInt32LE(8);at+=ch;
   need([0xcac1,0xcac2,0xcac3,0xcac4].includes(kind)&&bytes>=ch&&at+bytes-ch<=s.size,'invalid sparse chunk');
   const payload=kind===0xcac1?n:kind===0xcac3?0:4;need(bytes-ch===payload&&(kind!==0xcac4||n===0),'sparse chunk size mismatch');
   chunks.push({kind,at,length:n});total+=n;at+=payload;
  }
  need(at===s.size&&total===size,'sparse image extents mismatch');
  return {path,size,sparse:true,chunks,checksum:h.readUInt32LE(24)};
 }finally{await f.close();}
}
async function copyImage(image,out,start){
 const source=await open(image.path,'r');let offset=0,checksum=0;
 try{for(const c of image.chunks){
  if(c.kind===0xcac4){need((await exact(source,c.at,4)).readUInt32LE(0)===checksum,'sparse CRC32 chunk mismatch');continue;}
  const fill=c.kind===0xcac2?await exact(source,c.at,4):null;
  for(let done=0;done<c.length;){const n=Math.min(CHUNK,c.length-done);let b;
   if(c.kind===0xcac1)b=await exact(source,c.at+done,n);
   else if(fill){b=Buffer.alloc(n);for(let i=0;i<n;i+=4)fill.copy(b,i);}
   else b=ZERO.subarray(0,n);
   checksum=crc32(b,checksum);
   if(c.kind!==0xcac3)need((await out.write(b,0,n,start+offset+done)).bytesWritten===n,'short disk write');
   done+=n;
  }offset+=c.length;
 }need(offset===image.size,'expanded copy length mismatch');if(image.checksum)need(image.checksum===checksum,'sparse image checksum mismatch');
 }finally{await source.close();}
}
function guid(text){const b=Buffer.from(sha(Buffer.from(text)).slice(0,32),'hex');b[7]=(b[7]&15)|0x50;b[8]=(b[8]&63)|0x80;return b;}
function header(lba,other,entriesLba,last,diskGuid,entries){const b=Buffer.alloc(512);b.write('EFI PART');b.writeUInt32LE(0x10000,8);b.writeUInt32LE(92,12);b.writeBigUInt64LE(BigInt(lba),24);b.writeBigUInt64LE(BigInt(other),32);b.writeBigUInt64LE(34n,40);b.writeBigUInt64LE(BigInt(last-33),48);diskGuid.copy(b,56);b.writeBigUInt64LE(BigInt(entriesLba),72);b.writeUInt32LE(128,80);b.writeUInt32LE(128,84);b.writeUInt32LE(crc32(entries),88);b.writeUInt32LE(crc32(b.subarray(0,92)),16);return b;}
export async function buildAndroidPartitionDisk(manifest,target,base=process.cwd()){
 need(manifest&&Array.isArray(manifest.partitions)&&manifest.partitions.length>0&&manifest.partitions.length<=128,'manifest needs 1..128 partitions');
 const fixture=manifest.purpose==='protocol-fixture';need(fixture||manifest.purpose==='android','purpose must be android or protocol-fixture');
 const names=new Set();for(const p of manifest.partitions){need(/^[a-z][a-z0-9_]{0,35}$/.test(p.name)&&!names.has(p.name),'invalid or duplicate partition name');names.add(p.name);}
 const missing=REQUIRED.filter(n=>!names.has(n));need(fixture||missing.length===0,'missing Android partitions: '+missing.join(', '));
 const images=[];let sector=2048;
 for(const p of manifest.partitions){
  need(typeof p.path==='string'&&resolve(base,p.path)!==resolve(target),'input/output paths overlap');const path=resolve(base,p.path),image=await inspect(path),hash=await hashFile(path);
  need(fixture||/^[a-f0-9]{64}$/.test(p.sha256??''),'Android images require expected source SHA-256');if(p.sha256)need(p.sha256===hash,'source SHA-256 mismatch for '+p.name);
  const bytes=p.partition_bytes??align(image.size,512);need(Number.isSafeInteger(bytes)&&bytes>=image.size&&bytes%512===0&&bytes<=MAX,'invalid partition capacity');
  const first=sector,last=first+bytes/512-1;images.push({...image,name:p.name,source_sha256:hash,first,last,partition_bytes:bytes});sector=align(last+1,2048);
 }
 const sectors=align(sector+33,2048);need(sectors*512<=MAX,'GPT disk exceeds 1 TiB');
 const identity=images.map(i=>i.name+':'+i.source_sha256+':'+i.partition_bytes).join('\n'),diskGuid=guid(identity),entries=Buffer.alloc(128*128);
 for(let i=0;i<images.length;i++){const p=images[i],at=i*128;Buffer.from('af3dc60f838472478e793d69d8477de4','hex').copy(entries,at);guid(identity+'\n'+p.name).copy(entries,at+16);entries.writeBigUInt64LE(BigInt(p.first),at+32);entries.writeBigUInt64LE(BigInt(p.last),at+40);Buffer.from(p.name,'utf16le').copy(entries,at+56);}
 const mbr=Buffer.alloc(512);mbr[450]=0xee;mbr.writeUInt32LE(1,454);mbr.writeUInt32LE(Math.min(sectors-1,0xffffffff),458);mbr[510]=0x55;mbr[511]=0xaa;
 const temporary=resolve(target)+'.partial-'+randomBytes(8).toString('hex');let f;
 try{f=await open(temporary,'wx');await f.truncate(sectors*512);await f.write(mbr,0,512,0);await f.write(header(1,sectors-1,2,sectors-1,diskGuid,entries),0,512,512);await f.write(entries,0,entries.length,1024);
  for(const image of images)await copyImage(image,f,image.first*512);
  await f.write(entries,0,entries.length,(sectors-33)*512);await f.write(header(sectors-1,1,sectors-33,sectors-1,diskGuid,entries),0,512,(sectors-1)*512);await f.sync();await f.close();f=null;
  const digest=await hashFile(temporary);await link(temporary,resolve(target));await unlink(temporary);
  return {status:fixture?'gpt_protocol_fixture_created':'android_partition_disk_created_avb_not_verified',disk_bytes:sectors*512,disk_sha256:digest,gpt_primary_and_backup:true,source_payloads_preserved:true,missing_android_partitions:missing,fixture,android_boot_verified:false,avb_verified:false,partitions:images.map(i=>({name:i.name,first_lba:i.first,last_lba:i.last,source_bytes:i.size,source_sha256:i.source_sha256,source_sparse:i.sparse}))};
 }catch(e){if(f)await f.close();await unlink(temporary).catch(()=>{});throw e;}
}
// New media only: preserve the pinned original partition extents, append /data,
// and relocate GPT backup sectors. Never attach this different disk to an old RVG.
export async function extendAndroidUserdataDisk(original,userdata,target,{expectedOriginalSha256,expectedOriginalBytes,expectedUserdataSha256,fixture=false}={}){
 need(resolve(original)!==resolve(target)&&resolve(userdata)!==resolve(target),'input/output paths overlap');
 const sourceSize=(await stat(original)).size,sourceSha=await hashFile(original);
 const pinned='28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c';
 need(sourceSize===(expectedOriginalBytes??8738832384)&&sourceSha===(expectedOriginalSha256??pinned),'original disk identity mismatch');
 need(fixture||(sourceSize===8738832384&&sourceSha===pinned),'production source must be original C03 media');
 need(/^[a-f0-9]{64}$/.test(expectedUserdataSha256??''),'userdata requires a source SHA-256');
 const dataImage=await inspect(userdata),dataSha=await hashFile(userdata);need(dataSha===expectedUserdataSha256,'userdata SHA-256 mismatch');
 need(fixture||(dataImage.size>=512*1024*1024&&dataImage.size<=8*1024**3),'userdata must be 512 MiB..8 GiB');
 const source=await open(original,'r');let primary,entries,old=[];
 try{
  primary=await exact(source,512,512);need(primary.toString('ascii',0,8)==='EFI PART'&&primary.readUInt32LE(12)===92&&primary.readUInt32LE(80)===128&&primary.readUInt32LE(84)===128,'unsupported source GPT');
  const checksum=Buffer.from(primary.subarray(0,92)),wanted=checksum.readUInt32LE(16);checksum.writeUInt32LE(0,16);need(crc32(checksum)===wanted,'source GPT header CRC32 mismatch');
  need(primary.readBigUInt64LE(24)===1n&&primary.readBigUInt64LE(32)===BigInt(sourceSize/512-1),'source GPT extent mismatch');
  entries=await exact(source,Number(primary.readBigUInt64LE(72))*512,16384);need(crc32(entries)===primary.readUInt32LE(88),'source GPT table CRC32 mismatch');
  const backup=await exact(source,sourceSize-512,512),backupEntries=await exact(source,sourceSize-33*512,16384);
  const bc=Buffer.from(backup.subarray(0,92)),bw=bc.readUInt32LE(16);bc.writeUInt32LE(0,16);need(backup.toString('ascii',0,8)==='EFI PART'&&crc32(bc)===bw&&backupEntries.equals(entries),'source backup GPT differs');
  const names=new Set();for(let i=0;i<128;i++){
   const at=i*128;if(entries.subarray(at,at+16).every(x=>x===0))continue;
   const name=entries.subarray(at+56,at+128).toString('utf16le').split('\0')[0],first=Number(entries.readBigUInt64LE(at+32)),last=Number(entries.readBigUInt64LE(at+40));
   need(/^[a-z][a-z0-9_]{0,35}$/.test(name)&&!names.has(name)&&first>=34&&last>=first&&last<sourceSize/512-33,'invalid source GPT partition');names.add(name);old.push({name,first,last,index:i});
  }
  need(!names.has('userdata')&&!names.has('misc'),'source already contains writable partitions');
 }finally{await source.close();}
 const first=align(sourceSize/512,2048),dataBytes=align(dataImage.size,512),miscFirst=align(first+dataBytes/512,2048),miscBytes=1024*1024;
 const sectors=align(miscFirst+miscBytes/512+33,2048);need(sectors*512<=MAX,'derived disk exceeds limit');
 const identity=sourceSha+'\n'+dataSha+'\n'+dataBytes;
 const additions=[{name:'userdata',first,last:first+dataBytes/512-1},{name:'misc',first:miscFirst,last:miscFirst+miscBytes/512-1}];
 const free=[];for(let i=0;i<128;i++)if(entries.subarray(i*128,i*128+16).every(x=>x===0))free.push(i);need(free.length>=2,'source GPT has no free entries');
 for(let i=0;i<2;i++){const p=additions[i],at=free[i]*128;Buffer.from('af3dc60f838472478e793d69d8477de4','hex').copy(entries,at);guid(identity+'\n'+p.name).copy(entries,at+16);entries.writeBigUInt64LE(BigInt(p.first),at+32);entries.writeBigUInt64LE(BigInt(p.last),at+40);Buffer.from(p.name,'utf16le').copy(entries,at+56);}
 const temporary=resolve(target)+'.partial-'+randomBytes(8).toString('hex');let out;
 try{
  await cloneOrCopySparse(original,temporary);out=await open(temporary,'r+');await out.truncate(sectors*512);
  await out.write(Buffer.alloc(33*512),0,33*512,sourceSize-33*512);
  const mbr=Buffer.alloc(512);mbr[450]=0xee;mbr.writeUInt32LE(1,454);mbr.writeUInt32LE(Math.min(sectors-1,0xffffffff),458);mbr[510]=0x55;mbr[511]=0xaa;await out.write(mbr,0,512,0);
  const diskGuid=guid(identity);await out.write(header(1,sectors-1,2,sectors-1,diskGuid,entries),0,512,512);await out.write(entries,0,entries.length,1024);
  await copyImage(dataImage,out,first*512);await out.write(entries,0,entries.length,(sectors-33)*512);await out.write(header(sectors-1,1,sectors-33,sectors-1,diskGuid,entries),0,512,(sectors-1)*512);await out.sync();await out.close();out=null;
  const hashRange=async(path,p)=>{const h=createHash('sha256');for await(const b of createReadStream(path,{start:p.first*512,end:(p.last+1)*512-1}))h.update(b);return h.digest('hex');};
  const preserved=[];for(const p of old){const a=await hashRange(original,p),b=await hashRange(temporary,p);need(a===b,'original partition payload changed: '+p.name);preserved.push({...p,sha256:a});}
  const diskSha=await hashFile(temporary);await link(temporary,resolve(target));await unlink(temporary);
  return {schema:'reverse-userdata-media-v1',status:fixture?'protocol_fixture_only':'new_media_requires_fresh_boot',original_disk_sha256:sourceSha,original_disk_bytes:sourceSize,userdata_source_sha256:dataSha,userdata_expanded_bytes:dataBytes,disk_sha256:diskSha,disk_bytes:sectors*512,preserved_partitions:preserved,added_partitions:additions,source_payloads_preserved:true,old_snapshot_resume_allowed:false,android_boot_verified:false,avb_verified:false};
 }catch(e){if(out)await out.close();await unlink(temporary).catch(()=>{});throw e;}
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{const [input,output]=process.argv.slice(2);need(input&&output,'usage: manifest.json new-disk.raw');console.log(JSON.stringify(await buildAndroidPartitionDisk(JSON.parse(await readFile(input,'utf8')),resolve(output),dirname(resolve(input))),null,2));}catch(e){console.error(e.message);process.exitCode=1;}}
