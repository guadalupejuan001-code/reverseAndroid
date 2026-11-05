import test from 'node:test';import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';import {fnv1a64} from '../tools/rvg_checksum.mjs';
import {inspectBootPropertyPage,scanGuestBootProperty} from '../tools/android_boot_property.mjs';
// Independent fixture for AOSP's documented packed ABI, including dirty-backup padding.
function area(value='1'){
 const b=Buffer.alloc(4096);b.writeUInt32LE(0x504f5250,8);b.writeUInt32LE(0xfc6ed0ab,12);
 const root=128,sys=128+112,boot=128+136,prop=128+172;
 b.writeUInt32LE(288,0);b.writeUInt32LE(112,root+16);
 b.writeUInt32LE(3,sys);b.write('sys\0',sys+20);b.writeUInt32LE(136,sys+16);
 b.writeUInt32LE(14,boot);b.write('boot_completed\0',boot+20);b.writeUInt32LE(172,boot+4);
 b.writeUInt32LE(value.length*0x1000000,prop);b.write(value+'\0',prop+4);b.write('sys.boot_completed\0',prop+96);
 return b;
}
test('observes exactly the structured property without mutating RAM',()=>{const b=area(),copy=Buffer.from(b),r=inspectBootPropertyPage(b);assert.equal(r.status,'property_observed');assert.equal(r.value,'1');assert.equal(r.boot_completed_property_in_ram,true);assert.deepEqual(b,copy);});
test('zero and longer values never imply boot completed',()=>{for(const value of ['0','10','true',''])assert.equal(inspectBootPropertyPage(area(value)).boot_completed_property_in_ram,false);});
test('raw strings and mismatched full property names provide no proof',()=>{const b=Buffer.alloc(4096);b.write('sys.boot_completed=1');assert.equal(inspectBootPropertyPage(b),null);const c=area();c.write('sys.boot_completeX',128+172+96);assert.equal(inspectBootPropertyPage(c).status,'invalid_property_name');});
test('a dirty serial does not observe an in-flight value',()=>{const b=area();b.writeUInt32LE(0x1000001,128+172);assert.equal(inspectBootPropertyPage(b).status,'property_update_in_progress');});
test('rejects malformed, cyclic, unaligned and out-of-area offsets',()=>{for(const offset of [112,113,1048572]){const b=area();b.writeUInt32LE(offset,128+112+16);assert.equal(inspectBootPropertyPage(b).status,'invalid_property_area');}});
test('does not infer virtual contiguity from physical neighbors',()=>{const b=area();b.writeUInt32LE(8192,0);b.writeUInt32LE(4096,128+136+4);assert.equal(inspectBootPropertyPage(b).status,'needs_virtual_page_mapping');});
test('Bionic sibling search compares token length before lexical order',()=>{const b=area();b.writeUInt32LE(288,128+16);b.writeUInt32LE(316,0);b.writeUInt32LE(1,128+288);b.write('z\0',128+288+20);b.writeUInt32LE(112,128+288+12);assert.equal(inspectBootPropertyPage(b).value,'1');});
test('unknown ABI, reserved corruption and unterminated values are not accepted',()=>{for(const offset of [8,12,16]){const b=area();b[offset]^=1;assert.equal(inspectBootPropertyPage(b),null);}const b=area();b[128+172+5]=1;assert.equal(inspectBootPropertyPage(b).status,'invalid_property');});

const hash=b=>createHash('sha256').update(b).digest('hex');
function snapshot(records){
 // ABI-only synthetic metadata fixture; it never executes an Android instruction.
 const kernel=Buffer.alloc(64);kernel.writeUInt32LE(0x644d5241,56);
 const image=Buffer.alloc(48+64+80);image.set([82,86,73,51,13,10,26,10]);image.writeBigUInt64LE(64n,8);image.writeBigUInt64LE(80n,40);kernel.copy(image,48);image.write('0'.repeat(64),image.length-64);
 const memory=Buffer.from('MXIM2 0\nMXIM3 0\n'+records.join('\n')+'\n');
 const out=Buffer.alloc(360+image.length+memory.length+1+8);out.set([82,86,71,51,13,10,26,10]);out.writeBigUInt64LE(2147483648n,8);out.writeBigUInt64LE(BigInt(image.length),8+39*8);out.writeBigUInt64LE(BigInt(memory.length),8+41*8);out.writeBigUInt64LE(1n,8+42*8);image.copy(out,360);memory.copy(out,360+image.length);out.writeBigUInt64LE(fnv1a64(out,out.length-8),out.length-8);return out;
}
function delta(page,index=262149){const mask=Buffer.alloc(512),values=[];for(let i=0;i<4096;i++)if(page[i]){mask[i>>3]|=1<<(i&7);values.push(page[i]);}return 'P '+index+' '+mask.toString('hex')+' '+Buffer.from(values).toString('hex');}
test('reconstructs sparse RAM exactly and skips external u64 pages without numeric rounding',()=>{const b=snapshot([delta(area()),delta(area(),'4000000000000000')]),before=hash(b),r=scanGuestBootProperty(b,before);assert.equal(r.dirty_ram_pages_scanned,1);assert.equal(r.observations[0].value,'1');assert.equal(r.boot_completed_property_in_ram,true);assert.equal(r.live_property_mapping_verified,false);assert.equal(r.android_framework_verified,false);assert.equal(hash(b),before);});
test('a RAM string outside the property ABI cannot prove completion',()=>{const page=Buffer.alloc(4096);page.write('sys.boot_completed=1');const b=snapshot([delta(page)]),r=scanGuestBootProperty(b,hash(b));assert.equal(r.property_area_candidates,0);assert.equal(r.boot_completed_property_in_ram,false);});
test('integrity, duplicate pages and truncated XOR values are rejected',()=>{const d=delta(area());for(const records of [[d,d],[d.slice(0,-2)]]){const b=snapshot(records);assert.throws(()=>scanGuestBootProperty(b,hash(b)));}const b=snapshot([d]);assert.throws(()=>scanGuestBootProperty(b,'0'.repeat(64)),/SHA256 pin mismatch/);b[b.length-9]^=1;assert.throws(()=>scanGuestBootProperty(b,hash(b)),/checksum mismatch/);});
