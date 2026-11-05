// Read-only corroboration from Bionic property areas in actual guest RAM.
// Layout/reference: aosp-mirror/platform_bionic/libc/system_properties/
// include/system_properties/{prop_area,prop_info}.h and prop_area.cpp.
// An observed RAM value alone does not prove a live mapping, framework readiness or display.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,statSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {rvgMemoryLines} from './rvg_memory_lines.mjs';
const NAME='sys.boot_completed',MAGIC=0x504f5250,VERSION=0xfc6ed0ab;
export function inspectBootPropertyPage(page){
 assert.ok(Buffer.isBuffer(page)&&page.length===4096);return inspectBootPropertyArea(page);
}
export function inspectBootPropertyArea(page,{diagnostics=false,name=NAME}={}){
 assert.ok(Buffer.isBuffer(page)&&page.length>=4096&&page.length<=1048576&&page.length%4096===0);
 assert.ok(typeof name==='string'&&name.length>=1&&name.length<=90&&/^[a-zA-Z0-9._-]+$/.test(name),'Invalid target property');
 if(page.readUInt32LE(8)!==MAGIC||page.readUInt32LE(12)!==VERSION)return null;
 const used=page.readUInt32LE(0);
 if(used<112||used>1048576-128||used%4||!page.subarray(16,128).every(b=>b===0))return null;
 const seen=new Set();let incomplete=false;
 const extent=(off,n)=>{
  if(!Number.isInteger(off)||off<0||off%4||off+n>used)throw Error('Invalid property offset');
  if(128+off+n>page.length){incomplete=true;throw Error('Property extends beyond mapped area');}
  return 128+off;
 };
 const node=off=>{
  if(seen.has(off))throw Error('Cyclic property trie');seen.add(off);
  const at=extent(off,20),len=page.readUInt32LE(at);
  if(len>255||(!len&&off!==0))throw Error('Invalid property token length');
  // The empty root's name overlays Bionic's dirty-value backup at data+20.
  // Existing-property updates may leave text there; only nonempty tokens have a terminator.
  extent(off,20+len+1);if(len&&page[at+20+len]!==0)throw Error('Unterminated property token');
  const name=page.toString('latin1',at+20,at+20+len);
  if(len&&!/^[a-zA-Z0-9_-]+$/.test(name))throw Error('Invalid property token');
  return {name,prop:page.readUInt32LE(at+4),left:page.readUInt32LE(at+8),right:page.readUInt32LE(at+12),children:page.readUInt32LE(at+16)};
 };
 try{
  let cur=node(0);if(cur.name!==''||cur.left||cur.right||cur.prop)throw Error('Invalid root');
  for(const token of name.split('.')){
   let off=cur.children;if(!off)return {status:'property_not_found'};
   for(;;){
    cur=node(off);const cmp=token.length-cur.name.length||(token<cur.name?-1:token>cur.name?1:0);
    if(cmp===0)break;off=cmp<0?cur.left:cur.right;if(!off)return {status:'property_not_found'};
   }
  }
  if(!cur.prop)return {status:'property_not_found'};
  const at=extent(cur.prop,96+name.length+1),serial=page.readUInt32LE(at),length=serial>>>24;
  if(serial&1)return {status:'property_update_in_progress'};
  if(serial&0x10000||length>=92||page[at+4+length]!==0||page[at+96+name.length]!==0)return {status:'invalid_property'};
  if(page.toString('latin1',at+96,at+96+name.length)!==name)return {status:'invalid_property_name'};
  const value=page.toString('latin1',at+4,at+4+length);
  if(!/^[\x20-\x7e]*$/.test(value))return {status:'invalid_property'};
  return {status:'property_observed',name,value,serial,boot_completed_property_in_ram:name===NAME&&value==='1'};
 }catch(e){return {status:incomplete?'needs_virtual_page_mapping':'invalid_property_area',...(diagnostics?{diagnostic_error:e.message}:{})};}
}
export function scanGuestBootProperty(state,expectedSha,options){
 assert.match(expectedSha,/^[0-9a-f]{64}$/);
 const a=auditGkiRvg(state,undefined,expectedSha,options),field=i=>Number(state.readBigUInt64LE(8+i*8)),head=8+(state[3]===51?44:43)*8;
 assert.equal(a.rvi_version,'RVI3');
 const kn=Number(state.readBigUInt64LE(head+8)),dt=Number(state.readBigUInt64LE(head+16)),rd=Number(state.readBigUInt64LE(head+24));
 const kernel=state.subarray(head+48,head+48+kn),base=0x40000000,ram=a.logical_ram_bytes;
 const regions=[{at:base+Number(kernel.readBigUInt64LE(8)),bytes:kernel},{at:base+ram/2,bytes:state.subarray(head+48+kn+dt,head+48+kn+dt+rd)},{at:base+ram-2097152,bytes:state.subarray(head+48+kn,head+48+kn+dt)}];
 const start=head+field(39)+field(40),memory=state.subarray(start,start+field(41)),lines=rvgMemoryLines(memory);
 assert.match(lines.next().value,/^MXIM2 [0-9]+$/);assert.equal(lines.next().value,'MXIM3 0');
 const seen=new Set(),observations=[],statusCounts={};let scanned=0,candidates=0;
 for(const line of lines){
  if(!line)continue;
  const m=/^P ([0-9]+) ([0-9a-f]{1024}) ([0-9a-f]*)$/.exec(line);assert.ok(m,'Unsupported memory record');
  const index=BigInt(m[1]);assert.equal(index.toString(),m[1]);assert.ok(index<=0xfffffffffffffn);assert.ok(!seen.has(m[1]),'Duplicate RAM page');seen.add(m[1]);
  const address=index*4096n,mask=Buffer.from(m[2],'hex'),values=Buffer.from(m[3],'hex');assert.equal(m[3].length,values.length*2);let count=0;
  for(const byte of mask){let b=byte;while(b){count++;b&=b-1;}}
  assert.equal(count,values.length,'XOR value count mismatch');
  if(address<BigInt(base)||address+4096n>BigInt(base+ram))continue;
  const at=Number(address),page=Buffer.alloc(4096);
  for(const region of regions){const lo=Math.max(at,region.at),hi=Math.min(at+4096,region.at+region.bytes.length);if(lo<hi)region.bytes.copy(page,lo-at,lo-region.at,hi-region.at);}
  let value=0;for(let i=0;i<4096;i++)if(mask[i>>3]&(1<<(i&7)))page[i]^=values[value++];
  scanned++;const observation=inspectBootPropertyPage(page);if(observation){candidates++;statusCounts[observation.status]=(statusCounts[observation.status]??0)+1;if(observation.status==='property_observed')observations.push({physical_page:'0x'+at.toString(16),...observation});}
 }
 return {schema:'reverse-readonly-boot-property-v1',source_checkpoint_sha256:expectedSha,read_only:true,dirty_ram_pages_scanned:scanned,property_area_candidates:candidates,property_area_status_counts:statusCounts,observations,boot_completed_property_in_ram:observations.some(x=>x.boot_completed_property_in_ram===true),live_property_mapping_verified:false,android_framework_verified:false,android_display_verified:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 try{const [input,sha,output,bound]=process.argv.slice(2),maxSnapshotBytes=bound===undefined?256*1024*1024:Number(bound);assert.ok(input&&sha&&Number.isSafeInteger(maxSnapshotBytes)&&maxSnapshotBytes>=360&&maxSnapshotBytes<=1024*1024*1024);assert.ok(statSync(input).size<=maxSnapshotBytes,'Oversized evidence');const r=scanGuestBootProperty(readFileSync(input),sha,{maxSnapshotBytes});const s=JSON.stringify(r,null,2)+'\n';if(output)writeFileSync(output,s);else process.stdout.write(s);}catch(e){console.error(e.stack);process.exitCode=1;}
}
