#!/usr/bin/env node
// Read-only independent RVG2/RVG3 -> RVI1/RVI2 -> ARM64 provenance audit.
// Execution and restoration remain exclusively in XI/Ether QuantumRuntime.
import {readFileSync, statSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {fnv1a64} from './rvg_checksum.mjs';

const sha256 = buf => createHash('sha256').update(buf).digest('hex');
const check = (ok, what) => {if(!ok) throw Error('GKI checkpoint audit: '+what);};
const safe = (n, label) => {
  check(n<=BigInt(Number.MAX_SAFE_INTEGER),label+' exceeds exact numeric range');
  return Number(n);
};
export function auditGkiRvg(bytes,expectedKernelSha,expectedRvgSha,{maxSnapshotBytes=256*1024*1024}={}) {
  check(Number.isSafeInteger(maxSnapshotBytes)&&maxSnapshotBytes>=360&&maxSnapshotBytes<=2*1024*1024*1024,'invalid explicit transport audit bound');
  check(Buffer.isBuffer(bytes) && bytes.length<=maxSnapshotBytes &&
    bytes.length>=8+43*8+8,'snapshot size invalid');
  const version=bytes.toString('ascii',0,4);
  check((version==='RVG2'||version==='RVG3') &&
    bytes.subarray(4,8).equals(Buffer.from([13,10,26,10])),
    'snapshot is not RVG2/RVG3');
  const fields=version==='RVG3'?44:43;
  const offset=8+fields*8;
  const read = i => safe(bytes.readBigUInt64LE(8+i*8),'RVG field '+i);
  const imageBytes=read(39),coreBytes=read(40),memoryBytes=read(41);
  const systemBytes=read(42),fpBytes=version==='RVG3'?read(43):0;
  check(systemBytes>0&&systemBytes<=262144&&fpBytes<=532,
    'system/fp section invalid');
  check(imageBytes>=32&&imageBytes<=128*1024*1024,
    'RVI image bytes exceed 128 MiB');
  check(BigInt(offset)+BigInt(imageBytes)+BigInt(coreBytes)+
    BigInt(memoryBytes)+BigInt(systemBytes)+BigInt(fpBytes)+8n===
    BigInt(bytes.length),'RVG section extents invalid');
  const image=bytes.subarray(offset,offset+imageBytes);
  check(image.subarray(0,3).equals(Buffer.from('RVI')) &&
    (image[3]===49||image[3]===50||image[3]===51) &&
    image.subarray(4,8).equals(Buffer.from([13,10,26,10])),
    'RVI1/RVI2 header invalid');
  const pci=image[3]===51,extended=image[3]!==49;
  const header=pci?48:extended?40:32;
  check(image.length>=header,'RVI2 extended header truncated');
  const kernelBytes=safe(image.readBigUInt64LE(8),'kernel bytes');
  const treeBytes=safe(image.readBigUInt64LE(16),'FDT bytes');
  const initrdBytes=safe(image.readBigUInt64LE(24),'initrd bytes');
  const bootargsBytes=extended?safe(image.readBigUInt64LE(32),'bootargs bytes'):0;
  const deviceBytes=pci?safe(image.readBigUInt64LE(40),'device bytes'):0;
  check(kernelBytes>=64&&kernelBytes<=64*1024*1024&&
    treeBytes<=65536&&initrdBytes<=64*1024*1024&&
    (!extended||((pci||bootargsBytes>0)&&bootargsBytes<=1024))&&deviceBytes===(pci?80:0),
    'RVI payload limits exceeded');
  check(header+kernelBytes+treeBytes+initrdBytes+bootargsBytes+deviceBytes===imageBytes,
    'RVI1/RVI2 section extents invalid');
  const kernel=image.subarray(header,header+kernelBytes);
  const args=image.subarray(header+kernelBytes+treeBytes+initrdBytes,header+kernelBytes+treeBytes+initrdBytes+bootargsBytes);
  check(!extended||args.every(b=>b>=32&&b<=126),
    'RVI2 bootargs not printable ASCII');
  const bootargs=extended?args.toString('ascii'):null;
  check(kernel.readUInt32LE(56)===0x644d5241,
    'missing ARM64 Linux Image magic');
  const kernelSha=sha256(kernel),rvgSha=sha256(bytes);
  if(expectedKernelSha)
    check(kernelSha===expectedKernelSha.toLowerCase(),
      'kernel SHA256 pin mismatch');
  if(expectedRvgSha)
    check(rvgSha===expectedRvgSha.toLowerCase(),
      'checkpoint SHA256 pin mismatch');
  // Additional independent internal FNV-1a validation. XI/Ether also
  // validates this on its own restore path; the audit never restores state.
  const checksum=fnv1a64(bytes,bytes.length-8);
  check(checksum===bytes.readBigUInt64LE(bytes.length-8),
    'RVG internal checksum mismatch');
  return {
    status:'read_only_provenance_verified',
    kernel_sha256:kernelSha,checkpoint_sha256:rvgSha,
    rvg_version:version,checkpoint_bytes:bytes.length,
    rvi_version:pci?'RVI3':extended?'RVI2':'RVI1',
    external_disk_bytes:pci?safe(image.readBigUInt64LE(imageBytes-deviceBytes+8),'disk bytes'):0,
    external_disk_sha256:pci?image.subarray(imageBytes-64).toString('ascii'):null,
    bootargs_sha256:extended?sha256(args):null,
    bootargs_bytes:bootargsBytes,bootargs,
    instructions:read(4),kernel_bytes:kernelBytes,
    dtb_bytes:treeBytes,initrd_bytes:initrdBytes,
    immutable_image_bytes:imageBytes,immutable_image_sha256:sha256(image),
    logical_ram_bytes:read(0),native_halted:read(6),native_exit_code:read(7),
    native_pc:'0x'+bytes.readBigUInt64LE(24).toString(16),
    native_sp:'0x'+bytes.readBigUInt64LE(32).toString(16),
    classical_override_limit:bytes.readBigUInt64LE(16).toString(),
    android_init_observed:false,adb:false,wild_rift:false
  };
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href) {
  try {
    const [path,kernelSha,checkpointSha]=process.argv.slice(2);
    check(path&&/^[0-9a-f]{64}$/i.test(kernelSha||'') &&
      /^[0-9a-f]{64}$/i.test(checkpointSha||''),
      'Usage: node audit_gki_rvg.mjs checkpoint.rvg expected_kernel_sha256 expected_checkpoint_sha256');
    check(statSync(path).size<256*1024*1024,'file too large');
    console.log(JSON.stringify(
      auditGkiRvg(readFileSync(path),kernelSha,checkpointSha)));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
