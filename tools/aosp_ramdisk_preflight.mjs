#!/usr/bin/env node
// Bounded host-only Android first-stage ramdisk inspector. This does not run
// Android, mount partitions, or modify the original XI/Ether memory authority.
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const fail=m=>{throw Error('AOSP ramdisk preflight: '+m);};
const need=(condition,m)=>{if(!condition)fail(m);};
const sha=b=>createHash('sha256').update(b).digest('hex');
const MAX_BLOCK=8*1024*1024,MAX_CPIO=96*1024*1024;
const align=n=>(n+3)&~3;

function lz4RawBlock(raw){
  const dest=Buffer.alloc(MAX_BLOCK);
  let ip=0,op=0;
  while(ip<raw.length){
    const token=raw[ip++];
    let literal=token>>>4;
    if(literal===15){let next=255;while(next===255){
      need(ip<raw.length,'truncated LZ4 literal extension');
      next=raw[ip++];literal+=next;
      need(literal<=MAX_BLOCK,'LZ4 literal extent too large');
    }}
    need(literal<=raw.length-ip&&literal<=MAX_BLOCK-op,'LZ4 literals exceed block');
    raw.copy(dest,op,ip,ip+literal);ip+=literal;op+=literal;
    if(ip===raw.length)break;
    need(ip+2<=raw.length,'truncated LZ4 match offset');
    const offset=raw.readUInt16LE(ip);ip+=2;
    need(offset>0&&offset<=op,'invalid LZ4 backward match');
    let match=4+(token&15);
    if((token&15)===15){let next=255;while(next===255){
      need(ip<raw.length,'truncated LZ4 match extension');
      next=raw[ip++];match+=next;
      need(match<=MAX_BLOCK,'LZ4 match too long');
    }}
    need(match<=MAX_BLOCK-op,'LZ4 decompressed block >8 MiB');
    for(let j=0;j<match;j++)dest[op+j]=dest[op+j-offset];
    op+=match;
  }
  return dest.subarray(0,op);
}
export function decompressLegacyLz4(stream){
  need(Buffer.isBuffer(stream)&&stream.length>=8&&stream.length<=64*1024*1024,
    'compressed LZ4 stream outside 64 MiB input');
  need(stream.readUInt32LE(0)===0x184c2102,'expected legacy LZ4 magic');
  const blocks=[];let at=4,total=0,ended=false;
  while(at<stream.length){
    need(at+4<=stream.length,'truncated LZ4 block header');
    const size=stream.readUInt32LE(at);at+=4;
    if(size===0){ended=true;break;}
    need(size>0&&size<=MAX_BLOCK&&at+size<=stream.length,
      'invalid LZ4 block compressed extent');
    const data=lz4RawBlock(stream.subarray(at,at+size));at+=size;
    total+=data.length;
    need(total<=MAX_CPIO,'decompressed initramfs exceeds 96 MiB');
    blocks.push(data);
  }
  if(ended)need(at===stream.length,'unexpected suffix after legacy LZ4 terminator');
  need(blocks.length>0,'no LZ4 compressed blocks found');
  return Buffer.concat(blocks,total);
}
function hexWord(bytes,at){
  need(at+8<=bytes.length,'truncated cpio field');
  const text=bytes.toString('ascii',at,at+8);
  need(/^[0-9a-fA-F]{8}$/.test(text),'invalid newc CPIO integer');
  return Number.parseInt(text,16);
}
export function parseNewcCpio(bytes){
  need(Buffer.isBuffer(bytes)&&bytes.length>110&&bytes.length<=MAX_CPIO,
    'newc CPIO byte limit exceeded');
  const members=[],names=new Map();
  let at=0,ended=false;
  while(at<bytes.length){
    if(bytes.subarray(at).every(b=>b===0))break;
    need(at+110<=bytes.length,'truncated newc CPIO header');
    const magic=bytes.toString('ascii',at,at+6);
    need(magic==='070701'||magic==='070702','unsupported newc CPIO magic');
    const mode=hexWord(bytes,at+14),size=hexWord(bytes,at+54),
      nameSize=hexWord(bytes,at+94);
    need(nameSize>=2&&nameSize<=4096,'invalid newc CPIO name size');
    need(at+110+nameSize<=bytes.length,'truncated cpio name');
    const nameB=bytes.subarray(at+110,at+110+nameSize);
    need(nameB[nameB.length-1]===0,'CPIO name missing NUL');
    const name=nameB.toString('utf8',0,nameB.length-1);
    need(name.length>0&&!name.includes('\0')&&!name.startsWith('/')&&
      !name.split('/').includes('..'),'unsafe CPIO member path');
    const contentAt=align(at+110+nameSize);
    need(size<=MAX_CPIO&&contentAt<=bytes.length&&size<=bytes.length-contentAt,
      'CPIO member data outside archive');
    if(name==='TRAILER!!!'){need(size===0,'nonempty CPIO trailer');ended=true;at=align(contentAt);break;}
    // Android mkbootfs can record the same empty directory more than once.
    // Never permit duplicate regular files, symlinks, or the /init executable.
    if(names.has(name)){
      const oldMode=names.get(name);
      need((oldMode&0xf000)===0x4000&&(mode&0xf000)===0x4000&&size===0,
        'duplicate non-directory CPIO path: '+name);
    }
    names.set(name,mode);
    members.push({path:name,mode,size,
      contents:bytes.subarray(contentAt,contentAt+size)});
    need(members.length<=20000,'too many CPIO files');
    at=align(contentAt+size);
  }
  need(ended,'missing TRAILER!!! terminator');
  need(bytes.subarray(at).every(x=>x===0),'nonzero bytes following CPIO trailer');
  return members;
}
export function inspectRamdisk(lz4) {
  const cpio=decompressLegacyLz4(lz4),members=parseNewcCpio(cpio);
  const normalized=x=>x.replace(/^(\.\/)+/,'');
  const inits=members.filter(m=>normalized(m.path)==='init');
  need(inits.length===1,'expected exactly one /init in generic ramdisk');
  const init=inits[0],data=init.contents;
  const regular=(init.mode&0xf000)===0x8000;
  const elf=data.length>=64&&data.subarray(0,4).equals(Buffer.from('7f454c46','hex')),
    class64=elf&&data[4]===2,little=elf&&data[5]===1,
    arm64=elf&&data.readUInt16LE(18)===183,
    elfType=elf&&data.readUInt16LE(16);
  need(regular&&elf&&class64&&little&&arm64,
    'real first-stage /init is not a regular ELF64 little-endian ARM64 file');
  return {report:{
    status:'genuine_arm64_android_init_found_not_executed',
    compressed_ramdisk_sha256:sha(lz4),compressed_ramdisk_bytes:lz4.length,
    decompressed_newc_sha256:sha(cpio),decompressed_newc_bytes:cpio.length,
    file_count:members.length,init_path:init.path,init_mode:'0'+init.mode.toString(8),
    init_size:data.length,init_sha256:sha(data),init_elf_machine:183,
    init_elf_type:elfType,init_class:'ELF64 little-endian ARM64',
    relevant_files:members.filter(m=>/^(?:\.\/)?(?:init|fstab|first_stage_ramdisk|system\/etc\/init|etc\/init)/.test(m.path))
      .slice(0,65).map(m=>({path:m.path,size:m.size})),
    android_init_executed:false,adb:false,wild_rift:false
  },cpio};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const args=process.argv.slice(2);
    need(args.length===1||(args.length===3&&args[1]==='--out-cpio'),
      'Usage: node tools/aosp_ramdisk_preflight.mjs generic_ramdisk.bin [--out-cpio NEWFILE]');
    const {report,cpio}=inspectRamdisk(readFileSync(args[0]));
    if(args[1]==='--out-cpio')writeFileSync(args[2],cpio,{flag:'wx'});
    console.log(JSON.stringify(report,null,2));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
