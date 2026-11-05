import {test} from 'node:test';
import assert from 'node:assert/strict';
import {makeBootconfigTrailer} from '../tools/aosp_cuttlefish_stage.mjs';
import {deriveLightsProfile} from '../tools/reverseandroid-lights-profile.mjs';
test('new boot profile preserves CPIO and all existing boot configuration',()=>{
  const cpio=Buffer.from('original compressed CPIO payload');
  const config=Buffer.from('androidboot.hardware=cutf_cvm\nandroidboot.hardware.egl=angle\n');
  const source=makeBootconfigTrailer(cpio,config).result,{initrd,report}=deriveLightsProfile(source);
  assert.ok(initrd.subarray(0,cpio.length).equals(cpio));assert.equal(report.source_cpio_payload_unchanged,true);
  assert.equal(report.live_android_properties_written,false);assert.equal(report.requires_hardware_power_cycle,true);
  assert.ok(initrd.includes(Buffer.from(config+'androidboot.vsock_lights_port=6900\n')));
  assert.throws(()=>deriveLightsProfile(initrd),/already configured/);
});
test('rejects a damaged bootconfig checksum',()=>{
  const source=makeBootconfigTrailer(Buffer.from('cpio'),Buffer.from('androidboot.hardware=cutf_cvm\n')).result;
  source[source.length-16]^=1;assert.throws(()=>deriveLightsProfile(source));
});
