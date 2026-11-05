// Independent LLVM assembler check of the vector multiply-long instruction family.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
const dir=mkdtempSync(join(tmpdir(),'reverse-multiply-long-'));
try{
  const lines=['.text'],expected=[];
  for(const upper of [false,true])for(const uns of [false,true])for(let operation=0;operation<3;operation++)for(let size=0;size<3;size++){
    const mnemonic=(uns?'u':'s')+['mull','mlal','mlsl'][operation]+(upper?'2':'');
    const source=[upper?'16b':'8b',upper?'8h':'4h',upper?'4s':'2s'][size],dest=['8h','4s','2d'][size];
    for(let rn=0;rn<32;rn++)for(let rm=0;rm<32;rm++){
      const rd=(rn+rm)%3===0?rn:(rn+rm)%3===1?rm:(rn+rm+1)%32;
      lines.push(`${mnemonic} v${rd}.${dest}, v${rn}.${source}, v${rm}.${source}`);
      expected.push(([0x0e20c000,0x0e208000,0x0e20a000][operation]|(upper?0x40000000:0)|(uns?0x20000000:0)|(size<<22)|(rm<<16)|(rn<<5)|rd)>>>0);
    }
  }
  writeFileSync(join(dir,'all.s'),lines.join('\n')+'\n');
  const run=spawnSync(process.env.AARCH64_AS??process.env.CLANG??'clang',process.env.AARCH64_AS?[join(dir,'all.s'),'-o',join(dir,'all.o')]:['--target=aarch64-none-elf','-c',join(dir,'all.s'),'-o',join(dir,'all.o')],{encoding:'utf8',timeout:60000});
  assert.equal(run.error,undefined);assert.equal(run.status,0,run.stderr);
  const b=readFileSync(join(dir,'all.o'));assert.equal(b.readUInt32LE(0),0x464c457f);assert.equal(b[4],2);assert.equal(b[5],1);assert.equal(b.readUInt16LE(18),183);
  const sh=Number(b.readBigUInt64LE(40)),size=b.readUInt16LE(58),count=b.readUInt16LE(60),str=sh+b.readUInt16LE(62)*size;
  const names=b.subarray(Number(b.readBigUInt64LE(str+24)),Number(b.readBigUInt64LE(str+24)+b.readBigUInt64LE(str+32)));
  let text;
  for(let n=0;n<count;n++){const at=sh+n*size,start=b.readUInt32LE(at);if(names.subarray(start,names.indexOf(0,start)).toString()==='.text')text=b.subarray(Number(b.readBigUInt64LE(at+24)),Number(b.readBigUInt64LE(at+24)+b.readBigUInt64LE(at+32)));}
  assert.ok(text);assert.equal(text.length,expected.length*4);
  expected.forEach((opcode,n)=>assert.equal(text.readUInt32LE(n*4),opcode,lines[n+1]));
  console.log(JSON.stringify({assembler:process.env.AARCH64_AS?"GNU as":"LLVM clang",encoding_cases:expected.length,forms:36,all_source_register_pairs:true,android_boot:false}));
}finally{rmSync(dir,{recursive:true,force:true});}
