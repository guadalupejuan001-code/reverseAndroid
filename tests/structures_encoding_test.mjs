// Cross-check all arrangements and vector start registers with LLVM, not the emulator.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
const dir=mkdtempSync(join(tmpdir(),'reverse-structures-encoding-'));
try{
 const lines=['.text'],expected=[];
 for(const [operation,count,selem] of [[0,4,4],[4,3,3],[8,2,2],[7,1,1],[10,2,1],[6,3,1],[2,4,1]])
 for(let size=0;size<4;size++)for(let wide=0;wide<2;wide++){
  if(selem>1&&!wide&&size===3)continue;
  const bytes=wide?16:8,arrangement=`${bytes/(1<<size)}${'bhsd'[size]}`;
  for(let load=0;load<2;load++)for(let post=0;post<3;post++)for(let rd=0;rd<32;rd++){
   const regs=Array.from({length:count},(_,n)=>`v${(rd+n)&31}.${arrangement}`).join(', ');
   lines.push(`${load?'ld':'st'}${selem} { ${regs} }, [x12]${post===1?`, #${count*bytes}`:post===2?', x17':''}`);
   expected.push((0x0c000000|(wide<<30)|(load<<22)|(post?0x00800000|((post===1?31:17)<<16):0)|(operation<<12)|(size<<10)|(12<<5)|rd)>>>0);
  }
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
 console.log(JSON.stringify({llvm_encoding_cases:expected.length,all_vector_start_registers:true,android_boot:false}));
}finally{rmSync(dir,{recursive:true,force:true});}
