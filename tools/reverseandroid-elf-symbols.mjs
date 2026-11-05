// Read-only ELF64 dynamic symbols from verified process VMAs.
// gABI: https://gabi.xinuos.com/elf/08-dynamic.html
// GNU hash layout: AOSP bionic linker/linker.cpp, commit ec18ce06f2d007be40ad6f043058f5a4c7236573.
import assert from 'node:assert/strict';
export function memoryElfSymbols({memory,areas,pathFor,file}) {
  const moduleAreas=areas.filter(a=>a.file&&pathFor(a.file)===file);
  const headerArea=moduleAreas.find(a=>a.filePageOffset===0n);assert.ok(headerArea,'No mapped ELF header');
  const h=memory.read(headerArea.start,64);
  assert.equal(h.subarray(0,6).toString('hex'),'7f454c460201');assert.equal(h.readUInt16LE(18),183);
  assert.equal(h.readUInt16LE(54),56);const count=h.readUInt16LE(56);assert.ok(count>0&&count<=1024);
  const phoff=h.readBigUInt64LE(32);assert.ok(phoff<65536n);
  const headers=memory.read(headerArea.start+phoff,count*56);let loadZero,dynamic;
  for(let i=0;i<count;i++){
    const p=headers.subarray(i*56,(i+1)*56),type=p.readUInt32LE(0),offset=p.readBigUInt64LE(8),va=p.readBigUInt64LE(16),size=p.readBigUInt64LE(40);
    if(type===1&&offset===0n)loadZero=va;if(type===2)dynamic={va,size};
  }
  assert.ok(loadZero!==undefined&&dynamic&&dynamic.size>0n&&dynamic.size<=65536n);
  const bias=headerArea.start-loadZero,tags=new Map(),db=memory.read(bias+dynamic.va,Number(dynamic.size));
  let terminated=false;for(let i=0;i+16<=db.length;i+=16){const tag=db.readBigUInt64LE(i);if(tag===0n){terminated=true;break;}tags.set(tag,db.readBigUInt64LE(i+8));}
  assert.ok(terminated);assert.equal(tags.get(11n),24n);
  const ptr=value=>{
    assert.ok(value!==undefined);const candidates=[value,bias+value].filter(p=>moduleAreas.some(a=>p>=a.start&&p<a.end));
    assert.equal(candidates.length,1,'Ambiguous ELF dynamic pointer');return candidates[0];
  };
  let symbols;
  if(tags.has(4n))symbols=memory.read(ptr(tags.get(4n)),8).readUInt32LE(4);
  else {
    const at=ptr(tags.get(0x6ffffef5n)),g=memory.read(at,16),buckets=g.readUInt32LE(0),first=g.readUInt32LE(4),words=g.readUInt32LE(8);
    assert.ok(buckets>0&&buckets<=65536&&words>0&&words<=65536);
    const bucketAt=at+16n+BigInt(words)*8n,b=memory.read(bucketAt,buckets*4);let max=0;
    for(let i=0;i<buckets;i++)max=Math.max(max,b.readUInt32LE(i*4));
    if(max===0)symbols=first;
    else {assert.ok(max>=first&&max<65536);let i=max;for(;i<65536;i++){const chain=memory.read(bucketAt+BigInt(buckets*4+(i-first)*4),4).readUInt32LE(0);if(chain&1)break;}assert.ok(i<65536);symbols=i+1;}
  }
  assert.ok(symbols>0&&symbols<=65536);const stringSize=Number(tags.get(10n));assert.ok(stringSize>0&&stringSize<=8388608);
  const boundedRead=(address,size)=>{const chunks=[];for(let i=0;i<size;i+=1048576)chunks.push(memory.read(address+BigInt(i),Math.min(1048576,size-i)));return Buffer.concat(chunks);};
  const strings=boundedRead(ptr(tags.get(5n)),stringSize),table=boundedRead(ptr(tags.get(6n)),symbols*24),functions=[];
  for(let i=0;i<symbols;i++){
    const s=table.subarray(i*24,(i+1)*24),kind=s[4]&15,nameAt=s.readUInt32LE(0),value=s.readBigUInt64LE(8),size=s.readBigUInt64LE(16);
    if(![2,10].includes(kind)||s.readUInt16LE(6)===0||value===0n||size===0n)continue;
    assert.ok(nameAt<strings.length);const end=strings.indexOf(0,nameAt);assert.ok(end>=nameAt&&end-nameAt<8192);
    const start=bias+value;
    if(!moduleAreas.some(a=>(a.flags&4n)&&start>=a.start&&start<a.end))continue;
    functions.push({start,end:start+size,name:strings.subarray(nameAt,end).toString()});
  }
  functions.sort((a,b)=>a.start<b.start?-1:a.start>b.start?1:0);
  return {file,symbol_count:symbols,defined_function_count:functions.length,lookup(address){
    let best;for(const f of functions){if(f.start>address)break;if(address<f.end)best=f;}
    return best?{name:best.name,offset:'0x'+(address-best.start).toString(16),range_verified:true}:null;
  }};
}
