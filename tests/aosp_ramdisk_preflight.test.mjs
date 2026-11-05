import {test} from 'node:test';
import assert from 'node:assert/strict';
import {decompressLegacyLz4,parseNewcCpio,inspectRamdisk} from '../tools/aosp_ramdisk_preflight.mjs';

const align=n=>(n+3)&~3;
function newc(name,data,mode=0o100755){
  const fields=[1,mode,0,0,1,0,data.length,0,0,0,0,Buffer.byteLength(name)+1,0];
  const header='070701'+fields.map(n=>n.toString(16).padStart(8,'0')).join('');
  assert.equal(header.length,110);
  const prefix=Buffer.from(header+name+'\0'),padding=Buffer.alloc(align(prefix.length)-prefix.length);
  const dataPadding=Buffer.alloc(align(data.length)-data.length);
  return Buffer.concat([prefix,padding,data,dataPadding]);
}
function cpio(files){
  return Buffer.concat([...files.map(x=>newc(x.name,x.data,x.mode)),
    newc('TRAILER!!!',Buffer.alloc(0),0)]);
}
function legacy(data){
  // Hand-encode one LZ4 *literal-only* block; this is a test fixture, not
  // a synthetic Android boot or a replacement for official initramfs.
  const ext=[];let extra=data.length-15;
  assert.ok(extra>=0);
  while(extra>=255){ext.push(255);extra-=255;}ext.push(extra);
  const raw=Buffer.concat([Buffer.from([0xf0,...ext]),data]);
  const header=Buffer.alloc(8);header.writeUInt32LE(0x184c2102,0);
  header.writeUInt32LE(raw.length,4);
  return Buffer.concat([header,raw]);
}
const fakeElf=()=>{
  const b=Buffer.alloc(64);
  b.write('ELF',1);b[0]=0x7f;b[4]=2;b[5]=1;b[6]=1;
  b.writeUInt16LE(3,16);b.writeUInt16LE(183,18);
  return b;
};
test('LZ4 legacy literal-only decompresses real CPIO bytes losslessly',()=>{
  const data=cpio([{name:'init',data:fakeElf()}]);
  const uncompressed=decompressLegacyLz4(legacy(data));
  assert.deepEqual(uncompressed,data);
});
test('LZ4 raw block overlapping backwards matches work',()=>{
  // Three literals abc plus six overlapping match bytes from offset=3.
  const block=Buffer.from([0x32,97,98,99,3,0]);
  const header=Buffer.alloc(8);header.writeUInt32LE(0x184c2102);
  header.writeUInt32LE(block.length,4);
  assert.equal(decompressLegacyLz4(Buffer.concat([header,block])).toString(),'abcabcabc');
});
test('identify genuine-form ELF64 ARM64 /init in bounded newc archive',()=>{
  const image=cpio([{name:'init',data:fakeElf()},
    {name:'first_stage_ramdisk/fstab.cuttlefish',data:Buffer.from('fstab')}]);
  const x=inspectRamdisk(legacy(image)).report;
  assert.equal(x.status,'genuine_arm64_android_init_found_not_executed');
  assert.equal(x.init_size,64);
  assert.equal(x.init_elf_machine,183);
  assert.equal(x.android_init_executed,false);
  assert.equal(x.file_count,2);
});
test('reject non-ARM init, missing /init and duplicate name',()=>{
  const wrong=fakeElf();wrong.writeUInt16LE(62,18);
  assert.throws(()=>inspectRamdisk(legacy(cpio([{name:'init',data:wrong}]))),/ARM64/);
  assert.throws(()=>inspectRamdisk(legacy(cpio([{name:'not_init',data:fakeElf()}]))),/exactly one/);
  assert.throws(()=>parseNewcCpio(cpio([{name:'init',data:fakeElf()},
    {name:'init',data:fakeElf()}])),/duplicate/);
});
test('reject LZ4 malformed backwards pointers, length and magic',()=>{
  const raw=Buffer.from([0x00,0x01,0x00]),hdr=Buffer.alloc(8);
  hdr.writeUInt32LE(0x184c2102);hdr.writeUInt32LE(raw.length,4);
  assert.throws(()=>decompressLegacyLz4(Buffer.concat([hdr,raw])),/backward match/);
  assert.throws(()=>decompressLegacyLz4(Buffer.from('invalid!')),/magic/);
  const data=legacy(cpio([{name:'init',data:fakeElf()}]));
  const broken=Buffer.from(data);broken.writeUInt32LE(0xfffffffe,4);
  assert.throws(()=>decompressLegacyLz4(broken),/extent/);
});
test('reject invalid newc magic, traversal and truncated bodies',()=>{
  const data=cpio([{name:'init',data:fakeElf()}]);
  const corrupt=Buffer.from(data);corrupt[0]=0;
  assert.throws(()=>parseNewcCpio(corrupt),/magic/);
  assert.throws(()=>parseNewcCpio(cpio([{name:'../init',data:fakeElf()}])),/unsafe/);
  assert.throws(()=>parseNewcCpio(data.subarray(0,data.length-60)),/truncated|terminator|extent/);
});

test('repeat empty device directory metadata, but never replace an ELF or symlink',()=>{
  const first={name:'dev',data:Buffer.alloc(0),mode:0o40755};
  const archive=cpio([first,first,{name:'init',data:fakeElf()}]);
  assert.equal(parseNewcCpio(archive).length,3);
  assert.equal(inspectRamdisk(legacy(archive)).report.init_size,64);
  assert.throws(()=>parseNewcCpio(cpio([
    first,{name:'dev',data:Buffer.from('file'),mode:0o100644},
    {name:'init',data:fakeElf()}])),/duplicate non-directory/);
});
