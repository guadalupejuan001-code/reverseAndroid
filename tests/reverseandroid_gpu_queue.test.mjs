import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectGpuControlQueue} from '../tools/reverseandroid-gpu-queue.mjs';

function fixture(){
  const memory=Buffer.alloc(8192),q={desc:0x1000,avail:0x1200,used:0x1300,size:8,enabled:1,last:0,used_index:0};
  // The latest available entry wraps from 65535 to zero.
  memory.writeUInt16LE(0,q.avail+2);memory.writeUInt16LE(0,q.used+2);
  memory.writeUInt16LE(0,q.avail+4+7*2);
  memory.writeBigUInt64LE(0x1800n,q.desc);memory.writeUInt32LE(56,q.desc+8);memory.writeUInt16LE(1,q.desc+12);memory.writeUInt16LE(1,q.desc+14);
  memory.writeBigUInt64LE(0x1900n,q.desc+16);memory.writeUInt32LE(24,q.desc+24);memory.writeUInt16LE(2,q.desc+28);
  memory.writeUInt32LE(0x105,0x1800);memory.writeUInt32LE(1,0x1804);memory.writeBigUInt64LE(123n,0x1808);memory.writeUInt32LE(99,0x1830);
  memory.writeUInt32LE(0x1203,0x1900);memory.writeUInt32LE(1,0x1904);memory.writeBigUInt64LE(123n,0x1908);
  const options={gpu:{queues:[q]},resources:[{id:4,format:2,width:832,height:640}],scanout:{resource:4},physicalByte(address){assert.ok(address>=0n&&address<BigInt(memory.length),'outside fixture RAM');return memory[Number(address)];}};
  return {memory,q,options};
}

test('reports missing resource and a matching fenced pair across index wrap without writing RAM or claiming history',()=>{
  const {memory,options}=fixture(),before=Buffer.from(memory),r=inspectGpuControlQueue(options),e=r.entries[0];
  assert.equal(e.available_sequence,65535);assert.equal(e.request.type,'0x105');assert.equal(e.request.resource_id,99);
  assert.equal(e.response.type,'0x1203');assert.equal(e.request.resource_present_in_current_table,false);
  assert.equal(e.fenced_header_pair_matches,true);assert.equal(e.descriptor_contents_may_have_been_reused,true);
  assert.equal(r.retained_buffers_are_not_proof_of_historical_execution,true);assert.deepEqual(memory,before);
});

test('a stale response with another fence is not paired with a retained create request',()=>{
  const {memory,options}=fixture();memory.writeUInt32LE(0x101,0x1800);memory.writeUInt32LE(2,0x181c);memory.writeUInt32LE(832,0x1820);memory.writeUInt32LE(640,0x1824);
  memory.writeBigUInt64LE(124n,0x1908);memory.writeUInt32LE(0x1201,0x1900);
  const e=inspectGpuControlQueue(options).entries[0];
  assert.deepEqual(e.request.create,{format:2,width:832,height:640});assert.equal(e.fenced_header_pair_matches,false);
});

test('corrupt descriptor cycles, indirect descriptors and invalid physical addresses remain unverified',()=>{
  for(const kind of ['cycle','indirect','outside']){
    const {memory,q,options}=fixture();
    if(kind==='cycle')memory.writeUInt16LE(0,q.desc+14);
    if(kind==='indirect')memory.writeUInt16LE(4,q.desc+12);
    if(kind==='outside')memory.writeBigUInt64LE(0x9000n,q.desc);
    const e=inspectGpuControlQueue(options).entries[0];
    assert.match(e.unverified,/Cyclic|Indirect|outside/);assert.notEqual(e.fenced_header_pair_matches,true);
  }
});
