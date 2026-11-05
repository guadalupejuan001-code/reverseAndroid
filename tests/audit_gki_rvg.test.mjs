import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {auditGkiRvg} from '../tools/audit_gki_rvg.mjs';

const sha = bytes => createHash('sha256').update(bytes).digest('hex');
function setFnv(bytes) {
  const mask=0xffffffffffffffffn;
  let h=14695981039346656037n;
  for(let i=0;i<bytes.length-8;i++)
    h=((h^BigInt(bytes[i]))*1099511628211n)&mask;
  bytes.writeBigUInt64LE(h,bytes.length-8);
}
function fixture(bootargs=null) {
  // Synthetic *metadata* only; not an Android, Linux or CPU boot.
  const kernel=Buffer.alloc(64);kernel.writeUInt32LE(0x644d5241,56);
  const args=bootargs===null?Buffer.alloc(0):Buffer.from(bootargs,'ascii');
  const header=bootargs===null?32:40;
  const image=Buffer.alloc(header+kernel.length+args.length);
  image.set([82,86,73,bootargs===null?49:50,13,10,26,10]);
  image.writeBigUInt64LE(BigInt(kernel.length),8);
  if(bootargs!==null)image.writeBigUInt64LE(BigInt(args.length),32);
  kernel.copy(image,header);args.copy(image,header+kernel.length);
  const bytes=Buffer.alloc(8+43*8+image.length+1+8);
  bytes.set([82,86,71,50,13,10,26,10]);
  bytes.writeBigUInt64LE(134217728n,8); // 128 MiB logical RAM
  bytes.writeBigUInt64LE(0n,16); // adaptive InverseMemory
  bytes.writeBigUInt64LE(64n,8+4*8); // instructions
  bytes.writeBigUInt64LE(BigInt(image.length),8+39*8);
  bytes.writeBigUInt64LE(1n,8+42*8); // synthetic state size
  image.copy(bytes,8+43*8);
  setFnv(bytes);
  return {bytes,kernel};
}
test('read-only GKI RVG2/RVI1 kernel and checkpoint provenance',()=>{
  const {bytes,kernel}=fixture();
  const result=auditGkiRvg(bytes,sha(kernel),sha(bytes));
  assert.equal(result.status,'read_only_provenance_verified');
  assert.equal(result.instructions,64);
  assert.equal(result.kernel_bytes,64);
  assert.equal(result.classical_override_limit,'0');
  assert.equal(result.android_init_observed,false);
});
test('reject unexpected kernel identity or wrong checkpoint',()=>{
  const {bytes,kernel}=fixture();
  assert.throws(()=>auditGkiRvg(bytes,'0'.repeat(64),sha(bytes)),
    /kernel SHA256 pin mismatch/);
  assert.throws(()=>auditGkiRvg(bytes,sha(kernel),'0'.repeat(64)),
    /checkpoint SHA256 pin mismatch/);
});
test('reject modified RVG body even with a new external SHA',()=>{
  const {bytes,kernel}=fixture();bytes[bytes.length-9]^=1;
  assert.throws(()=>auditGkiRvg(bytes,sha(kernel),sha(bytes)),
    /RVG internal checksum mismatch/);
});
test('reject forged RVI1 extent with correctly recomputed RVG checksum',()=>{
  const {bytes,kernel}=fixture();
  bytes.writeBigUInt64LE(999n,8+43*8+8);
  setFnv(bytes);
  assert.throws(()=>auditGkiRvg(bytes,sha(kernel),sha(bytes)),
    /RVI1\/RVI2 section extents invalid/);
});
test('reject invalid magic and excessive snapshot',()=>{
  const {bytes,kernel}=fixture();bytes[0]=0;
  assert.throws(()=>auditGkiRvg(bytes,sha(kernel),sha(bytes)),
    /RVG2\/RVG3/);
  assert.throws(()=>auditGkiRvg(Buffer.alloc(256*1024*1024+1)),
    /snapshot size invalid/);
});

test('read-only RVI2 Android bootargs stored after real kernel bytes',()=>{
  const args='console=ttyAMA0 earlycon=pl011,0x09000000 init=/init bootconfig';
  const {bytes,kernel}=fixture(args);
  const result=auditGkiRvg(bytes,sha(kernel),sha(bytes));
  assert.equal(result.rvi_version,'RVI2');
  assert.equal(result.bootargs,args);
  assert.equal(result.bootargs_bytes,args.length);
  assert.equal(result.bootargs_sha256,sha(Buffer.from(args)));
  assert.equal(result.android_init_observed,false);
});
test('read-only RVI2 rejects forged profile length and control bytes with valid RVG checksum',()=>{
  const {bytes,kernel}=fixture('init=/init bootconfig');
  const bad=Buffer.from(bytes);
  bad.writeBigUInt64LE(2048n,8+43*8+32);
  setFnv(bad);
  assert.throws(()=>auditGkiRvg(bad,sha(kernel),sha(bad)),/RVI payload limits/);
  const changed=Buffer.from(bytes);
  changed[8+43*8+40+kernel.length]=10;
  setFnv(changed);
  assert.throws(()=>auditGkiRvg(changed,sha(kernel),sha(changed)),/not printable/);
});

test('halt flag and exit status retain their distinct serialized positions',()=>{
 for(const [halted,exit] of [[1n,37n],[0n,9n]]){
  const {bytes,kernel}=fixture();bytes.writeBigUInt64LE(halted,8+6*8);bytes.writeBigUInt64LE(exit,8+7*8);setFnv(bytes);
  const result=auditGkiRvg(bytes,sha(kernel),sha(bytes));assert.equal(result.native_halted,Number(halted));assert.equal(result.native_exit_code,Number(exit));
 }
});

test('explicit audit bounds are finite, reject oversized data, and preserve exact-bound inputs',()=>{
 const {bytes,kernel}=fixture();const digest=sha(bytes);
 assert.equal(auditGkiRvg(bytes,sha(kernel),digest,{maxSnapshotBytes:bytes.length}).checkpoint_sha256,digest);
 assert.throws(()=>auditGkiRvg(bytes,sha(kernel),digest,{maxSnapshotBytes:bytes.length-1}),/snapshot size invalid/);
 for(const maxSnapshotBytes of [0,1.5,2*1024*1024*1024+1,Infinity])assert.throws(()=>auditGkiRvg(bytes,undefined,undefined,{maxSnapshotBytes}),/invalid explicit transport audit bound/);
});
