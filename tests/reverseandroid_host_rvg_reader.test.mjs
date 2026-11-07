import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {createHash} from 'node:crypto';
import {readBoundedRvg,HOST_RVG_CAP,installLargeRvgReader} from '../tools/reverseandroid-host-rvg-reader.mjs';

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
