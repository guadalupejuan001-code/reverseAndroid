// Read-only named Bionic property probes. Fixture is host-only, not Android boot.
import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectBootPropertyArea,inspectBootPropertyPage} from '../tools/android_boot_property.mjs';
import {ODSIGN_BOOT_PATH_PROPERTIES} from '../tools/android_live_property.mjs';
function propertyArea(name,value){
  const tokens=name.split('.'),base=128,offsets=tokens.map((_,i)=>112+48*i);
  const propOff=112+48*tokens.length,used=4*Math.ceil((propOff+96+name.length+1)/4);
  assert.ok(used<4096-128);
  const b=Buffer.alloc(4096);
  b.writeUInt32LE(used,0);
  b.writeUInt32LE(0x504f5250,8);
  b.writeUInt32LE(0xfc6ed0ab,12);
  b.writeUInt32LE(offsets[0],base+16);
  for(let i=0;i<tokens.length;i++){
    const at=base+offsets[i];
    b.writeUInt32LE(tokens[i].length,at);
    b.write(tokens[i]+'\0',at+20,'latin1');
    b.writeUInt32LE(i+1===tokens.length?propOff:0,at+4);
    if(i+1<tokens.length)b.writeUInt32LE(offsets[i+1],at+16);
  }
  const p=base+propOff;
  b.writeUInt32LE(value.length<<24,p);
  b.write(value+'\0',p+4,'latin1');
  b.write(name+'\0',p+96,'latin1');
  return b;
}
test('live odsign.key.done=1 is parsed from exact property trie, without implying boot',()=>{
  const b=propertyArea('odsign.key.done','1'),before=Buffer.from(b);
  const p=inspectBootPropertyArea(b,{name:'odsign.key.done'});
  assert.equal(p.status,'property_observed');
  assert.equal(p.name,'odsign.key.done');
  assert.equal(p.value,'1');
  assert.equal(p.boot_completed_property_in_ram,false);
  assert.deepEqual(b,before);
  assert.equal(inspectBootPropertyPage(b).status,'property_not_found');
});
test('verification.done is independent from key.done and may remain zero',()=>{
  const b=propertyArea('odsign.verification.done','0');
  assert.equal(inspectBootPropertyArea(b,{name:'odsign.verification.done'}).value,'0');
  assert.equal(inspectBootPropertyArea(b,{name:'odsign.key.done'}).status,'property_not_found');
});
test('structured init service can show running; that does not mean odsign exited',()=>{
  const b=propertyArea('init.svc.odsign','running');
  assert.equal(inspectBootPropertyArea(b,{name:'init.svc.odsign'}).value,'running');
  assert.equal(inspectBootPropertyArea(b,{name:'odsign.key.done'}).status,'property_not_found');
});
test('noncanonically named or unrelated properties are rejected or missing',()=>{
  const b=propertyArea('odsign.key.done','0');
  for(const name of ['odsign/key/done','','bad property','a'.repeat(100)])
    assert.throws(()=>inspectBootPropertyArea(b,{name}));
  assert.equal(inspectBootPropertyArea(b,{name:'odsign.verification.done'}).status,'property_not_found');
});
test('dirty serial and corrupt property name are not trusted',()=>{
  const b=propertyArea('odsign.key.done','1');
  const p=128+112+3*48;
  b.writeUInt32LE(0x1000001,p);
  assert.equal(inspectBootPropertyArea(b,{name:'odsign.key.done'}).status,'property_update_in_progress');
  b.writeUInt32LE(0x1000000,p);
  b.write('odsign.key.donX',p+96);
  assert.equal(inspectBootPropertyArea(b,{name:'odsign.key.done'}).status,'invalid_property_name');
});
test('only the pinned target properties are requested',()=>{
  assert.ok(ODSIGN_BOOT_PATH_PROPERTIES.includes('odsign.key.done'));
  assert.ok(ODSIGN_BOOT_PATH_PROPERTIES.includes('odsign.verification.done'));
  assert.ok(ODSIGN_BOOT_PATH_PROPERTIES.includes('init.svc.odsign'));
  assert.ok(!ODSIGN_BOOT_PATH_PROPERTIES.includes('sys.boot_completed'));
});
