import {test} from 'node:test';
import assert from 'node:assert/strict';
import {deflateRawSync} from 'node:zlib';
import {crc32,parseCentralDirectory,unpackSelectedLocalMember} from '../tools/aosp_cuttlefish_zip_probe.mjs';

function center(name,data,{method=0,crcOverride=null,offset=0,flags=0}={}){
  const nameB=Buffer.from(name),compressed=method===8?deflateRawSync(data):data;
  const cd=Buffer.alloc(46+nameB.length),lh=Buffer.alloc(30);
  cd.writeUInt32LE(0x02014b50,0);
  cd.writeUInt16LE(flags,8);cd.writeUInt16LE(method,10);
  cd.writeUInt32LE(crcOverride??crc32(data),16);
  cd.writeUInt32LE(compressed.length,20);cd.writeUInt32LE(data.length,24);
  cd.writeUInt16LE(nameB.length,28);cd.writeUInt32LE(offset,42);nameB.copy(cd,46);
  lh.writeUInt32LE(0x04034b50,0);lh.writeUInt16LE(method,8);lh.writeUInt16LE(nameB.length,26);
  return {cd,lh,compressed,data,name};
}
test('ZIP CRC32 matches independent standard test vector',()=>{
  assert.equal(crc32(Buffer.from('123456789')),0xcbf43926);
  assert.equal(crc32(Buffer.alloc(0)),0);
});
test('strict central-directory parses exact plain boot and vendor sections',()=>{
  const x=center('boot.img',Buffer.from('ANDROID!'));
  const y=center('vendor_boot.img',Buffer.from('VNDRBOOT'));
  const entries=parseCentralDirectory(Buffer.concat([x.cd,y.cd]),2);
  assert.deepEqual(entries.map(e=>e.name),['boot.img','vendor_boot.img']);
  assert.equal(entries[1].uncompressed,8);
  assert.equal(unpackSelectedLocalMember(x.lh,x.compressed,entries[0]).data.toString(),'ANDROID!');
});
test('deflated member is decompressed without changing original bytes',()=>{
  const x=center('init_boot.img',Buffer.from('RAMDISK'.repeat(100)),{method:8});
  const info=parseCentralDirectory(x.cd,1)[0];
  assert.deepEqual(unpackSelectedLocalMember(x.lh,x.compressed,info).data,x.data);
});
test('fails closed on central truncation, duplicate or path traversal',()=>{
  const x=center('boot.img',Buffer.from('hello'));
  assert.throws(()=>parseCentralDirectory(x.cd.subarray(0,45),1),/central directory/);
  assert.throws(()=>parseCentralDirectory(Buffer.concat([x.cd,x.cd]),2),/duplicate/);
  assert.throws(()=>parseCentralDirectory(center('../init_boot.img',Buffer.from('x')).cd,1),/unsafe/);
});
test('reject ZIP64 field marker and encryption',()=>{
  const a=center('boot.img',Buffer.from('x'));
  const z=Buffer.from(a.cd);z.writeUInt32LE(0xffffffff,20);
  assert.throws(()=>parseCentralDirectory(z,1),/ZIP64/);
  const b=center('boot.img',Buffer.from('x'),{flags:1});
  assert.throws(()=>parseCentralDirectory(b.cd,1),/encrypted/);
});
test('reject corrupted compressed payload and incorrect local header',()=>{
  const a=center('boot.img',Buffer.from('ANDROID!'));
  const e=parseCentralDirectory(a.cd,1)[0];
  const bad=Buffer.from(a.compressed);bad[0]^=1;
  assert.throws(()=>unpackSelectedLocalMember(a.lh,bad,e),/CRC32/);
  const lh=Buffer.from(a.lh);lh.writeUInt16LE(8,8);
  assert.throws(()=>unpackSelectedLocalMember(lh,a.compressed,e),/compression mismatch/);
});
test('reject any boot member exceeding bounded memory and section extents',()=>{
  const a=center('boot.img',Buffer.from('ANDROID!'));
  const bad=Buffer.from(a.cd);bad.writeUInt32LE(96*1024*1024+1,24);
  const e=parseCentralDirectory(bad,1)[0];
  assert.throws(()=>unpackSelectedLocalMember(a.lh,a.compressed,e),/96 MiB/);
  const e2={...parseCentralDirectory(a.cd,1)[0],compressed:0};
  assert.throws(()=>unpackSelectedLocalMember(a.lh,a.compressed,e2),/extent mismatch/);
});
