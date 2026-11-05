#!/usr/bin/env node
// Read-only, bounded selected-member inspector for a SAME-BUILD public AOSP
// Cuttlefish archive. No kernel execution, no partition mounting, no extra RAM
// authority and no signed object-storage URL persisted or printed.
import {readFileSync,writeFileSync,mkdirSync,mkdtempSync,rmSync,statSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {inflateRawSync} from 'node:zlib';
import {createHash} from 'node:crypto';
import {tmpdir} from 'node:os';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';
const error=(m)=>{throw Error('Cuttlefish ZIP preflight: '+m);};
const need=(x,m)=>{if(!x)error(m);};
const sha=b=>createHash('sha256').update(b).digest('hex');
const OFFICIAL={
  build:'16644040',
  target:'aosp_cf_arm64_only_phone-userdebug',
  artifact:'aosp_cf_arm64_only_phone-img-16644040.zip',
  viewer:'https://ci.android.com/builds/submitted/16644040/aosp_cf_arm64_only_phone-userdebug/latest/aosp_cf_arm64_only_phone-img-16644040.zip'
};
const ALLOWED=new Set(['boot.img','init_boot.img','vendor_boot.img']);
const MAX_MEMBER=96*1024*1024, MAX_TOTAL=192*1024*1024;
const crcTable=new Uint32Array(256);
for(let i=0;i<256;i++){let x=i;for(let j=0;j<8;j++)x=(x>>>1)^((x&1)?0xedb88320:0);crcTable[i]=x>>>0;}
export function crc32(buf) {
  let x=0xffffffff;
  for(const b of buf)x=(x>>>8)^crcTable[(x^b)&255];
  return (x^0xffffffff)>>>0;
}
export function parseCentralDirectory(buf,expectedCount) {
  const entries=[];let p=0;const names=new Set();
  while(p<buf.length){
    need(p+46<=buf.length && buf.readUInt32LE(p)===0x02014b50,
      'invalid central directory record');
    const flags=buf.readUInt16LE(p+8),method=buf.readUInt16LE(p+10);
    const crc=buf.readUInt32LE(p+16);
    const compressed=buf.readUInt32LE(p+20),uncompressed=buf.readUInt32LE(p+24);
    const n=buf.readUInt16LE(p+28),ex=buf.readUInt16LE(p+30),cm=buf.readUInt16LE(p+32);
    const offset=buf.readUInt32LE(p+42);
    need(p+46+n+ex+cm<=buf.length&&n>0,'truncated ZIP filename/extra/comment');
    const name=buf.subarray(p+46,p+46+n).toString('utf8');
    need(!name.startsWith('/')&&!name.includes('\\')&&
      !name.split('/').includes('..')&&!name.includes('\0'),
      'unsafe ZIP member path');
    need(!names.has(name),'duplicate ZIP member');
    need(!(flags&1)&&method===0||(!(flags&1)&&method===8),
      'encrypted/unsupported ZIP entry');
    need(compressed!==0xffffffff&&uncompressed!==0xffffffff&&offset!==0xffffffff,
      'ZIP64 entry is unsupported by this bounded archive version');
    names.add(name);
    entries.push({name,flags,method,crc32:crc,compressed,uncompressed,
      localHeaderOffset:offset});
    p+=46+n+ex+cm;
    need(entries.length<=5000,'too many ZIP entries');
  }
  need(p===buf.length && entries.length===expectedCount,'ZIP entry count/extent mismatch');
  return entries;
}
export function unpackSelectedLocalMember(header,content,entry) {
  need(header.length>=30&&header.readUInt32LE(0)===0x04034b50,
    'invalid local ZIP header');
  const method=header.readUInt16LE(8),nameLen=header.readUInt16LE(26),
    extraLen=header.readUInt16LE(28);
  need(method===entry.method,'local/central compression mismatch');
  need(entry.uncompressed<=MAX_MEMBER&&entry.compressed<=MAX_MEMBER,
    'member exceeds 96 MiB bounded input');
  // The caller fetches compressed bytes beginning at
  // localHeaderOffset + 30 + nameLen + extraLen.
  need(header.length===30,'local header extent must be exact');
  need(content.length===entry.compressed,'compressed extent mismatch');
  const data=entry.method===0?content:inflateRawSync(content,{maxOutputLength:MAX_MEMBER+1});
  need(data.length===entry.uncompressed,'uncompressed byte size mismatch');
  need(crc32(data)===entry.crc32,'ZIP CRC32 mismatch');
  return {data,nameLen,extraLen};
}
function curl(url,options,maxOut=8*1024*1024){
  const p=spawnSync('curl',['--silent','--show-error','--fail','--location',
    '--max-time','120',...options,url],{encoding:null,timeout:130000,maxBuffer:maxOut});
  if(p.status!==0)error('official artifact transport failure (curl exit '+p.status+')');
  return p.stdout;
}
function getViewerSignedUrl(){
  const page=curl(OFFICIAL.viewer,[],1024*1024).toString('utf8');
  const m=/var JSVariables = (\{[^\n]+?\});/.exec(page);
  need(m,'missing official Artifact Viewer variables');
  const vars=JSON.parse(m[1]);
  need(vars.buildId===OFFICIAL.build&&vars.target===OFFICIAL.target&&
    vars.artifact===OFFICIAL.artifact,'official artifact identity mismatch');
  const url=new URL(vars.artifactUrl);
  need(url.protocol==='https:'&&url.hostname==='storage.googleapis.com'&&
    url.pathname.startsWith('/android-build/builds/aosp-android-latest-release-linux-'+OFFICIAL.target+'/'+OFFICIAL.build+'/')&&
    url.pathname.endsWith('/'+OFFICIAL.artifact),
    'unexpected storage artifact source');
  return url.href; // Ephemeral only; never print this bearer URL.
}
function range(url,begin,end,maxSize=MAX_MEMBER){
  need(Number.isSafeInteger(begin)&&Number.isSafeInteger(end)&&
    begin>=0&&end>=begin&&end-begin+1<=maxSize,'unbounded range requested');
  const temp=mkdtempSync(join(tmpdir(),'reverse-cf-range-'));
  try{
    const headers=join(temp,'headers'),dest=join(temp,'range');
    const p=spawnSync('curl',['--silent','--show-error','--fail','--location',
      '--max-time','120','--max-filesize',String(maxSize),
      '--range',String(begin)+'-'+String(end),'--dump-header',headers,
      '--output',dest,url],{encoding:'utf8',timeout:130000,maxBuffer:4096});
    if(p.status!==0)error('range HTTP fetch failed (curl exit '+p.status+')');
    const h=readFileSync(headers,'utf8').trim().split(/\r?\n\r?\n/).at(-1)||'';
    const status=Number(/^HTTP\/[\d.]+\s+(\d+)/m.exec(h)?.[1]??0);
    const cr=/^content-range:\s*bytes\s+(\d+)-(\d+)\/(\d+)/im.exec(h);
    need(status===206&&cr&&Number(cr[1])===begin&&Number(cr[2])===end,
      'range request was not honored exactly');
    const bytes=readFileSync(dest);
    need(bytes.length===end-begin+1,'range returned an incorrect number of bytes');
    return {bytes,total:Number(cr[3])};
  }finally{rmSync(temp,{recursive:true,force:true});}
}
function readArchiveIndex(url){
  // Source size anchored in the 206 Content-Range response, never an HTML HEAD.
  const first=range(url,0,127,512);
  need(first.bytes.readUInt32LE(0)===0x04034b50,'not a ZIP local header');
  const total=first.total;
  need(total>=128&&total<2*1024*1024*1024,'unexpected archive byte length');
  const tail=range(url,total-131072,total-1,131072);
  need(tail.total===total,'remote archive changed during inspection');
  let at=-1;
  for(let i=tail.bytes.length-22;i>=0;i--){
    if(tail.bytes.readUInt32LE(i)===0x06054b50 &&
      i+22+tail.bytes.readUInt16LE(i+20)===tail.bytes.length){at=i;break;}
  }
  need(at>=0,'end of ZIP directory missing');
  const disk=tail.bytes.readUInt16LE(at+4),diskCD=tail.bytes.readUInt16LE(at+6),
    countDisk=tail.bytes.readUInt16LE(at+8),count=tail.bytes.readUInt16LE(at+10),
    size=tail.bytes.readUInt32LE(at+12),offset=tail.bytes.readUInt32LE(at+16);
  need(disk===0&&diskCD===0&&count===countDisk,'multi-disk ZIP unsupported');
  need(count>0&&count<5000&&size>0&&size<=8*1024*1024&&
    offset!==0xffffffff&&offset+size<=total-22,
    'invalid or excessive ZIP central directory');
  const cd=range(url,offset,offset+size-1,8*1024*1024);
  need(cd.total===total,'remote archive size changed during inspection');
  const entries=parseCentralDirectory(cd.bytes,count);
  return {total,entries,centralSize:size,centralOffset:offset};
}
function extractOne(url,total,entry){
  const at=entry.localHeaderOffset;
  need(at>=0&&at+30<total,'invalid local header address');
  const hdr=range(url,at,at+29,64);
  need(hdr.total===total,'remote archive changed during extraction');
  const nameLen=hdr.bytes.readUInt16LE(26),extraLen=hdr.bytes.readUInt16LE(28);
  const name=range(url,at+30,at+30+nameLen-1,65536);
  need(name.bytes.toString('utf8')===entry.name,'local/central ZIP member name differs');
  const dataAt=at+30+nameLen+extraLen;
  need(dataAt+entry.compressed<=total,'member data exceeds archive');
  need(entry.compressed>0&&entry.compressed<=MAX_MEMBER,'compressed image over budget');
  const data=range(url,dataAt,dataAt+entry.compressed-1,MAX_MEMBER);
  need(data.total===total,'remote archive changed during member transfer');
  return unpackSelectedLocalMember(hdr.bytes,data.bytes,entry).data;
}
// Share the verified transport with the streaming partition acquisition tool.
// The callback must keep the ephemeral signed URL out of reports and files.
export function withOfficialArchive(action){const url=getViewerSignedUrl();return action({url,index:readArchiveIndex(url),readRange:(a,b,n)=>range(url,a,b,n)});}
export function runOfficialProbe(mode='list',directory=null){
  need(mode==='list'||mode==='extract','invalid mode');
  const signed=getViewerSignedUrl(),index=readArchiveIndex(signed);
  const selected=index.entries.filter(e=>ALLOWED.has(e.name));
  const list=index.entries.map(e=>({name:e.name,method:e.method,compressed:e.compressed,
    uncompressed:e.uncompressed,crc32:e.crc32.toString(16).padStart(8,'0')}));
  const report={
    status:mode==='list'?'official_archive_index_verified':'official_boot_partitions_verified',
    official_branch:'aosp-android-latest-release',target:OFFICIAL.target,
    build_id:OFFICIAL.build,artifact:OFFICIAL.artifact,
    archive_size:index.total,
    central_directory_bytes:index.centralSize,
    archive_members:list,selected_boot_members:selected.map(e=>e.name),
    extracted:{},android_guest_boot:false,android_init_observed:false,adb:false,wild_rift:false
  };
  if(mode==='extract'){
    need(directory,'--out-dir is required');
    for(const name of ALLOWED)need(selected.some(e=>e.name===name),'missing '+name);
    const totalUncompressed=selected.reduce((n,e)=>n+e.uncompressed,0);
    need(totalUncompressed<=MAX_TOTAL,'boot image extraction exceeds 192 MiB');
    const items=[];
    for(const entry of selected){
      const bytes=extractOne(signed,index.total,entry);
      items.push({name:entry.name,bytes,sha256:sha(bytes)});
    }
    const dir=resolve(directory);mkdirSync(dir); // fail if already present
    for(const item of items){
      writeFileSync(join(dir,item.name),item.bytes,{flag:'wx'});
      report.extracted[item.name]={size:item.bytes.length,sha256:item.sha256};
    }
    writeFileSync(join(dir,'manifest.json'),JSON.stringify(report,null,2)+'\n',{flag:'wx'});
  }
  return report;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const args=process.argv.slice(2);
    const mode=args[0]||'list';
    need(mode==='list'||(mode==='extract'&&args[1]==='--out-dir'&&
      args[2]&&args.length===3),'Usage: node tools/aosp_cuttlefish_zip_probe.mjs list | extract --out-dir NEWDIR');
    const report=runOfficialProbe(mode,args[2]);
    console.log(JSON.stringify(report,null,2));
  }catch(e){console.error(e.message);process.exitCode=1;}
}

