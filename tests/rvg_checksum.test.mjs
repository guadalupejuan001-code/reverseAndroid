import test from 'node:test';
import assert from 'node:assert/strict';
import {fnv1a64} from '../tools/rvg_checksum.mjs';
const reference=(b,n=b.length)=>{let x=14695981039346656037n;for(let i=0;i<n;i++)x=BigInt.asUintN(64,(x^BigInt(b[i]))*1099511628211n);return x;};
for(const [text,expected] of [['','cbf29ce484222325'],['a','af63dc4c8601ec8c'],['hello','a430d84680aabd0b'],['foobar','85944171f73967e8']])
  test('FNV vector '+JSON.stringify(text),()=>assert.equal(fnv1a64(Buffer.from(text)).toString(16),expected));
for(const size of [1,2,3,7,8,15,31,64,255,1024,4096,65536])test('uint32-limb checksum agrees with exact BigInt at '+size+' bytes',()=>{
  let seed=0x7f4a7c15;const b=Buffer.alloc(size);for(let i=0;i<size;i++){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;b[i]=seed&255;}
  assert.equal(fnv1a64(b),reference(b));
  assert.equal(fnv1a64(b,Math.floor(size/2)),reference(b,Math.floor(size/2)));
});
for(const byte of [0,255,127,128])test('carry behavior for repeated byte '+byte,()=>{const b=Buffer.alloc(8192,byte);assert.equal(fnv1a64(b),reference(b));});
test('only the requested prefix is hashed',()=>assert.equal(fnv1a64(Buffer.from('hello!'),5),fnv1a64(Buffer.from('hello'))));
test('rejects invalid input bounds and types',()=>{for(const n of [-1,1.5,Infinity,NaN,4])assert.throws(()=>fnv1a64(Buffer.alloc(3),n));assert.throws(()=>fnv1a64('text'));});
