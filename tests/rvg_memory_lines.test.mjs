import test from 'node:test';import assert from 'node:assert/strict';
import {rvgMemoryLines} from '../tools/rvg_memory_lines.mjs';
test('preserves record boundaries, empty records and final newline behavior',()=>{
 for(const text of ['','one','one\n','one\ntwo','one\n\ntwo\n']){
  const expected=text?text.split('\n'):[];if(text.endsWith('\n'))expected.pop();
  assert.deepEqual([...rvgMemoryLines(Buffer.from(text))],expected);
 }
});
test('checks record bounds lazily without decoding all RAM',()=>{const it=rvgMemoryLines(Buffer.from('ok\n'+'x'.repeat(16385)));assert.equal(it.next().value,'ok');assert.throws(()=>it.next(),/Oversized/);});
test('does not fold high-bit corruption into ASCII',()=>{assert.equal([...rvgMemoryLines(Buffer.from([0xd0,10]))][0].charCodeAt(0),0xd0);});
test('rejects invalid inputs and finite bounds',()=>{for(const bytes of ['x',null,new Uint8Array(1)])assert.throws(()=>[...rvgMemoryLines(bytes)]);for(const maxLineBytes of [0,-1,65537,Infinity,1.5])assert.throws(()=>[...rvgMemoryLines(Buffer.from('x'),{maxLineBytes})]);assert.deepEqual([...rvgMemoryLines(Buffer.from('x'),{maxLineBytes:1})],['x']);});
