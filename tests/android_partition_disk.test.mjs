import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile,rm,stat} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {crc32} from 'node:zlib';
import {buildAndroidPartitionDisk,REQUIRED} from '../tools/android_partition_disk.mjs';
const temporary=async f=>{const d=await mkdtemp(join(tmpdir(),'reverse-gpt-'));try{await f(d);}finally{await rm(d,{recursive:true,force:true});}};
test('GPT headers, backup, CRC32, names and raw source sectors agree independently',()=>temporary(async d=>{
 const source=Buffer.alloc(4096,0x67);await writeFile(join(d,'input'),source);
 const manifest={purpose:'protocol-fixture',partitions:[{name:'metadata',path:'input'},{name:'super',path:'input'}]};
 const report=await buildAndroidPartitionDisk(manifest,join(d,'a'),d),disk=await readFile(join(d,'a'));
 assert.equal(disk.readUInt16LE(510),0xaa55);assert.equal(disk[450],0xee);
 const verify=at=>{const b=Buffer.from(disk.subarray(at,at+92)),expected=b.readUInt32LE(16);assert.equal(b.toString('ascii',0,8),'EFI PART');b.writeUInt32LE(0,16);assert.equal(crc32(b),expected);};
 verify(512);verify(disk.length-512);
 const entries=disk.subarray(1024,1024+16384);assert.equal(crc32(entries),disk.readUInt32LE(512+88));assert.deepEqual(entries,disk.subarray(disk.length-33*512,disk.length-512));
 assert.equal(entries.subarray(56,56+16).toString('utf16le'),'metadata');assert.equal(entries.subarray(128+56,128+56+10).toString('utf16le'),'super');
 for(const p of report.partitions)assert.deepEqual(disk.subarray(p.first_lba*512,p.first_lba*512+source.length),source);
 await buildAndroidPartitionDisk(manifest,join(d,'b'),d);assert.deepEqual(await readFile(join(d,'b')),disk);assert.equal(report.android_boot_verified,false);
 await assert.rejects(buildAndroidPartitionDisk(manifest,join(d,'a'),d),/EEXIST/);assert.deepEqual(await readFile(join(d,'a')),disk);
}));
test('Android sparse RAW, FILL, DONT_CARE and CRC32 expand without giant buffers',()=>temporary(async d=>{
 const raw=Buffer.alloc(4096,0x17),fill=Buffer.from([0x11,0x22,0x33,0x44]);const expanded=Buffer.concat([raw,Buffer.alloc(4096,fill),Buffer.alloc(4096)]);
 const header=Buffer.alloc(28);header.writeUInt32LE(0xed26ff3a);header.writeUInt16LE(1,4);header.writeUInt16LE(28,8);header.writeUInt16LE(12,10);header.writeUInt32LE(4096,12);header.writeUInt32LE(3,16);header.writeUInt32LE(4,20);header.writeUInt32LE(crc32(expanded),24);
 const chunk=(kind,blocks,data)=>{const b=Buffer.alloc(12);b.writeUInt16LE(kind);b.writeUInt32LE(blocks,4);b.writeUInt32LE(12+data.length,8);return Buffer.concat([b,data]);};const crc=Buffer.alloc(4);crc.writeUInt32LE(crc32(expanded));
 const sparse=Buffer.concat([header,chunk(0xcac1,1,raw),chunk(0xcac2,1,fill),chunk(0xcac3,1,Buffer.alloc(0)),chunk(0xcac4,0,crc)]);await writeFile(join(d,'sparse'),sparse);
 const manifest={purpose:'protocol-fixture',partitions:[{name:'super',path:'sparse'}]};const r=await buildAndroidPartitionDisk(manifest,join(d,'disk'),d);const b=await readFile(join(d,'disk'));assert.equal(r.partitions[0].source_sparse,true);assert.deepEqual(b.subarray(r.partitions[0].first_lba*512,r.partitions[0].first_lba*512+expanded.length),expanded);
 sparse[sparse.length-1]^=1;await writeFile(join(d,'broken'),sparse);await assert.rejects(buildAndroidPartitionDisk({purpose:'protocol-fixture',partitions:[{name:'super',path:'broken'}]},join(d,'bad'),d),/CRC32/);await assert.rejects(stat(join(d,'bad')),/ENOENT/);
}));
test('missing Android images, wrong source pins and duplicates fail without publishing a disk',()=>temporary(async d=>{
 await writeFile(join(d,'input'),Buffer.alloc(512));
 await assert.rejects(buildAndroidPartitionDisk({purpose:'android',partitions:[{name:'super',path:'input'}]},join(d,'none'),d),/missing Android partitions/);
 const partitions=REQUIRED.map(name=>({name,path:'input',sha256:'0'.repeat(64)}));await assert.rejects(buildAndroidPartitionDisk({purpose:'android',partitions},join(d,'wrong'),d),/SHA-256 mismatch/);
 await assert.rejects(buildAndroidPartitionDisk({purpose:'protocol-fixture',partitions:[{name:'super',path:'input'},{name:'super',path:'input'}]},join(d,'duplicate'),d),/duplicate/);
 await assert.rejects(stat(join(d,'none')),/ENOENT/);
}));
