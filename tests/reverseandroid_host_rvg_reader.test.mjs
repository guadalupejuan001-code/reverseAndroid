import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {createHash,Hash,Hmac} from 'node:crypto';
import {readBoundedRvg,HOST_RVG_CAP,installLargeRvgReader,installLargeRvgHashing} from '../tools/reverseandroid-host-rvg-reader.mjs';

const originalHashUpdate=Hash.prototype.update;
const originalHmacUpdate=Hmac.prototype.update;

test('bounded positional chunks preserve every byte including zeroes and chunk boundaries',()=>{
  const dir=mkdtempSync(join(tmpdir(),'reverseandroid-rvg-'));
  try{
    const path=join(dir,'current.rvg'),data=Buffer.alloc(16391);
    for(let i=0;i<data.length;i++)data[i]=(i*139^(i>>>3))&255;
    writeFileSync(path,data);const before=createHash('sha256').update(readFileSync(path)).digest('hex');
    for(const chunkBytes of [1,7,4096,8192])assert.deepEqual(readBoundedRvg(path,{chunkBytes}),data);
    assert.equal(createHash('sha256').update(readFileSync(path)).digest('hex'),before);
    assert.throws(()=>readBoundedRvg(path,{maxBytes:data.length-1}),/explicit host file bound/);
    assert.throws(()=>readBoundedRvg(path,{maxBytes:HOST_RVG_CAP+1}),/finite host RVG bound/);
    assert.throws(()=>readBoundedRvg(path,{chunkBytes:67108865}),/bounded host read chunk/);
  }finally{rmSync(dir,{recursive:true,force:true});}
});

test('explicit preload retains ordinary file and text reads and is idempotent',()=>{
  const dir=mkdtempSync(join(tmpdir(),'reverseandroid-rvg-'));
  try{
    const path=join(dir,'small.rvg');writeFileSync(path,'RVG fixture\n');
    installLargeRvgReader();installLargeRvgReader();
    assert.equal(readFileSync(path,'utf8'),'RVG fixture\n');assert.deepEqual(readFileSync(path),Buffer.from('RVG fixture\n'));
  }finally{rmSync(dir,{recursive:true,force:true});}
});

test('host hashing preserves ordinary inputs, encoding, chaining, copies and native errors',()=>{
  installLargeRvgHashing();const installed=Hash.prototype.update;
  installLargeRvgHashing();assert.equal(Hash.prototype.update,installed);
  assert.equal(Hmac.prototype.update,originalHmacUpdate);
  for(const [data,encoding] of [
    ['café','utf8'],['636166c3a9','hex'],['Y2Fmw6k=','base64'],
    [Buffer.from([0,1,128,255]),undefined],
    [new Uint16Array([0,256,65535]),undefined],
    [new DataView(Uint8Array.from([99,0,128,255,88]).buffer,1,3),'utf8'],
  ]){
    const expected=originalHashUpdate.call(createHash('sha256'),data,encoding).digest('hex');
    const actual=createHash('sha256');assert.equal(actual.update(data,encoding),actual);
    assert.equal(actual.digest('hex'),expected);
  }
  const hash=createHash('sha256').update('prefix');const fork=hash.copy();
  assert.equal(hash.update('left').digest('hex'),originalHashUpdate.call(createHash('sha256'),'prefixleft').digest('hex'));
  assert.equal(fork.update('right').digest('hex'),originalHashUpdate.call(createHash('sha256'),'prefixright').digest('hex'));
  assert.throws(()=>hash.update('already finalized'),{code:'ERR_CRYPTO_HASH_FINALIZED'});
  assert.throws(()=>createHash('sha256').update({}),{code:'ERR_INVALID_ARG_TYPE'});
  assert.throws(()=>createHash('sha256').update('abc','hex'),{code:'ERR_INVALID_ARG_VALUE'});
});

test('actual input above 2 GiB hashes identically through bounded Buffer and DataView updates',{
  timeout:120000,
},()=>{
  const length=0x80000000+257,offset=13;
  // Zero-backed pages avoid committing a second large resident allocation.
  // Sentinels cover both 64 MiB chunk edges, INT_MAX, view offsets and the tail.
  const storage=Buffer.alloc(length+offset+19);
  const data=storage.subarray(offset,offset+length);
  storage[0]=0xa5;storage[storage.length-1]=0x5a;
  const sentinels=new Map([
    [0,0x81],[1,0x42],[64*1024*1024-1,0x23],[64*1024*1024,0x64],
    [64*1024*1024+1,0x95],[0x7ffffffe,0xa6],[0x7fffffff,0x17],
    [0x80000000,0x38],[length-1,0xf9],
  ]);
  for(const [at,value] of sentinels)data[at]=value;
  // Exercise the unmodified method too: this is the real Node 24 blocker,
  // independently of whether another test has installed the host wrapper.
  assert.throws(()=>originalHashUpdate.call(createHash('sha256'),data),{code:'ERR_OUT_OF_RANGE'});

  // Independent expected stream is built from an 8 MiB scratch buffer; it
  // neither hashes slices of the large input nor uses the new 64 MiB wrapper.
  const expected=createHash('sha256'),scratch=Buffer.alloc(8*1024*1024);
  for(let at=0;at<length;at+=scratch.length){
    scratch.fill(0);const count=Math.min(scratch.length,length-at);
    for(const [position,value] of sentinels){
      if(position>=at&&position<at+count)scratch[position-at]=value;
    }
    originalHashUpdate.call(expected,scratch.subarray(0,count));
  }
  const expectedPlain=expected.copy().digest('hex');
  const expectedSuffix=originalHashUpdate.call(expected,'suffix','utf8').digest('hex');
  installLargeRvgHashing();installLargeRvgHashing();
  const actual=createHash('sha256');assert.equal(actual.update(data),actual);
  const fork=actual.copy();
  assert.equal(actual.digest('hex'),expectedPlain);
  assert.equal(fork.update('suffix','utf8').digest('hex'),expectedSuffix);
  const view=new DataView(storage.buffer,storage.byteOffset+offset,length);
  const viewHash=createHash('sha256');assert.equal(viewHash.update(view),viewHash);
  assert.equal(viewHash.digest('hex'),expectedPlain);
  assert.throws(()=>actual.update(data),{code:'ERR_CRYPTO_HASH_FINALIZED'});
});
