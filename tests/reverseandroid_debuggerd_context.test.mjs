import test from 'node:test';
import assert from 'node:assert/strict';
import {readDebuggerdSignalCandidate} from '../tools/reverseandroid-debuggerd-context.mjs';

function fixture() {
  const bytes=Buffer.alloc(0x10000),base=0x1000;
  bytes.writeInt32LE(7354,base);bytes.writeInt32LE(7816,base+4);
  bytes.writeBigUInt64LE(0x2000n,base+8);bytes.writeBigUInt64LE(0x3000n,base+16);
  bytes.writeInt32LE(11,0x2000);bytes.writeInt32LE(1,0x2008);
  bytes.writeBigUInt64LE(0xdeadn,0x3000+176);
  bytes.writeBigUInt64LE(0x7000n,0x3000+432);bytes.writeBigUInt64LE(0x8000n,0x3000+440);
  const mapping=address=>address===0x8000n?{address:'0x8000',flags:'0x75'}:address===0x7000n?{address:'0x7000',flags:'0x73'}:null;
  const memory={read(address,length){assert.ok(address>=0n&&address+BigInt(length)<=BigInt(bytes.length),'Unmapped candidate memory');return bytes.subarray(Number(address),Number(address)+length);}};
  return {bytes,options:{memory,futexAddress:0x1004n,leaderPid:7354,observedChildTid:7816,mapping}};
}

test('decodes bounded AArch64 candidate bytes without claiming a version-locked interpretation',()=>{
  const {bytes,options}=fixture(),before=Buffer.from(bytes),result=readDebuggerdSignalCandidate(options);
  assert.equal(result.signal_number,11);assert.equal(result.signal_code,1);
  assert.equal(result.saved_fault_address,'0xdead');assert.equal(result.saved_PC.address,'0x8000');
  assert.equal(result.layout_source_version_locked,false);assert.equal(result.interpretation_requires_guest_build_and_signal_validation,true);
  assert.deepEqual(bytes,before);
});

test('rejects unrelated futexes, unreadable pointers, bad signal numbers and privileged contexts',()=>{
  const corruptions=[
    bytes=>bytes.writeInt32LE(999,0x1000),
    bytes=>bytes.writeInt32LE(999,0x1004),
    bytes=>bytes.writeBigUInt64LE(0xffff000000000000n,0x1008),
    bytes=>bytes.writeBigUInt64LE(0x10000n,0x1010),
    bytes=>bytes.writeInt32LE(0,0x2000),
    bytes=>bytes.writeBigUInt64LE(5n,0x3000+448),
    bytes=>bytes.writeBigUInt64LE(0x9000n,0x3000+440),
    bytes=>bytes.writeBigUInt64LE(0x9000n,0x3000+432),
  ];
  for(const corrupt of corruptions){const {bytes,options}=fixture();corrupt(bytes);assert.throws(()=>readDebuggerdSignalCandidate(options));}
});
