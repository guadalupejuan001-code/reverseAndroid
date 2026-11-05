import test from 'node:test';import assert from 'node:assert/strict';
import {mkdtemp,open,readFile,stat,writeFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';import {join} from 'node:path';
import {copySparseImage} from '../tools/android_partition_disk.mjs';
test('copies sparse image bytes, holes and an unaligned tail without changing source',async()=>{
 const root=await mkdtemp(join(tmpdir(),'reverse-sparse-copy-'));
 try{
  const source=join(root,'original'),target=join(root,'candidate'),size=17*1024*1024+7;
  const f=await open(source,'wx');await f.truncate(size);
  for(const [at,value] of [[0,0x50],[5*1024*1024+17,0xab],[size-1,0xcd]])await f.write(Buffer.from([value]),0,1,at);await f.close();
  const before=await readFile(source),s=await stat(source);await copySparseImage(source,target);
  assert.deepEqual(await readFile(target),before);assert.deepEqual(await readFile(source),before);assert.equal((await stat(source)).mtimeMs,s.mtimeMs);
  const output=await stat(target);assert.equal(output.size,size);assert.ok(output.blocks*512<8*1024*1024,'Zero holes were expanded into dense storage');
 }finally{await rm(root,{recursive:true});}
});
test('collision and overlap never overwrite an existing file',async()=>{
 const root=await mkdtemp(join(tmpdir(),'reverse-sparse-copy-'));
 try{const source=join(root,'source'),target=join(root,'target');await writeFile(source,'original');await writeFile(target,'retain');await assert.rejects(copySparseImage(source,target),{code:'EEXIST'});await assert.rejects(copySparseImage(source,source),/paths overlap/);assert.equal((await readFile(source)).toString(),'original');assert.equal((await readFile(target)).toString(),'retain');}
 finally{await rm(root,{recursive:true});}
});
test('invalid source leaves no candidate',async()=>{const root=await mkdtemp(join(tmpdir(),'reverse-sparse-copy-'));try{const source=join(root,'empty'),target=join(root,'candidate');await writeFile(source,'');await assert.rejects(copySparseImage(source,target),/invalid sparse copy source/);await assert.rejects(stat(target),{code:'ENOENT'});}finally{await rm(root,{recursive:true});}});
