// Independent LLVM assembler check of the single-lane SIMD load/store family.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
const dir=mkdtempSync(join(tmpdir(),'reverse-lane-transfer-'));
try{
  const lines=['.text'],expected=[];
  for(let count=1;count<=4;count++)for(const load of [false,true])for(let size=0;size<4;size++)for(let lane=0;lane<(16>>size);lane++)for(let mode=0;mode<3;mode++){
    const suffix=['b','h','s','d'][size];
    const q=lane>>(3-size),s=size===3?0:(lane>>(2-size))&1,field=size===0?lane&3:size===1?(lane&1)*2:size===2?0:1,opcode=2*(size===3?2:size)+((count-1)>>1);
    for(let rn=0;rn<32;rn++)for(let rd=0;rd<32;rd++){
      const rm=(rn+rd)%31,regs=Array.from({length:count},(_,n)=>`v${(rd+n)%32}.${suffix}`).join(', ');
      const post=mode===0?'':mode===1?`, #${count*(1<<size)}`:`, x${rm}`;
      lines.push(`${load?'ld':'st'}${count} {${regs}}[${lane}], [${rn===31?'sp':`x${rn}`}]${post}`);
      expected.push((0x0d000000|(load?0x400000:0)|(q<<30)|(((count-1)&1)<<21)|(opcode<<13)|(s<<12)|(field<<10)|(mode?0x800000:0)|((mode===1?31:mode===2?rm:0)<<16)|(rn<<5)|rd)>>>0);
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
  console.log(JSON.stringify({assembler:process.env.AARCH64_AS?"GNU as":"LLVM clang",encoding_cases:expected.length,lane_and_address_forms:720,all_source_register_pairs:true,android_boot:false}));
}finally{rmSync(dir,{recursive:true,force:true});}
