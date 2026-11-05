#!/usr/bin/env node
// One bounded AOSP Cuttlefish bootloader PREPARATION stage, not an Android boot.
// Reuses the existing v4 boot parser; XI/Ether remains the only guest RAM authority.
import {inspect,bootParts} from './aosp_boot_preflight.mjs';
import {stat,readFile,mkdir,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';

const sha256=b=>createHash('sha256').update(b).digest('hex');
const requireTrue=(ok,message)=>{if(!ok)throw Error('AOSP stage: '+message);};
const MAGIC=Buffer.from('#BOOTCONFIG\n','ascii');
const LZ4_LEGACY=0x184c2102;
const MAX_IMAGE=96*1024*1024;
const MAX_INITRD=64*1024*1024;
const MAX_CONTAINER=128*1024*1024;
// Android bootloader: PLATFORM vendor ramdisk first, generic last, no page gap.
// Bootconfig is then applied to the END of initrd per Linux bootconfig ABI.
export function makeBootconfigTrailer(preConfig,bootconfig){
  requireTrue(Buffer.isBuffer(preConfig)&&Buffer.isBuffer(bootconfig),
    'bootconfig inputs must be Buffers');
  requireTrue(bootconfig.length>0&&bootconfig.length<=32767,
    'bootconfig bytes outside Linux XBC limit');
  requireTrue(bootconfig.every(b=>b===9||b===10||(b>=32&&b<=126)),
    'invalid bootconfig ASCII/control bytes');
  const pad=(4-(preConfig.length+bootconfig.length+20)%4)%4;
  const padded=Buffer.concat([bootconfig,Buffer.alloc(pad)]);
  const checksum=padded.reduce((sum,b)=>(sum+b)>>>0,0);
  const trailer=Buffer.alloc(20);
  trailer.writeUInt32LE(padded.length,0);
  trailer.writeUInt32LE(checksum,4);
  MAGIC.copy(trailer,8);
  const result=Buffer.concat([preConfig,padded,trailer]);
  requireTrue(result.length%4===0,'Linux bootconfig alignment failure');
  return {result,checksum,size:padded.length,padding:pad};
}
export function stageCuttlefishBoot(boot,vendor,initBoot){
  for(const [name,b] of [['boot',boot],['vendor_boot',vendor],['init_boot',initBoot]]){
    requireTrue(Buffer.isBuffer(b)&&b.length<=MAX_IMAGE,
      name+' is not a bounded official image buffer');
  }
  const inspected=inspect(boot,vendor,initBoot);
  const v=inspected.manifest.vendor_boot;
  const fragments=v.fragments;
  requireTrue(v.version===4&&inspected.manifest.boot.version===4&&
    inspected.manifest.init_boot?.version===4,'expected all three boot v4 images');
  requireTrue(fragments.length===1&&fragments[0].type===1&&fragments[0].offset===0&&
    fragments[0].size===v.ramdisk.size,
    'normal boot requires exactly one complete PLATFORM vendor fragment');
  requireTrue(v.dtb_fdt_magic,'vendor DTB header is not FDT');
  const {kernel,vendor_ramdisk:vendorRd,generic_ramdisk:genericRd,
    bootconfig,vendor_dtb:dtb}=inspected.segments;
  requireTrue(kernel.length>64&&kernel.length<=64*1024*1024,
    'kernel exceeds original XI Linux image budget');
  requireTrue(vendorRd.length>=8&&genericRd.length>=8&&
    vendorRd.readUInt32LE(0)===LZ4_LEGACY&&genericRd.readUInt32LE(0)===LZ4_LEGACY,
    'vendor and generic ramdisk must both be LZ4 legacy streams');
  requireTrue(bootconfig.length>0,'no official vendor bootconfig');
  const joined=Buffer.concat([vendorRd,genericRd]);
  const stamped=makeBootconfigTrailer(joined,bootconfig);
  const final=stamped.result;
  requireTrue(final.length<=MAX_INITRD&&
    32+kernel.length+1456+final.length<=MAX_CONTAINER,
    'combined boot payload exceeds original XI Linux loader budget');
  const kernelImage=bootParts(boot);
  const vendorCmdline=vendor.subarray(28,28+2048).toString('utf8').split('\0')[0];
  const manifest={
    status:'official_vendor_generic_initrd_staged_not_booted',
    image_build:'16644040',target:'aosp_cf_arm64_only_phone-userdebug',
    source_boot_sha256:inspected.manifest.boot.sha256,
    source_vendor_boot_sha256:v.sha256,
    source_init_boot_sha256:inspected.manifest.init_boot.sha256,
    kernel:{bytes:kernel.length,sha256:sha256(kernel)},
    vendor_ramdisk:{bytes:vendorRd.length,sha256:sha256(vendorRd),
      fragments:fragments.map(({name,type,offset,size})=>({name,type,offset,size}))},
    generic_ramdisk:{bytes:genericRd.length,sha256:sha256(genericRd)},
    vendor_dtb:{bytes:dtb.length,sha256:sha256(dtb),fdt_magic:v.dtb_fdt_magic},
    bootconfig:{raw_bytes:bootconfig.length,raw_sha256:sha256(bootconfig),
      checksum:stamped.checksum,padded_size:stamped.size,
      pad_bytes:stamped.padding,magic:MAGIC.toString('ascii')},
    combined_initrd:{bytes:final.length,sha256:sha256(final),
      order:'vendor PLATFORM LZ4 + generic init_boot LZ4 + vendor bootconfig footer',
      bootconfig_magic_last_bytes:true},
    official_boot_cmdline:kernelImage.cmdline,
    official_vendor_cmdline:vendorCmdline,
    existing_reverse_fdt_cmdline:'console=ttyAMA0 earlycon=pl011,0x09000000 keep_bootcon nokaslr maxcpus=1',
    reverse_bootconfig_option_present:false,
    android_init_executed:false,adb:false,wild_rift:false,
    pending:[
      'REVERSE virtual DTB bootargs lacks bootconfig, init=/init and exact Cuttlefish command line',
      'Cuttlefish 72-byte vendor DTB is not a drop-in replacement for REVERSE virtual board',
      'No compatible virtio block or Android super/system/vendor filesystem device and AVB stack',
      'No direct guest evidence of first-stage /init, Binder or ADB'
    ]
  };
  return {manifest,segments:{kernel,combined_initrd:final,
    raw_vendor_ramdisk:vendorRd,raw_generic_ramdisk:genericRd,
    raw_bootconfig:bootconfig}};
}
async function load(path){
  const s=await stat(path);
  requireTrue(s.size<=MAX_IMAGE,'host source file exceeds 96 MiB: '+path);
  return readFile(path);
}
async function cli(){
  const options=new Map();
  for(let i=2;i<process.argv.length;i+=2){
    requireTrue(i+1<process.argv.length,'missing argument value');
    requireTrue(!options.has(process.argv[i]),'duplicate argument');
    options.set(process.argv[i],process.argv[i+1]);
  }
  for(const key of ['--boot','--vendor-boot','--init-boot'])
    requireTrue(options.has(key),'missing '+key);
  for(const key of options.keys())
    requireTrue(['--boot','--vendor-boot','--init-boot','--out-dir'].includes(key),
      'unknown option '+key);
  const stage=stageCuttlefishBoot(await load(options.get('--boot')),
    await load(options.get('--vendor-boot')),await load(options.get('--init-boot')));
  if(options.has('--out-dir')){
    const dest=resolve(options.get('--out-dir'));await mkdir(dest);
    // Never commit or upload the actual binary: this is a local CI staging path.
    await writeFile(join(dest,'kernel.bin'),stage.segments.kernel,{flag:'wx'});
    await writeFile(join(dest,'combined_initrd.bin'),
      stage.segments.combined_initrd,{flag:'wx'});
    await writeFile(join(dest,'manifest.json'),JSON.stringify(stage.manifest,null,2)+'\n',
      {flag:'wx'});
  }
  console.log(JSON.stringify(stage.manifest));
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href)
  cli().catch(e=>{console.error(e.message);process.exitCode=1;});
