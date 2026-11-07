// Read-only retained split-ring evidence. Completed descriptor buffers can be
// reused or freed by the guest, so these bytes are never execution history.
import assert from 'node:assert/strict';

const hex=x=>'0x'+BigInt(x).toString(16);
export function inspectGpuControlQueue({gpu,resources,scanout,physicalByte}) {
  const q=gpu.queues[0], result={read_only:true,queue:{...q},resources,scanout,
    retained_buffers_are_not_proof_of_historical_execution:true,entries:[]};
  assert.ok(Number.isInteger(q.size)&&q.size>0&&q.size<=256,'Invalid bounded GPU queue size');
  assert.ok(q.enabled,'GPU control queue is disabled');
  let total=0;
  const bytes=(address,length)=>{
    assert.ok(Number.isInteger(length)&&length>=0&&length<=65536,'Oversized retained GPU buffer');
    total+=length;assert.ok(total<=1048576,'GPU observation byte budget exceeded');
    const at=BigInt(address),out=Buffer.alloc(length);
    for(let i=0;i<length;i++)out[i]=physicalByte(at+BigInt(i));
    return out;
  };
  const u16=at=>bytes(at,2).readUInt16LE(0);
  const available=u16(BigInt(q.avail)+2n),used=u16(BigInt(q.used)+2n);
  result.available_index=available;result.used_index=used;
  result.device_and_used_indices_match=q.last===available&&q.used_index===used;
  const count=Math.min(q.size,32),seenHeads=new Set();
  for(let back=1;back<=count;back++){
    const sequence=(available-back)&65535,slot=sequence%q.size;
    const head=u16(BigInt(q.avail)+4n+BigInt(slot*2));
    if(seenHeads.has(head))continue;seenHeads.add(head);
    const entry={available_sequence:sequence,head,descriptor_contents_may_have_been_reused:true};
    result.entries.push(entry);
    try{
      assert.ok(head<q.size,'Invalid retained descriptor head');
      const visited=new Set(),input=[],output=[],descriptors=[];let index=head,writing=false;
      for(;;){
        assert.ok(index<q.size&&!visited.has(index),'Cyclic or invalid retained descriptor chain');visited.add(index);
        const d=bytes(BigInt(q.desc)+BigInt(index*16),16),address=d.readBigUInt64LE(0),length=d.readUInt32LE(8),flags=d.readUInt16LE(12);
        assert.ok((flags&~3)===0,'Indirect or unsupported retained descriptor');
        descriptors.push({index,address:hex(address),length,flags});
        if(flags&2){writing=true;output.push(bytes(address,length));}
        else{assert.ok(!writing,'Retained request follows a response buffer');input.push(bytes(address,length));}
        if(!(flags&1))break;index=d.readUInt16LE(14);
      }
      entry.descriptors=descriptors;
      const request=Buffer.concat(input),response=Buffer.concat(output);
      assert.ok(request.length>=24&&response.length>=24,'Retained GPU headers unavailable');
      const type=request.readUInt32LE(0),flags=request.readUInt32LE(4),fence=request.readBigUInt64LE(8);
      entry.request={type:hex(type),flags,fence:hex(fence),bytes:request.length};
      entry.response={type:hex(response.readUInt32LE(0)),flags:response.readUInt32LE(4),fence:hex(response.readBigUInt64LE(8)),bytes:response.length};
      assert.ok((flags&~1)===0&&request.readUInt32LE(16)===0,'Retained GPU header is not supported by the actual device');
      const offsets=new Map([[0x101,24],[0x102,24],[0x103,44],[0x104,40],[0x105,48],[0x106,24],[0x107,24]]);
      if(offsets.has(type)){
        const offset=offsets.get(type);assert.ok(request.length>=offset+4,'Truncated retained resource command');
        const id=request.readUInt32LE(offset);entry.request.resource_id=id;
        entry.request.resource_present_in_current_table=id!==0&&resources.some(r=>r.id===id);
      }
      if(type===0x101){assert.ok(request.length>=40);entry.request.create={format:request.readUInt32LE(28),width:request.readUInt32LE(32),height:request.readUInt32LE(36)};}
      entry.fenced_header_pair_matches=Boolean(flags&1)&&entry.response.flags===1&&response.readBigUInt64LE(8)===fence;
      entry.request_header_hex=request.subarray(0,Math.min(request.length,64)).toString('hex');
      entry.response_header_hex=response.subarray(0,24).toString('hex');
    }catch(e){entry.unverified=e.message;}
  }
  result.physical_bytes_read=total;
  return result;
}
