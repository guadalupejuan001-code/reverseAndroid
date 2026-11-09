import test from 'node:test';
import assert from 'node:assert/strict';
import {performance} from 'node:perf_hooks';
import {rvgMemoryLines as originalLines} from '../tools/rvg_memory_lines.mjs';
import {rvgMemoryLines as hostLines} from '../extensions/host-memory-lines/rvg_memory_lines.mjs';

function outcome(reader,bytes,options){
 const lines=[];
 try{for(const line of reader(bytes,options))lines.push(line);return {lines};}
 catch(error){return {lines,error:{name:error.name,message:error.message}};}
}

test('bounded host search preserves original lines, EOF, empty records and every high bit',()=>{
 const page='P 18446744073709551615 '+'ff'.repeat(512)+' '+'a5'.repeat(4096);
 assert.equal(page.length,9240);
 const cases=[Buffer.alloc(0),Buffer.from('\n'),Buffer.from('\n\n'),Buffer.from('last'),
  Buffer.from('first\nlast'),Buffer.from('first\nlast\n'),Buffer.from('first\n\nlast\n'),
  Buffer.from('MXIM2 71351670215\nMXIM3 0\n'+page+'\n'),
  Buffer.from('MXIM2 0\nMXIM3 0\n'+page),
  Buffer.from([0,0x80,0xd0,0xff,10,0xff,0,0x80]),
  Buffer.from('x'.repeat(16384)),Buffer.from('x'.repeat(16384)+'\n'),
  Buffer.from('ok\n'+'x'.repeat(16385)),Buffer.from('ok\n'+'x'.repeat(16385)+'\n')];
 for(const [index,bytes] of cases.entries()){
  const before=Buffer.from(bytes);
  assert.deepEqual(outcome(hostLines,bytes),outcome(originalLines,bytes),'case '+index);
  assert.deepEqual(bytes,before,'input changed in case '+index);
 }
 for(const maxLineBytes of [1,2,7,16384,65536]){
  for(const length of [maxLineBytes-1,maxLineBytes,maxLineBytes+1]){
   for(const suffix of ['', '\n']){
    const bytes=Buffer.from('x'.repeat(length)+suffix);
    assert.deepEqual(outcome(hostLines,bytes,{maxLineBytes}),outcome(originalLines,bytes,{maxLineBytes}));
   }
  }
 }
});

test('invalid inputs, finite limits and lazy oversized-record rejection match the original',()=>{
 for(const bytes of ['x',null,new Uint8Array(1),{},1]){
  assert.deepEqual(outcome(hostLines,bytes),outcome(originalLines,bytes));
 }
 for(const maxLineBytes of [0,-1,65537,Infinity,NaN,1.5,'16',null]){
  assert.deepEqual(outcome(hostLines,Buffer.from('x'),{maxLineBytes}),outcome(originalLines,Buffer.from('x'),{maxLineBytes}));
 }
 const bytes=Buffer.from('ok\n'+'x'.repeat(16385)+'\n');
 for(const reader of [hostLines,originalLines]){
  const lines=reader(bytes);
  assert.equal(lines.next().value,'ok');
  assert.throws(()=>lines.next(),{message:'Oversized RVG memory record'});
 }
});

test('actual buffer beyond 2 GiB preserves all lines and rejects real oversized records',t=>{
 const started=performance.now(),boundary=2**31,maxLineBytes=16384,stride=maxLineBytes+1;
 const terminatedRecords=Math.ceil((boundary+2*stride)/stride);
 const tail=Buffer.from([0x54,0x41,0x49,0x4c,0x80,0xff,0,0x45,0x4e,0x44]);
 const bytes=Buffer.alloc(terminatedRecords*stride+tail.length);
 // calloc supplies untouched zero pages. Touch one LF per 16 KiB instead of
 // filling the entire 2-GiB buffer; the parser still visits every record/byte.
 for(let at=maxLineBytes;at<terminatedRecords*stride;at+=stride)bytes[at]=10;
 tail.copy(bytes,terminatedRecords*stride);
 const firstLargeNewline=Math.ceil((boundary-maxLineBytes)/stride)*stride+maxLineBytes;
 assert.ok(firstLargeNewline>=boundary);
 const firstLargeStart=firstLargeNewline-maxLineBytes;
 const nativeIndex=bytes.indexOf(10,firstLargeStart);
 assert.equal(bytes.subarray(firstLargeStart,firstLargeNewline+1).indexOf(10),maxLineBytes);
 const fullRecord='\0'.repeat(maxLineBytes);

 let count=0;
 for(const line of hostLines(bytes)){
  assert.equal(line,count<terminatedRecords?fullRecord:tail.toString('latin1'),'record '+count);
  count++;
 }
 assert.equal(count,terminatedRecords+1);
 assert.equal(bytes[firstLargeNewline],10);
 assert.deepEqual(bytes.subarray(terminatedRecords*stride),tail);

 // Truncate only the final unterminated record. The complete >2-GiB view is
 // parsed again; neither the prefix nor a real end-of-file suffix is omitted.
 let truncatedCount=0;
 for(const line of hostLines(bytes.subarray(0,bytes.length-3))){
  assert.equal(line,truncatedCount<terminatedRecords?fullRecord:tail.subarray(0,-3).toString('latin1'));
  truncatedCount++;
 }
 assert.equal(truncatedCount,terminatedRecords+1);

 // The affected Node 24 numeric search returns a signed index for this LF.
 // Keep the fixed-reader test valid if a future host fixes the native search.
 if(nativeIndex<0){
  assert.throws(()=>{for(const line of originalLines(bytes)){void line;}},
   {message:'Oversized RVG memory record'});
 }else{
  assert.equal(nativeIndex,firstLargeNewline);
 }

 // Two legitimate records become one genuinely oversized record past 2 GiB.
 // It must fail lazily, at that record, without increasing the 16-KiB bound.
 const badStart=(Math.floor(boundary/stride)+1)*stride;
 const badNewline=badStart+maxLineBytes;
 assert.ok(badStart>boundary&&badNewline<terminatedRecords*stride);
 const reader=hostLines(bytes);
 for(let index=0;index<badStart/stride;index++)assert.equal(reader.next().value,fullRecord);
 bytes[badNewline]=65;
 assert.throws(()=>reader.next(),{message:'Oversized RVG memory record'});
 bytes[badNewline]=10;
 assert.equal(bytes[badNewline],10);

 t.diagnostic(JSON.stringify({node:process.version,actual_buffer_bytes:bytes.length,
  complete_records:terminatedRecords,first_newline_above_2GiB:firstLargeNewline,
  original_numeric_index:nativeIndex,original_signed_index_bug_reproduced:nativeIndex<0,
  full_buffer_and_truncated_EOF_verified:true,real_oversized_record_rejected:true,
  elapsed_ms:Math.round(performance.now()-started),rss_bytes:process.memoryUsage().rss,
  peak_rss_bytes:process.resourceUsage().maxRSS*1024}));
});
