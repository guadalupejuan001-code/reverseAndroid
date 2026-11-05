// Read-only PCI metadata bound to an already verified native continuation.
// This does not decode RAM, execute instructions or authorize a new source.
import assert from 'node:assert/strict';
import {readFileSync,openSync,readSync,closeSync,fstatSync,createReadStream} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';
const json=p=>JSON.parse(readFileSync(p,'utf8'));
async function sha(path){const hash=createHash('sha256');for await(const b of createReadStream(path))hash.update(b);return hash.digest('hex');}
export async function inspectGpuBacking(root){
 const contract=json(join(root,'output-contract.json')),report=json(join(root,'current.json')),gate=json(join(root,'c46-progress.json'));
 assert.equal(gate.source_boundary_verified,true);assert.equal(gate.instructions,report.guest.instructions);
 for(const x of [gate,report])assert.equal(x.checkpoint_sha256,contract.checkpoint_sha256);
 const path=join(root,'current.rvg');assert.equal(await sha(path),contract.checkpoint_sha256);
 const fd=openSync(path,'r');let machine;
 try{
  const header=Buffer.alloc(360);assert.equal(readSync(fd,header,0,360,0),360);assert.equal(header.toString('ascii',0,4),'RVG3');assert.equal(Number(header.readBigUInt64LE(40)),report.guest.instructions);assert.equal('0x'+header.readBigUInt64LE(24).toString(16),report.guest.pc);
  const sizes=Array.from({length:5},(_,i)=>{const n=header.readBigUInt64LE(320+i*8);assert.ok(n<=2147483648n);return Number(n);});
  assert.equal(360+sizes.reduce((a,b)=>a+b,0)+8,fstatSync(fd).size);assert.ok(sizes[3]<=262144);
  machine=Buffer.alloc(sizes[3]);assert.equal(readSync(fd,machine,0,machine.length,360+sizes[0]+sizes[1]+sizes[2]),machine.length);
 }finally{closeSync(fd);}
 assert.equal(machine.readBigUInt64LE(0),7n);
 const receive=Number(machine.readBigUInt64LE(576));assert.ok(receive<=4096);
 const begin=592+receive,length=Number(machine.readBigUInt64LE(584+receive));assert.ok(length<=196608);assert.equal(begin+length+64,machine.length);
 const pci=machine.subarray(begin,begin+length);let at=0;
 function take(){assert.ok(at+8<=pci.length);const v=pci.readBigUInt64LE(at);at+=8;assert.ok(v<=BigInt(Number.MAX_SAFE_INTEGER));return Number(v);}
 assert.equal(take(),1);assert.equal(take(),1);const disk_bytes=take(),flushes=take(),devices=[];
 for(let d=0;d<2;++d){
  const device=Object.fromEntries(['features','bar','command','feature_select','driver_select','status','isr','queue_select'].map(k=>[k,take()]));
  device.queues=Array.from({length:2},()=>Object.fromEntries(['desc','avail','used','size','enabled','last','used_index'].map(k=>[k,take()])));devices.push(device);
 }
 const resources=Array.from({length:4},()=>{
  const [id,format,width,height,count]=Array.from({length:5},take);assert.ok(count<=2048);
  const backing=Array.from({length:count},()=>({address:take(),length:take()}));
  return {id,format,width,height,backing,total_backing_bytes:backing.reduce((n,e)=>n+e.length,0)};
 });
 const scanout=Object.fromEntries(['resource','x','y','width','height','cursor','cursor_x','cursor_y','hot_x','hot_y'].map(k=>[k,take()]));assert.equal(at,pci.length);
 assert.equal(await sha(path),contract.checkpoint_sha256);
 return {schema:'reverse-android-gpu-backing-evidence-v1',read_only:true,input_unchanged:true,
  checkpoint_sha256:contract.checkpoint_sha256,instructions:report.guest.instructions,pc:report.guest.pc,
  pci_state_version:1,disk_bytes,flushes,gpu:devices[1],resources,scanout};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{assert.ok(process.argv[2]);console.log(JSON.stringify(await inspectGpuBacking(resolve(process.argv[2])),null,2));}catch(e){console.error(e.stack);process.exitCode=1;}}
