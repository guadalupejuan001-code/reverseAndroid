// Verify every scalar ADDP register pair against LLVM assembly, not Android execution.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
const dir=mkdtempSync(join(tmpdir(),'reverse-addp-scalar-encoding-'));
try{
  const lines=['.text'],expected=[];
  for(let rn=0;rn<32;rn++)for(let rd=0;rd<32;rd++){
    lines.push(`addp d${rd}, v${rn}.2d`);
    expected.push((0x5ef1b800|(rn<<5)|rd)>>>0);
  }
  writeFileSync(join(dir,'all.s'),lines.join('\n')+'\n');
  const run=spawnSync(process.env.CLANG??'clang',['--target=aarch64-none-elf','-c',join(dir,'all.s'),'-o',join(dir,'all.o')],{encoding:'utf8',timeout:30000});
  assert.equal(run.error,undefined);assert.equal(run.status,0,run.stderr);
  const b=readFileSync(join(dir,'all.o'));assert.equal(b.readUInt32LE(0),0x464c457f);assert.equal(b[4],2);assert.equal(b[5],1);assert.equal(b.readUInt16LE(18),183);
  const sh=Number(b.readBigUInt64LE(40)),size=b.readUInt16LE(58),count=b.readUInt16LE(60),str=sh+b.readUInt16LE(62)*size;
  const names=b.subarray(Number(b.readBigUInt64LE(str+24)),Number(b.readBigUInt64LE(str+24)+b.readBigUInt64LE(str+32)));
  let text;
  for(let n=0;n<count;n++){const at=sh+n*size,start=b.readUInt32LE(at);if(names.subarray(start,names.indexOf(0,start)).toString()==='.text')text=b.subarray(Number(b.readBigUInt64LE(at+24)),Number(b.readBigUInt64LE(at+24)+b.readBigUInt64LE(at+32)));}
  assert.ok(text);assert.equal(text.length,expected.length*4);
  expected.forEach((opcode,n)=>assert.equal(text.readUInt32LE(n*4),opcode,lines[n+1]));
  console.log(JSON.stringify({llvm_encoding_cases:expected.length,forms:1,all_register_pairs:true,android_boot:false}));
}finally{rmSync(dir,{recursive:true,force:true});}
