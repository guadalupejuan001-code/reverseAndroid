// Independent LLVM assembler check of the vector multiply-long instruction family.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
const dir=mkdtempSync(join(tmpdir(),'reverse-replicate-load-'));
try{
  const lines=['.text'],expected=[];
  for(let count=1;count<=4;count++)for(const q of [false,true])for(let size=0;size<4;size++)for(let mode=0;mode<3;mode++){
    const suffix=[q?'16b':'8b',q?'8h':'4h',q?'4s':'2s',q?'2d':'1d'][size];
    for(let rn=0;rn<32;rn++)for(let rd=0;rd<32;rd++){
      const rm=(rn+rd)%31,regs=Array.from({length:count},(_,n)=>`v${(rd+n)%32}.${suffix}`).join(', ');
      const post=mode===0?'':mode===1?`, #${count*(1<<size)}`:`, x${rm}`;
      lines.push(`ld${count}r {${regs}}, [${rn===31?'sp':`x${rn}`}]${post}`);
      expected.push((0x0d40c000|(q?0x40000000:0)|(((count-1)&1)<<21)|(((count-1)>>1)<<13)|(size<<10)|(mode?0x800000:0)|((mode===1?31:mode===2?rm:0)<<16)|(rn<<5)|rd)>>>0);
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
  console.log(JSON.stringify({assembler:process.env.AARCH64_AS?"GNU as":"LLVM clang",encoding_cases:expected.length,forms:96,all_source_register_pairs:true,android_boot:false}));
}finally{rmSync(dir,{recursive:true,force:true});}
