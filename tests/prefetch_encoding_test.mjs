// Independent LLVM assembler check of the AArch64 prefetch hints.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
const dir=mkdtempSync(join(tmpdir(),'reverse-prefetch-'));
try{
  const lines=['.text'],expected=[];
  const add=(line,opcode)=>{lines.push(line);expected.push(opcode>>>0);};
  const reg=n=>n===31?'sp':`x${n}`;
  for(let hint=0;hint<32;hint++){
   for(let rn=0;rn<32;rn++){
    for(const imm of [0,1,2048,4095])add(`prfm #${hint}, [${reg(rn)}, #${imm*8}]`,0xf9800000|(imm<<10)|(rn<<5)|hint);
    for(const imm of [-256,-1,0,1,255])add(`prfum #${hint}, [${reg(rn)}, #${imm}]`,0xf8800000|((imm&511)<<12)|(rn<<5)|hint);
   }
   for(const imm of [-262144,-1,0,1,262143])add(`prfm #${hint}, .${imm<0?'':'+'}${imm*4}`,0xd8000000|((imm&0x7ffff)<<5)|hint);
   for(const rm of [0,8,20,30,31])for(const option of [2,3,6,7])for(const scale of [0,1]){
    const index=rm===31?(option&1?'xzr':'wzr'):`${option&1?'x':'w'}${rm}`;
    const extend={2:'uxtw',3:'lsl',6:'sxtw',7:'sxtx'}[option];
    add(`prfm #${hint}, [x20, ${index}, ${extend} #${scale*3}]`,0xf8a00800|(rm<<16)|(option<<13)|(scale<<12)|(20<<5)|hint);
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
  console.log(JSON.stringify({assembler:process.env.AARCH64_AS?"GNU as":"LLVM clang",encoding_cases:expected.length,forms:4,all_hint_codes:true,android_boot:false}));
}finally{rmSync(dir,{recursive:true,force:true});}
