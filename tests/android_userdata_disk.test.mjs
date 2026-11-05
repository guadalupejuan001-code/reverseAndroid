import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile,rm,stat} from 'node:fs/promises';
import {tmpdir} from 'node:os';import {join} from 'node:path';import {createHash} from 'node:crypto';import {crc32} from 'node:zlib';
import {buildAndroidPartitionDisk,extendAndroidUserdataDisk,REQUIRED} from '../tools/android_partition_disk.mjs';
const sha=b=>createHash('sha256').update(b).digest('hex');
const temporary=async f=>{const d=await mkdtemp(join(tmpdir(),'reverse-userdata-'));try{await f(d);}finally{await rm(d,{recursive:true,force:true});}};
function gpt(disk){
 const h=disk.subarray(512,1024),backup=disk.subarray(disk.length-512);for(const x of [h,backup]){const c=Buffer.from(x.subarray(0,92)),want=c.readUInt32LE(16);c.writeUInt32LE(0,16);assert.equal(crc32(c),want);assert.equal(x.toString('ascii',0,8),'EFI PART');}
 const entries=disk.subarray(1024,1024+16384);assert.deepEqual(entries,disk.subarray(disk.length-33*512,disk.length-512));assert.equal(crc32(entries),h.readUInt32LE(88));assert.equal(crc32(entries),backup.readUInt32LE(88));
 const parts=new Map();for(let i=0;i<128;i++){const p=entries.subarray(i*128,(i+1)*128);if(p.subarray(0,16).every(x=>x===0))continue;parts.set(p.subarray(56).toString('utf16le').split('\0')[0],{entry:p,first:Number(p.readBigUInt64LE(32))*512,end:(Number(p.readBigUInt64LE(40))+1)*512});}return parts;
}
test('old eight-partition Android inventory cannot omit the fstab-required userdata',()=>temporary(async d=>{
 const payload=Buffer.alloc(512,3);await writeFile(join(d,'input'),payload);
 await assert.rejects(buildAndroidPartitionDisk({purpose:'android',partitions:REQUIRED.filter(n=>n!=='userdata').map(name=>({name,path:'input',sha256:sha(payload)}))},join(d,'wrong'),d),/missing Android partitions: userdata/);
 await assert.rejects(stat(join(d,'wrong')),/ENOENT/);
}));
test('derived media preserves all original partition entries and bytes, adds data/misc and validates independent GPT CRCs',()=>temporary(async d=>{
 const payload=Buffer.alloc(4096,0x9c);await writeFile(join(d,'input'),payload);await buildAndroidPartitionDisk({purpose:'protocol-fixture',partitions:[{name:'metadata',path:'input'},{name:'super',path:'input'}]},join(d,'original'),d);
 const before=await readFile(join(d,'original')),data=Buffer.alloc(8192,0x37);await writeFile(join(d,'userdata'),data);
 const options={expectedOriginalBytes:before.length,expectedOriginalSha256:sha(before),expectedUserdataSha256:sha(data),fixture:true};
 const report=await extendAndroidUserdataDisk(join(d,'original'),join(d,'userdata'),join(d,'derived'),options),disk=await readFile(join(d,'derived')),old=gpt(before),now=gpt(disk);
 assert.deepEqual(await readFile(join(d,'original')),before);assert.equal(report.old_snapshot_resume_allowed,false);assert.equal(report.android_boot_verified,false);assert.equal(report.avb_verified,false);
 assert.deepEqual([...now.keys()],['metadata','super','userdata','misc']);for(const [name,p] of old){assert.deepEqual(now.get(name).entry,p.entry);assert.deepEqual(disk.subarray(p.first,p.end),before.subarray(p.first,p.end));}
 const u=now.get('userdata'),misc=now.get('misc');assert.deepEqual(disk.subarray(u.first,u.end),data);assert.ok(disk.subarray(misc.first,misc.end).every(x=>x===0));assert.equal(sha(disk),report.disk_sha256);
 await assert.rejects(extendAndroidUserdataDisk(join(d,'original'),join(d,'userdata'),join(d,'derived'),options),/EEXIST/);assert.deepEqual(await readFile(join(d,'derived')),disk);
 await assert.rejects(extendAndroidUserdataDisk(join(d,'original'),join(d,'userdata'),join(d,'bad-pin'),{...options,expectedUserdataSha256:'0'.repeat(64)}),/userdata SHA-256 mismatch/);await assert.rejects(stat(join(d,'bad-pin')),/ENOENT/);
 const damaged=Buffer.from(before);damaged[damaged.length-512+16]^=1;await writeFile(join(d,'damaged'),damaged);
 await assert.rejects(extendAndroidUserdataDisk(join(d,'damaged'),join(d,'userdata'),join(d,'bad-gpt'),{...options,expectedOriginalSha256:sha(damaged)}),/backup GPT differs/);await assert.rejects(stat(join(d,'bad-gpt')),/ENOENT/);
}));
