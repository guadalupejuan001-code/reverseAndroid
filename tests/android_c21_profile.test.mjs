import test from 'node:test';import assert from 'node:assert/strict';import {deriveBootconfig,KEYS} from '../tools/android_c21_profile.mjs';import {makeBootconfigTrailer} from '../tools/aosp_cuttlefish_stage.mjs';import {sha} from '../tools/android_c13_avb.mjs';
const cpio=Buffer.from('synthetic metadata fixture; not an Android image\n');
const source=(extra='')=>makeBootconfigTrailer(cpio,Buffer.from('androidboot.hardware=cutf_cvm\nkernel.example=unchanged\n'+extra)).result;
test('profile changes only bootconfig and explicitly selects the genuine local HAL variants',()=>{
 const old=source(),copy=Buffer.from(old),r=deriveBootconfig(old,sha(old));assert.deepEqual(old,copy);const n=r.initrd.readUInt32LE(r.initrd.length-20),config=r.initrd.subarray(r.initrd.length-20-n,r.initrd.length-20);assert.deepEqual(r.initrd.subarray(0,r.initrd.length-20-n),cpio);
 assert.match(config.toString(),/kernel.example=unchanged/);for(const [k,v] of Object.entries(KEYS))assert.ok(config.toString().includes(k+'='+v+'\n'));assert.equal(config.reduce((a,b)=>(a+b)>>>0,0),r.initrd.readUInt32LE(r.initrd.length-16));assert.equal(r.initrd.length%4,0);
 assert.equal(r.report.avb_disabled,false);assert.equal(r.report.source_cpio_payload_unchanged,true);assert.equal(r.report.requires_fresh_boot,true);assert.equal(r.report.android_boot_verified,false);assert.equal(r.report.hardware_tee,false);
});
test('identity, checksum, extent and duplicate selections fail closed',()=>{
 const old=source();assert.throws(()=>deriveBootconfig(old,'0'.repeat(64)),/Wrong source initrd/);
 const broken=Buffer.from(old);broken[broken.length-16]^=1;assert.throws(()=>deriveBootconfig(broken,sha(broken)),/Bootconfig checksum/);
 const extent=Buffer.from(old);extent.writeUInt32LE(0x10000000,extent.length-20);assert.throws(()=>deriveBootconfig(extent,sha(extent)),/Bootconfig extent/);
 const conflict=source('androidboot.vendor.apex.com.android.hardware.gatekeeper=com.android.hardware.gatekeeper.cf_remote\n');assert.throws(()=>deriveBootconfig(conflict,sha(conflict)),/already set/);
});
