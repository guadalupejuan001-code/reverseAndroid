import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {memoryElfSymbols} from '../tools/reverseandroid-elf-symbols.mjs';
function fixture(gnu=false) {
  const b=Buffer.alloc(4096),base=0x100000n;
  b.set(Buffer.from('7f454c4602010100','hex'));b.writeUInt16LE(3,16);b.writeUInt16LE(183,18);
  b.writeBigUInt64LE(64n,32);b.writeUInt16LE(56,54);b.writeUInt16LE(2,56);
  b.writeUInt32LE(1,64);b.writeBigUInt64LE(4096n,104);
  b.writeUInt32LE(2,120);b.writeBigUInt64LE(0x200n,136);b.writeBigUInt64LE(112n,160);
  const tags=[[5n,0x400n],[10n,6n],[6n,0x300n],[11n,24n],[gnu?0x6ffffef5n:4n,0x500n],[0n,0n]];
  tags.forEach(([t,v],i)=>{b.writeBigUInt64LE(t,0x200+i*16);b.writeBigUInt64LE(v,0x208+i*16);});
  const sym=0x318;b.writeUInt32LE(1,sym);b[sym+4]=0x12;b.writeUInt16LE(1,sym+6);b.writeBigUInt64LE(0x800n,sym+8);b.writeBigUInt64LE(32n,sym+16);
  b.write('\0work\0',0x400,'ascii');
  b.writeUInt32LE(1,0x500);b.writeUInt32LE(gnu?1:2,0x504);
  if(gnu){b.writeUInt32LE(1,0x508);b.writeBigUInt64LE(1n,0x510);b.writeUInt32LE(1,0x518);b.writeUInt32LE(1,0x51c);}
  const memory={read(a,n){assert.ok(a>=base&&a+BigInt(n)<=base+4096n);return b.subarray(Number(a-base),Number(a-base)+n);}};
  return {b,memory,areas:[{start:base,end:base+4096n,file:1n,filePageOffset:0n,flags:5n}],pathFor:()=>'/fixture.so',file:'/fixture.so'};
}
for(const gnu of [false,true])test((gnu?'GNU':'SysV')+' symbols resolve only an exact declared function range',()=>{
  const f=fixture(gnu),before=createHash('sha256').update(f.b).digest('hex'),s=memoryElfSymbols(f);
  assert.equal(s.symbol_count,2);assert.deepEqual(s.lookup(0x100810n),{name:'work',offset:'0x10',range_verified:true});
  assert.equal(s.lookup(0x100820n),null);assert.equal(s.lookup(0x100700n),null);
  assert.equal(createHash('sha256').update(f.b).digest('hex'),before);
});
test('foreign architecture and oversized hash tables are rejected',()=>{
  const a=fixture();a.b.writeUInt16LE(62,18);assert.throws(()=>memoryElfSymbols(a));
  const b=fixture(true);b.b.writeUInt32LE(0xffffffff,0x500);assert.throws(()=>memoryElfSymbols(b));
});
