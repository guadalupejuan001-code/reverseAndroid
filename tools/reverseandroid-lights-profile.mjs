// A real boot configuration change; never a live Android property write.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {makeBootconfigTrailer} from './aosp_cuttlefish_stage.mjs';
export const LIGHTS_PORT=6900;
export const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
export function deriveLightsProfile(source){
  assert.ok(Buffer.isBuffer(source)&&source.length>=20&&source.length<=64*1024*1024);
  assert.equal(source.subarray(-12).toString(),'#BOOTCONFIG\n');
  const n=source.readUInt32LE(source.length-20),sum=source.readUInt32LE(source.length-16);
  assert.ok(n>0&&n<=32767&&n<=source.length-20);
  const cpio=source.subarray(0,source.length-20-n),padded=source.subarray(cpio.length,source.length-20);
  assert.equal(padded.reduce((a,b)=>(a+b)>>>0,0),sum);
  let old=padded;while(old.length&&old.at(-1)===0)old=old.subarray(0,-1);
  assert.ok(old.every(b=>b===9||b===10||(b>=32&&b<=126)));
  const text=old.toString();assert.match(text,/(?:^|\n)androidboot\.hardware=cutf_cvm\n/);
  assert.ok(!/(?:^|\n)\s*androidboot\.vsock_lights_port\s*=/.test(text),'Lights port already configured');
  const addition=`androidboot.vsock_lights_port=${LIGHTS_PORT}\n`;
  const config=Buffer.from(text+(text.endsWith('\n')?'':'\n')+addition);
  const initrd=makeBootconfigTrailer(cpio,config).result;
  assert.ok(cpio.equals(initrd.subarray(0,cpio.length)));
  return {initrd,report:{schema:'reverseandroid-lights-boot-profile-v1',source_initrd_sha256:hash(source),
    derived_initrd_sha256:hash(initrd),cpio_payload_sha256:hash(cpio),source_cpio_payload_unchanged:true,
    source_bootconfig_preserved:true,added_bootconfig:{'androidboot.vsock_lights_port':String(LIGHTS_PORT)},
    requires_hardware_power_cycle:true,live_android_properties_written:false,kernel_modified:false,avb_disabled:false}};
}
