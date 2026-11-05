#!/usr/bin/env node
// AOSP v3/v4 boot-container inspector. No Android execution is performed.
// Layout: AOSP system/tools/mkbootimg/include/bootimg/bootimg.h.
import {stat,readFile,mkdir,open,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';

const sha = data => createHash('sha256').update(data).digest('hex');
const check = (ok,what) => {if(!ok) throw Error(what);};
const word = (data,at) => {check(at+4<=data.length,'truncated header field');return data.readUInt32LE(at);};
const pageEnd = (n,page) => Math.ceil(n/page)*page;
const textField = (b,at,n) => b.subarray(at,at+n).toString('utf8').split('\0')[0];
const blob = (bytes,at,size,name) => {
  check(Number.isSafeInteger(at)&&Number.isSafeInteger(size)&&at>=0&&size>=0,'invalid '+name+' bounds');
  check(size===0||(at<=bytes.length&&size<=bytes.length-at),'truncated '+name);
  const data=bytes.subarray(at,at+size);
  return {offset:at,size,sha256:sha(data),data};
};
const summary = b => {const {data,...info}=b;return info;};
const magic = (b,s) => check(b.subarray(0,s.length).toString('ascii')===s,'invalid magic: '+s);
export function bootParts(b,name='boot'){
  magic(b,'ANDROID!');check(b.length>=4096,'truncated '+name+' boot page');
  const ver=word(b,40),head=word(b,20);
  check(ver===3||ver===4,'unsupported '+name+' boot header version');
  check(head===(ver===3?1580:1584),'invalid '+name+' header size');
  const ks=word(b,8),rs=word(b,12),sig=ver===4?word(b,1580):0;
  const kernel=blob(b,4096,ks,name+'.kernel');
  const ramdiskAt=pageEnd(4096+ks,4096);
  const ramdisk=blob(b,ramdiskAt,rs,name+'.ramdisk');
  const signature=blob(b,pageEnd(ramdiskAt+rs,4096),sig,name+'.signature');
  const arm64=kernel.data.length>=60&&kernel.data.subarray(56,60).equals(Buffer.from([65,82,77,100]));
  return {ver,head,hash:sha(b),kernel,ramdisk,signature,arm64,cmdline:textField(b,44,1536)};
}
export function vendorParts(b){
  magic(b,'VNDRBOOT');check(b.length>=2112,'truncated vendor_boot header');
  const ver=word(b,8),pg=word(b,12),head=word(b,2096);
  check(ver===3||ver===4,'unsupported vendor_boot version');
  check(b.length>=(ver===3?2112:2128),'truncated vendor_boot v4 header');
  check(pg>=2048&&pg<=65536&&(pg&(pg-1))===0,'invalid vendor page size');
  check(head===(ver===3?2112:2128),'invalid vendor header size');
  const rs=word(b,24),ds=word(b,2100);
  const ramdisk=blob(b,pageEnd(head,pg),rs,'vendor ramdisk');
  const dtb=blob(b,pageEnd(ramdisk.offset+rs,pg),ds,'vendor DTB');
  const tableAt=pageEnd(dtb.offset+ds,pg);
  let table=blob(b,tableAt,0,'vendor ramdisk table'),config=blob(b,tableAt,0,'vendor bootconfig');
  const entries=[];
  if(ver===4){
    const ts=word(b,2112),count=word(b,2116),entrySize=word(b,2120),cs=word(b,2124);
    check(count===0||(entrySize>=108&&count<=Math.floor(ts/entrySize)),'invalid vendor ramdisk table shape');
    table=blob(b,tableAt,ts,'vendor ramdisk table');
    config=blob(b,pageEnd(tableAt+ts,pg),cs,'vendor bootconfig');
    for(let i=0;i<count;++i){
      const at=i*entrySize,size=word(table.data,at),offset=word(table.data,at+4);
      check(offset<=rs&&size<=rs-offset,'vendor fragment outside ramdisk section');
      entries.push({size,offset,type:word(table.data,at+8),name:textField(table.data,at+12,32),
        sha256:sha(ramdisk.data.subarray(offset,offset+size))});
    }
  }
  return {ver,head,pg,hash:sha(b),board:textField(b,2080,16),
    kernelAddr:word(b,16),ramdiskAddr:word(b,20),dtbAddr:b.readBigUInt64LE(2104).toString(),
    ramdisk,dtb,table,config,entries,dtbFdt:dtb.data.length>=4&&dtb.data.readUInt32BE(0)===0xd00dfeed};
}
export function inspect(boot,vendor,init=null){
  const b=bootParts(boot),v=vendorParts(vendor),i=init===null?null:bootParts(init,'init_boot');
  check(b.ver===v.ver,'boot and vendor_boot versions mismatch');
  check(b.kernel.size>0,'missing kernel in boot.img');
  if(i){
    check(b.ver===4&&i.ver===4,'init_boot requires v4');
    check(i.kernel.size===0&&i.ramdisk.size>0,'init_boot must contain ramdisk without kernel');
    check(b.ramdisk.size===0,'duplicate generic ramdisk in boot and init_boot');
  }
  const generic=i?i.ramdisk:b.ramdisk;
  const gates=[];
  if(!b.arm64)gates.push('kernel not an uncompressed AArch64 Image');
  if(!generic.size)gates.push('missing generic ramdisk or matching init_boot.img');
  if(!v.dtbFdt)gates.push('vendor DTB does not start with FDT magic');
  if(v.entries.length>1)gates.push('vendor ramdisk fragment selection requires board-specific policy');
  gates.push('unverified AOSP kernel/device/ramdisk/bootconfig compatibility and Android services');
  const bootInfo = x => x?{version:x.ver,sha256:x.hash,header_size:x.head,kernel:summary(x.kernel),
    generic_ramdisk:summary(x.ramdisk),signature:summary(x.signature),arm64_image_magic:x.arm64}:null;
  return {manifest:{status:'layout_only',android_boot:false,adb:false,wild_rift:false,
      boot:bootInfo(b),init_boot:bootInfo(i),generic_ramdisk_from:i?'init_boot':'boot',
      vendor_boot:{version:v.ver,sha256:v.hash,header_size:v.head,page_size:v.pg,board:v.board,
        kernel_address:v.kernelAddr,ramdisk_address:v.ramdiskAddr,dtb_address:v.dtbAddr,
        ramdisk:summary(v.ramdisk),dtb:summary(v.dtb),dtb_fdt_magic:v.dtbFdt,
        ramdisk_table:summary(v.table),bootconfig:summary(v.config),fragments:v.entries},
      pending:gates},
    segments:{kernel:b.kernel.data,generic_ramdisk:generic.data,vendor_ramdisk:v.ramdisk.data,
      vendor_dtb:v.dtb.data,bootconfig:v.config.data}};
}
// Independent GKI kernel-image gate. This does NOT assemble an Android machine.
export function inspectGkiKernelOnly(boot) {
  const b=bootParts(boot);
  check(b.kernel.size>0,'GKI boot.img is missing its kernel');
  check(b.arm64,'GKI kernel must be an uncompressed AArch64 Linux Image');
  const k=b.kernel.data;
  const flags=k.readBigUInt64LE(24);
  // Exact ARM64 Linux Image header fields; fail on big-endian or unsupported granules.
  check((flags&1n)===0n&&((flags>>1n)&3n)<=1n,
    'GKI kernel Image flags are not supported by the current XI/Ether loader');
  // Mirror the *Linux-only* limits in MinimalGuest. CI also checks both
  // constants against src/minimal_guest.hpp; ELF remains capped at 16 MiB.
  const kernelBudget=64*1024*1024,totalBudget=128*1024*1024;
  const certainSizeBlock=b.kernel.size>kernelBudget;
  return {manifest:{
    status:'gki_kernel_only',android_boot:false,android_init_observed:false,adb:false,wild_rift:false,
    container_sha256:b.hash,container_header_version:b.ver,
    kernel:summary(b.kernel),arm64_image_magic:true,
    linux_image_header:{text_offset:'0x'+k.readBigUInt64LE(8).toString(16),
      image_size:k.readBigUInt64LE(16).toString(),flags:'0x'+flags.toString(16)},
    existing_guest_image_budget_bytes:totalBudget,
    guest_linux_kernel_budget_bytes:kernelBudget,
    guest_loader_certain_size_block:certainSizeBlock,
    pending:['matching vendor_boot, init_boot or boot generic ramdisk, and Android system/vendor partitions',
      'virtual-board compatibility, bootconfig, device support, Android /init and services not verified',
      ...(certainSizeBlock?['kernel exceeds current 64 MiB Linux kernel host limit']:[])]
  },segments:{kernel:k}};
}
async function load(path){
  const st=await stat(path);
  check(st.size<=512*1024*1024,'host-only 512 MiB input limit exceeded');
  return readFile(path);
}
async function cli(){
  const args=new Map();
  for(let n=2;n<process.argv.length;){
    const key=process.argv[n++];
    check(key.startsWith('--')&&!args.has(key),'invalid or duplicate flag');
    if(key==='--gki-kernel-only'){args.set(key,true);continue;}
    check(n<process.argv.length,'expected value for '+key);
    args.set(key,process.argv[n++]);
  }
  for(const key of args.keys())check(['--boot','--vendor-boot','--init-boot','--extract-dir','--gki-kernel-only'].includes(key),'unknown flag');
  check(args.has('--boot'),'required: --boot');
  const gkiOnly=args.has('--gki-kernel-only');
  if(gkiOnly)check(!args.has('--vendor-boot')&&!args.has('--init-boot'),'kernel-only forbids vendor_boot/init_boot');
  else check(args.has('--vendor-boot'),'full inspection requires --vendor-boot');
  const b=await load(args.get('--boot'));
  const result=gkiOnly?inspectGkiKernelOnly(b):inspect(b,await load(args.get('--vendor-boot')),
    args.has('--init-boot')?await load(args.get('--init-boot')):null);
  if(args.has('--extract-dir')){
    const dest=resolve(args.get('--extract-dir'));
    await mkdir(dest); // never overwrite an existing directory
    for(const [name,bytes] of Object.entries(result.segments)){
      if(!bytes.length)continue;
      const file=await open(join(dest,name+'.bin'),'wx');
      try{await file.writeFile(bytes);}finally{await file.close();}
    }
    await writeFile(join(dest,'manifest.json'),JSON.stringify(result.manifest,null,2)+'\n',{flag:'wx'});
  }
  console.log(JSON.stringify(result.manifest));
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href)
  cli().catch(e=>{console.error('AOSP preflight: '+e.message);process.exitCode=1;});
