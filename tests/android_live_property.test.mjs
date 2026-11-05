import test from 'node:test';import assert from 'node:assert/strict';
import {enumerateMapleVmas,userMemoryReader,mountedFilePath} from '../tools/android_live_property.mjs';
import {inspectBootPropertyArea,inspectBootPropertyPage} from '../tools/android_boot_property.mjs';
function maple(){
 const mm=0xffff800000001000n,node=0xffff800000002000n,vma=0xffff800000003000n,memory=new Map(),set=(a,v)=>memory.set(a,BigInt(v));
 const layouts={mm_struct:{mm_mt:{offset:64},map_count:{offset:128}},maple_tree:{ma_root:{offset:8}},maple_node:{parent:{offset:0}},maple_range_64:{pivot:{offset:8,count:15},slot:{offset:128,count:16},meta:{offset:248}},maple_arange_64:{pivot:{offset:8,count:9},slot:{offset:80,count:10},meta:{offset:240}},vm_area_struct:{vm_start:{offset:0},vm_end:{offset:8},vm_mm:{offset:16},vm_file:{offset:24},vm_flags:{offset:32},vm_pgoff:{offset:40}}};
 set(mm+128n,1);set(mm+72n,node+10n);set(node,mm+65n);set(node+8n,4095);set(node+16n,12287);set(node+136n,vma);set(node+248n,2);
 set(vma,4096);set(vma+8n,12288);set(vma+16n,mm);set(vma+32n,9);
 return {mm,node,vma,memory,set,read:(a,n=8)=>(memory.get(a)??0n)&((1n<<BigInt(n*8))-1n),layout:name=>layouts[name]};
}
test('walks the actual range-slot ABI with gaps and validates the owning mm',()=>{const f=maple(),r=enumerateMapleVmas(f);assert.equal(r.length,1);assert.equal(r[0].start,4096n);assert.equal(r[0].end,12288n);assert.equal(r[0].filePageOffset,0n);});
for(const kind of ['wrong_mm','wrong_parent','wrong_count','bad_end','bad_pivot','dense','cycle'])test('rejects invalid/transitional maple evidence: '+kind,()=>{
 const f=maple();if(kind==='wrong_mm')f.set(f.vma+16n,f.mm+1n);if(kind==='wrong_parent')f.set(f.node,f.node+1n);if(kind==='wrong_count')f.set(f.mm+128n,2);if(kind==='bad_end')f.set(f.node+248n,16);if(kind==='bad_pivot')f.set(f.node+16n,1);if(kind==='dense')f.set(f.mm+72n,f.node+2n);if(kind==='cycle'){f.set(f.mm+72n,f.node+18n);f.set(f.node+128n,f.node+18n);}assert.throws(()=>enumerateMapleVmas(f));
});
function user(){
 const table=new Map([[0x1000n,0x2003n],[0x2000n,0x3003n],[0x3008n,0x6443n],[0x3010n,0x9443n]]),data=new Map([[0x6ffen,17],[0x6fffn,34],[0x9000n,51],[0x9001n,68]]);
 return {table,data,options:{pgdPhysical:0x1000n,userAddressBits:39,physical64:a=>table.get(a)??0n,physicalByte:a=>data.get(a)??0}};
}
test('reads consecutive user bytes across noncontiguous physical pages',()=>{const f=user(),r=userMemoryReader(f.options);assert.deepEqual([...r.read(0x1ffen,4)],[17,34,51,68]);assert.equal(r.translate(0x2000n),0x9000n);});
for(const kind of ['unmapped','no_access_flag','no_user_permission','table_restricts_user'])test('rejects inaccessible property pages: '+kind,()=>{
 const f=user();if(kind==='unmapped')f.table.set(0x3008n,0n);if(kind==='no_access_flag')f.table.set(0x3008n,0x6043n);if(kind==='no_user_permission')f.table.set(0x3008n,0x6403n);if(kind==='table_restricts_user')f.table.set(0x1000n,0x2003n|(1n<<61n));assert.throws(()=>userMemoryReader(f.options).read(0x1000n,1));
});
test('bounds user addresses, allocation lengths and root alignment',()=>{const f=user(),r=userMemoryReader(f.options);for(const n of [0,-1,1048577,Infinity])assert.throws(()=>r.read(0x1000n,n));assert.throws(()=>r.read(1n<<39n,1));assert.throws(()=>userMemoryReader({...f.options,pgdPhysical:0x1001n}));});
function area(){
 const b=Buffer.alloc(8192);b.writeUInt32LE(4212,0);b.writeUInt32LE(0x504f5250,8);b.writeUInt32LE(0xfc6ed0ab,12);b.writeUInt32LE(112,144);
 b.writeUInt32LE(3,240);b.write('sys\0',260);b.writeUInt32LE(136,256);b.writeUInt32LE(14,264);b.write('boot_completed\0',284);b.writeUInt32LE(4096,268);
 b.writeUInt32LE(0x1000000,4224);b.write('1\0',4228);b.write('sys.boot_completed\0',4320);return b;
}
test('finds a full structured property beyond the header page without assuming physical adjacency',()=>{const b=area();assert.equal(inspectBootPropertyPage(b.subarray(0,4096)).status,'needs_virtual_page_mapping');const r=inspectBootPropertyArea(b);assert.equal(r.status,'property_observed');assert.equal(r.value,'1');});
test('root dirty backup can contain an old value while the current property remains valid',()=>{const b=area();b.write('running\0',148);const r=inspectBootPropertyArea(b);assert.equal(r.status,'property_observed');assert.equal(r.value,'1');});
test('high-bit corruption cannot become ASCII property or value proof',()=>{for(const at of [260,4228,4320]){const b=area();b[at]|=128;assert.notEqual(inspectBootPropertyArea(b).status,'property_observed');}});
test('rejects malformed mapped extents and labels incomplete traversal',()=>{assert.throws(()=>inspectBootPropertyArea(Buffer.alloc(8191)));assert.throws(()=>inspectBootPropertyArea(Buffer.alloc(1048576+4096)));assert.match(inspectBootPropertyArea(area().subarray(0,4096),{diagnostics:true}).diagnostic_error,/mapped area/);});
test('a user pointer cannot masquerade as a kernel file mapping',()=>assert.throws(()=>mountedFilePath(0x1000n,{read:()=>0n,layout:()=>({})})));
