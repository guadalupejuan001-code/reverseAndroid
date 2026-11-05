import test from 'node:test';
import assert from 'node:assert/strict';
import {fnv1a64} from '../tools/rvg_checksum.mjs';
import {fnv1a64Wasm} from '../tools/rvg_checksum_wasm.mjs';
const exact=(b,n=b.length)=>{let h=0xcbf29ce484222325n;for(let i=0;i<n;++i)h=BigInt.asUintN(64,(h^BigInt(b[i]))*1099511628211n);return h;};
for(const n of [0,1,2,7,8,255,4096,65535,65536,65537,131071,131072,131073]){
 test('exact checksum across fixed scratch boundary '+n,()=>{
  const backing=Buffer.alloc(n+31,0xa5),b=backing.subarray(17,17+n);let seed=0x73e2ab19;
  for(let i=0;i<n;++i){seed=(Math.imul(seed,1664525)+1013904223)>>>0;b[i]=seed>>>24;}
  const before=Buffer.from(backing);
  for(const count of [...new Set([0,n,Math.floor(n/2)])]){
   assert.equal(fnv1a64Wasm(b,count),exact(b,count));assert.equal(fnv1a64Wasm(b,count),fnv1a64(b,count));
  }
  assert.deepEqual(backing,before);
 });
}
test('consecutive independent calls never reuse a prior seed or scratch suffix',()=>{
 for(let i=0;i<100;++i){const b=new Uint8Array((i*71429)%180003);b.fill(i);assert.equal(fnv1a64Wasm(b),fnv1a64(b));}
 assert.equal(fnv1a64Wasm(Buffer.alloc(0)),0xcbf29ce484222325n);
});
test('optional dispatch preserves the original checksum and input',()=>{
 const old=process.env.REVERSE_PC_FAST_FNV,b=Buffer.from('the complete original bytes');
 try{delete process.env.REVERSE_PC_FAST_FNV;const reference=fnv1a64(b);process.env.REVERSE_PC_FAST_FNV='1';assert.equal(fnv1a64(b),reference);assert.equal(b.toString(),'the complete original bytes');}
 finally{if(old===undefined)delete process.env.REVERSE_PC_FAST_FNV;else process.env.REVERSE_PC_FAST_FNV=old;}
});
test('invalid bounds and types fail before scratch access',()=>{
 for(const n of [-1,0.5,4,NaN,Infinity,Number.MAX_SAFE_INTEGER])assert.throws(()=>fnv1a64Wasm(Buffer.alloc(3),n),TypeError);
 for(const b of ['text',[1,2,3],new Uint16Array(3)])assert.throws(()=>fnv1a64Wasm(b),TypeError);
});
