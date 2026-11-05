// Explicit Cuttlefish profile selection, matching Google's assembler configuration.
// Source: google/android-cuttlefish c857a805bd20fe32a791eb3552ed0c43d48b6452
// base/cvd/cuttlefish/host/commands/assemble_cvd/bootconfig_args.cpp
// base/cvd/cuttlefish/host/libs/vm_manager/crosvm_manager.cpp (GuestSwiftshader).
// The runtime remains the original XI/Ether guest; no alternate VMM is loaded.
import assert from 'node:assert/strict';import {readFileSync as read,writeFileSync as write,mkdirSync,statSync} from 'node:fs';import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';
import {sha} from './android_c13_avb.mjs';import {auditGkiRvg} from './audit_gki_rvg.mjs';import {makeBootconfigTrailer} from './aosp_cuttlefish_stage.mjs';
export const SOURCE_STATES=['1922206a01c7bf9392b20afccc195ae7a885954fec85ef662c791b9b17cd024c','c74b0d4f1bd874901c6ee9b1464d419fde1ed49af58ed4b13cfc803506e95f1d'];
export const SOURCE_INITRD='c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da';
export const KEYS=Object.freeze({
 'androidboot.vendor.apex.com.android.hardware.gatekeeper':'com.android.hardware.gatekeeper.nonsecure',
 'androidboot.vendor.apex.com.android.hardware.keymint':'com.android.hardware.keymint.rust_nonsecure',
 'androidboot.vendor.apex.com.android.hardware.secure_element':'com.android.hardware.secure_element',
 'androidboot.vendor.apex.com.android.hardware.strongbox':'none',
 'androidboot.vendor.apex.com.android.hardware.weaver':'none',
 'androidboot.vendor.apex.com.android.hardware.graphics.composer':'com.android.hardware.graphics.composer.drm_hwcomposer',
 'androidboot.vendor.apex.com.google.emulated.camera.provider.hal':'com.google.emulated.camera.provider.hal',
 'androidboot.vendor.apex.com.android.wifi.hal':'com.google.cf.wifi',
 'androidboot.cpuvulkan.version':'4206592',
 'androidboot.hardware.gralloc':'minigbm',
 'androidboot.hardware.hwcomposer':'drm',
 'androidboot.hardware.hwcomposer.display_finder_mode':'drm',
 'androidboot.hardware.hwcomposer.display_framebuffer_format':'bgra',
 'androidboot.hardware.egl':'angle',
 'androidboot.hardware.vulkan':'pastel',
 'androidboot.opengles.version':'196609'
});
export function deriveBootconfig(source,expectedSha){
 assert.ok(Buffer.isBuffer(source)&&source.length>=20&&source.length<=64*1024*1024);assert.equal(sha(source),expectedSha,'Wrong source initrd');assert.equal(source.subarray(-12).toString(),'#BOOTCONFIG\n');
 const n=source.readUInt32LE(source.length-20),checksum=source.readUInt32LE(source.length-16);assert.ok(n>0&&n<=32767&&n<=source.length-20,'Bootconfig extent');const before=source.subarray(0,source.length-20-n),padded=source.subarray(source.length-20-n,source.length-20);assert.equal(padded.reduce((a,b)=>(a+b)>>>0,0),checksum,'Bootconfig checksum');
 let old=padded;while(old.length&&old.at(-1)===0)old=old.subarray(0,-1);assert.ok(old.every(b=>b===9||b===10||(b>=32&&b<=126)),'Bootconfig ASCII');const text=old.toString();assert.match(text,/(?:^|\n)androidboot\.hardware=cutf_cvm(?:\n|$)/);
 for(const key of Object.keys(KEYS))assert.ok(!text.split('\n').some(line=>line.trim().startsWith(key+'=')||line.trim().startsWith(key+' =')),'Profile key already set: '+key);
 const additions=Object.entries(KEYS).map(([k,v])=>`${k}=${v}`).join('\n')+'\n',config=Buffer.from(text+(text.endsWith('\n')?'':'\n')+additions),stamped=makeBootconfigTrailer(before,config);
 const newN=stamped.result.readUInt32LE(stamped.result.length-20),after=stamped.result.subarray(0,stamped.result.length-20-newN);assert.ok(before.equals(after),'Original CPIO bytes changed');
 return {initrd:stamped.result,report:{source_initrd_sha256:expectedSha,derived_initrd_sha256:sha(stamped.result),cpio_payload_sha256:sha(before),source_cpio_payload_unchanged:true,source_bootconfig_preserved:true,bootconfig_sha256:sha(config),added_bootconfig:KEYS,software_keymint:true,hardware_tee:false,guest_software_graphics:true,avb_disabled:false,kernel_modified:false,requires_fresh_boot:true,android_boot_verified:false}};
}
export function buildProfile(checkpoint,directory){
 assert.ok(statSync(checkpoint).size<256*1024*1024,'Oversized source');const state=read(checkpoint),digest=sha(state);assert.ok(SOURCE_STATES.includes(digest),'Unrecognized original media source');const a=auditGkiRvg(state,'f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8',digest),head=8+(state[3]===51?44:43)*8;
 const kn=Number(state.readBigUInt64LE(head+8)),dt=Number(state.readBigUInt64LE(head+16)),rd=Number(state.readBigUInt64LE(head+24));const source=state.subarray(head+48+kn+dt,head+48+kn+dt+rd),r=deriveBootconfig(source,SOURCE_INITRD);
 const report={schema:'reverse-c21-profile-v1',source_media_checkpoint_sha256:digest,source_immutable_image_sha256:a.immutable_image_sha256,...r.report};mkdirSync(directory);write(join(directory,'initrd-profile.bin'),r.initrd,{flag:'wx'});write(join(directory,'profile.json'),JSON.stringify(report,null,2)+'\n',{flag:'wx'});return report;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{const [checkpoint,dir]=process.argv.slice(2);assert.ok(checkpoint&&dir);console.log(JSON.stringify(buildProfile(checkpoint,dir),null,2));}catch(e){console.error(e.stack);process.exitCode=1;}}
