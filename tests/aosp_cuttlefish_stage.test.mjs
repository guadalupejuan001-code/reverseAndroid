import {test} from 'node:test';
import assert from 'node:assert/strict';
import {makeBootconfigTrailer,stageCuttlefishBoot} from '../tools/aosp_cuttlefish_stage.mjs';

const align=(v,n)=>Math.ceil(v/n)*n;
const magic=Buffer.from([0x02,0x21,0x4c,0x18,1,0,0,0]);
const kernel=Buffer.alloc(140);kernel.write('ARMd',56);
const generic=Buffer.from(magic);
const vendorRd=Buffer.from(magic);
const config=Buffer.from('androidboot.hardware=cutf_cvm\n');

function boot({k=kernel,r=Buffer.alloc(0)}={}){
  const rOff=align(4096+k.length,4096);
  const b=Buffer.alloc(Math.max(4096,rOff+r.length,4096+k.length));
  b.write('ANDROID!');b.writeUInt32LE(k.length,8);b.writeUInt32LE(r.length,12);
  b.writeUInt32LE(1584,20);b.writeUInt32LE(4,40);
  k.copy(b,4096);r.copy(b,rOff);return b;
}
function vendor({ramdisk=vendorRd,type=1,bootconfig=config,dtbMagic=true}={}){
  const dtb=Buffer.alloc(72);if(dtbMagic)dtb.writeUInt32BE(0xd00dfeed);
  const pg=2048,rdOff=align(2128,pg),dtbOff=align(rdOff+ramdisk.length,pg),
    tabOff=align(dtbOff+dtb.length,pg),cfgOff=align(tabOff+108,pg);
  const data=Buffer.alloc(Math.max(cfgOff+bootconfig.length,tabOff+108));
  data.write('VNDRBOOT');data.writeUInt32LE(4,8);data.writeUInt32LE(pg,12);
  data.writeUInt32LE(ramdisk.length,24);data.writeUInt32LE(2128,2096);
  data.writeUInt32LE(dtb.length,2100);data.writeBigUInt64LE(0x40070000n,2104);
  data.writeUInt32LE(108,2112);data.writeUInt32LE(1,2116);data.writeUInt32LE(108,2120);
  data.writeUInt32LE(bootconfig.length,2124);
  ramdisk.copy(data,rdOff);dtb.copy(data,dtbOff);
  data.writeUInt32LE(ramdisk.length,tabOff);data.writeUInt32LE(0,tabOff+4);
  data.writeUInt32LE(type,tabOff+8);
  bootconfig.copy(data,cfgOff);
  return data;
}
const stage=(v=vendor(),i=boot({k:Buffer.alloc(0),r:generic}))=>
  stageCuttlefishBoot(boot(),v,i);
test('bootconfig footer uses the actual Linux 12-byte magic, LE size and additive checksum',()=>{
  const prefix=Buffer.from('12345'),cfg=Buffer.from('androidboot.hardware=cutf_cvm\n');
  const x=makeBootconfigTrailer(prefix,cfg);
  const f=x.result,off=f.length-20;
  assert.equal(f.subarray(off+8).toString(),'#BOOTCONFIG\n');
  assert.equal(f.readUInt32LE(off),x.size);
  assert.equal(f.readUInt32LE(off+4),x.checksum);
  assert.equal(f.length%4,0);
  assert.equal(x.size,cfg.length+x.padding);
  assert.deepEqual(f.subarray(0,prefix.length),prefix);
});
test('vendor PLATFORM LZ4 is followed without padding by generic LZ4 then bootconfig',()=>{
  const x=stage();
  const rd=x.segments.combined_initrd;
  assert.deepEqual(rd.subarray(0,vendorRd.length),vendorRd);
  assert.deepEqual(rd.subarray(vendorRd.length,vendorRd.length+generic.length),generic);
  assert.deepEqual(rd.subarray(vendorRd.length+generic.length,
    vendorRd.length+generic.length+config.length),config);
  assert.equal(x.manifest.vendor_ramdisk.fragments[0].type,1);
  assert.equal(x.manifest.combined_initrd.bootconfig_magic_last_bytes,true);
  assert.equal(x.manifest.android_init_executed,false);
  assert.equal(x.manifest.reverse_bootconfig_option_present,false);
});
test('never allow RECOVERY vendor fragment in normal boot',()=>{
  assert.throws(()=>stage(vendor({type:2})),/PLATFORM/);
});
test('never mix malformed FDT or unknown ramdisk compression',()=>{
  assert.throws(()=>stage(vendor({dtbMagic:false})),/vendor DTB/);
  assert.throws(()=>stage(vendor({ramdisk:Buffer.from('arbitrary')})),/LZ4/);
  assert.throws(()=>stage(vendor(),boot({k:Buffer.alloc(0),r:Buffer.from('noLZ4xx')})),/LZ4/);
});
test('bootconfig fields are not invented or silently rewritten',()=>{
  assert.throws(()=>makeBootconfigTrailer(Buffer.alloc(0),Buffer.alloc(0)),/outside/);
  assert.throws(()=>makeBootconfigTrailer(Buffer.alloc(0),Buffer.from('bad\rconfig')),/invalid/);
});
test('bootconfig source bytes remain identical, and result is aligned for Linux',()=>{
  const x=stage();const rd=x.segments.combined_initrd,footer=rd.subarray(rd.length-20);
  assert.equal(footer.readUInt32LE(0),config.length+x.manifest.bootconfig.pad_bytes);
  const sum=Array.from(config).reduce((a,b)=>(a+b)>>>0,0);
  assert.equal(footer.readUInt32LE(4),sum);
  assert.equal(rd.length%4,0);
  assert.equal(rd.subarray(-12).toString('ascii'),'#BOOTCONFIG\n');
});
