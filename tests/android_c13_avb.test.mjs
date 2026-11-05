import test from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {parseVbmeta,deriveHandoff,readDirectory,sha} from '../tools/android_c13_avb.mjs';
assert.ok(process.env.C13_METADATA,'Set C13_METADATA to the original six-image inspection directory');
const images=await readDirectory(process.env.C13_METADATA);
const order=['vbmeta_a','boot_a','init_boot_a','vbmeta_system_a','vbmeta_system_dlkm_a','vbmeta_vendor_dlkm_a'];
const expected='92f4ce73ee87fb244fee21aee3de3ddef2fd52d7631e12fc850be8348f3c9ed3';
test('complete original signed chain and exact boot arguments',()=>{const r=deriveHandoff(images);assert.equal(r.vbmeta_bytes,22912);assert.equal(r.vbmeta_digest,expected);assert.deepEqual(r.chain.map(x=>x.name),order);assert.ok(r.chain.every(x=>x.signature_verified));assert.equal(r.verification_disabled,false);assert.equal(r.hashtree_disabled,false);assert.equal(r.partition_payloads_verified,false);assert.equal(r.guest_execution_performed,false);assert.equal(r.bootargs.split(' ').length,3);});
test('independent OpenSSL digest over descriptor-order exact image extents',()=>{const input=Buffer.concat(order.map(n=>images.get(n)));const p=spawnSync('openssl',['dgst','-sha256'],{input});assert.equal(p.status,0);assert.ok(p.stdout.toString().includes(expected));assert.equal(input.length,22912);});
test('Map insertion order cannot alter signed descriptor traversal',()=>assert.equal(deriveHandoff(new Map([...images].reverse())).vbmeta_digest,expected));
test('partition padding is not included in the vbmeta digest',()=>{const padded=new Map([...images].map(([k,v])=>[k,Buffer.concat([v,Buffer.alloc(4096)])]));assert.equal(deriveHandoff(padded).vbmeta_digest,expected);assert.notEqual(sha(Buffer.concat([...padded.values()])),expected);});
for(const [name,bytes] of images){
  test(name+' authentic RSA signature',()=>assert.equal(parseVbmeta(bytes).algorithm,2));
  test(name+' omitted is rejected',()=>{const m=new Map(images);m.delete(name);assert.throws(()=>deriveHandoff(m));});
  for(const [label,offset] of [['magic',0],['version',4],['flags',120],['hash',256],['signature',288],['auxiliary',832],['last-signed-byte',bytes.length-1]])test(name+' altered '+label+' rejected',()=>{const b=Buffer.from(bytes);b[offset]^=1;assert.throws(()=>parseVbmeta(b));});
  for(const length of [0,255,bytes.length-1])test(name+' truncated '+length+' rejected',()=>assert.throws(()=>parseVbmeta(bytes.subarray(0,length))));
  test(name+' oversized declared extent rejected',()=>{const b=Buffer.from(bytes);b.writeBigUInt64BE(0xffffffffffffffffn,12);assert.throws(()=>parseVbmeta(b));});
}
test('validly signed but swapped children rejected by exact original pin',()=>{const m=new Map(images);m.set('boot_a',images.get('init_boot_a'));assert.throws(()=>deriveHandoff(m),/exact original/);});
test('extra image rejected',()=>{const m=new Map(images);m.set('unexpected_a',images.get('boot_a'));assert.throws(()=>deriveHandoff(m));});
test('root-only digest is not the handoff digest',()=>assert.notEqual(sha(images.get('vbmeta_a')),expected));
