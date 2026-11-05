#!/usr/bin/env node
// Host-only normal-boot image builder. Never changes kernel, /init or modules.
// The source stock/recovery-capable initrd remains a separate preserved input.
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {decompressLegacyLz4,parseNewcCpio} from './aosp_ramdisk_preflight.mjs';
import {makeBootconfigTrailer} from './aosp_cuttlefish_stage.mjs';
const SOURCE='21f6a9e2498f6ea30c4a4edffcd3efbaea3feec79524004492041650a1d4fb64';
const INIT='d5c901a96c6e0bad8e3ee8110dc638c9336067fdafd4378860eaead51e4299ba';
const sha=b=>createHash('sha256').update(b).digest('hex');
const need=(ok,m)=>{if(!ok)throw Error('Normal Android ramdisk: '+m);};
const align=n=>(n+3)&~3;
function records(cpio){
 const members=parseNewcCpio(cpio);let at=0;
 const result=members.map(m=>{
  const names=Number.parseInt(cpio.toString('ascii',at+94,at+102),16);
  const end=align(align(at+110+names)+m.size);
  const out={...m,record:cpio.subarray(at,end)};at=end;return out;
 });
 const names=Number.parseInt(cpio.toString('ascii',at+94,at+102),16);
 return {members:result,trailer:cpio.subarray(at,align(at+110+names))};
}
export function buildNormalAndroidRamdisk(source){
 need(Buffer.isBuffer(source)&&sha(source)===SOURCE,'unexpected immutable source initrd');
 const vendor=decompressLegacyLz4(source.subarray(0,18816074));
 const generic=decompressLegacyLz4(source.subarray(18816074,18816074+2992408));
 const v=records(vendor),g=records(generic);
 const keep=p=>p==='lib'||p==='lib/modules'||p.startsWith('lib/modules/')||
  p==='first_stage_ramdisk'||p.startsWith('first_stage_ramdisk/')||
  ['adb_keys','default.prop','prop.default'].includes(p);
 const selected=v.members.filter(m=>keep(m.path));
 const names=new Map(selected.map(m=>[m.path,m]));
 const load=names.get('lib/modules/modules.load');
 need(load,'missing genuine module load list');
 const requested=load.contents.toString().split(/\r?\n/).map(x=>x.trim()).filter(x=>x&&!x.startsWith('#'));
 for(const name of requested)need(names.has('lib/modules/'+name),'dropped requested module '+name);
 const fstab=names.get('first_stage_ramdisk/system/etc/fstab.cf.ext4.cts');
 need(fstab,'missing selected normal-boot fstab');
 const fstabText=fstab.contents.toString();
 need(fstabText.includes('slotselect'),'unexpected non-slotted fstab');
 const init=g.members.find(m=>m.path==='init');
 need(init&&(init.mode&0xf000)===0x8000&&sha(init.contents)===INIT,'changed genuine Android /init');
 const elf=init.contents,entry=elf.readBigUInt64LE(24),ph=Number(elf.readBigUInt64LE(32)),stride=elf.readUInt16LE(54),number=elf.readUInt16LE(56);
 let entryOffset=null;
 for(let i=0;i<number;i++){
  const p=ph+i*stride;need(p+56<=elf.length,'ELF program header outside /init');
  if(elf.readUInt32LE(p)!==1)continue;
  const va=elf.readBigUInt64LE(p+16),size=elf.readBigUInt64LE(p+32);
  if(entry>=va&&entry+64n<=va+size)entryOffset=Number(elf.readBigUInt64LE(p+8)+entry-va);
 }
 need(entryOffset!==null&&entryOffset+64<=elf.length,'ELF entry code window is not file backed');
 const rawVendor=Buffer.concat([...selected.map(m=>m.record),v.trailer]);
 const paddedVendor=Buffer.concat([rawVendor,Buffer.alloc((512-rawVendor.length%512)%512)]);
 // Linux initramfs accepts consecutive uncompressed newc archives. Generic
 // CPIO bytes are verbatim; all selected vendor records retain their bytes.
 const pre=Buffer.concat([paddedVendor,generic]);
 need(source.subarray(source.length-12).toString()==='#BOOTCONFIG\n','missing bootconfig footer');
 const n=source.readUInt32LE(source.length-20);
 let config=source.subarray(source.length-20-n,source.length-20);
 while(config.length&&config.at(-1)===0)config=config.subarray(0,-1);
 const stamped=makeBootconfigTrailer(pre,config);
 const actual=records(paddedVendor);
 need(actual.members.length===selected.length,'repacked vendor count mismatch');
 for(let i=0;i<selected.length;i++)need(actual.members[i].path===selected[i].path&&actual.members[i].mode===selected[i].mode&&
  actual.members[i].contents.equals(selected[i].contents),'changed selected vendor record');
 return {initrd:stamped.result,report:{
  status:'normal_boot_image_prepared_not_executed',source_initrd_sha256:SOURCE,
  kernel_policy:'kernel unchanged; force normal boot and packaged fstab selected in separate RVI2 bootargs',
  format:'filtered original vendor newc + verbatim generic newc + Linux bootconfig',
  source_vendor_cpio_bytes:vendor.length,source_generic_cpio_bytes:generic.length,
  output_vendor_cpio_bytes:paddedVendor.length,output_initrd_bytes:stamped.result.length,
  output_initrd_sha256:sha(stamped.result),generic_cpio_sha256:sha(generic),
  original_init_sha256:INIT,init_entry:'0x2a4000',
  selected_fstab_sha256:sha(fstab.contents),selected_fstab_requires_slot_suffix:true,
  init_entry_file_offset:entryOffset,init_entry_bytes_hex:elf.subarray(entryOffset,entryOffset+64).toString('hex'),
  bootconfig_sha256:sha(config),bootconfig_text:config.toString(),
  kept_vendor_files:selected.map(m=>({path:m.path,mode:m.mode,size:m.size,sha256:sha(m.contents)})),
  omitted_recovery_members:v.members.length-selected.length,modules_required:requested,
  android_init_executed:false,android_ui_observed:false
 }};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 try{const [input,output]=process.argv.slice(2);need(input&&output&&resolve(input)!==resolve(output),'usage: source-initrd output-new-initrd');
  const result=buildNormalAndroidRamdisk(readFileSync(input));writeFileSync(output,result.initrd,{flag:'wx'});console.log(JSON.stringify(result.report,null,2));
 }catch(e){console.error(e.message);process.exitCode=1;}
}
