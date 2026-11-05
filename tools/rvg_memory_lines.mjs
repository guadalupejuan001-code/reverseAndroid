// Decode one bounded record at a time; never create a whole-memory JS string.
export function* rvgMemoryLines(bytes,{maxLineBytes=16384}={}){
 if(!Buffer.isBuffer(bytes)||!Number.isSafeInteger(maxLineBytes)||maxLineBytes<1||maxLineBytes>65536)throw TypeError('Invalid bounded memory-line input');
 for(let at=0;at<bytes.length;){
  let end=bytes.indexOf(10,at);if(end<0)end=bytes.length;
  if(end-at>maxLineBytes)throw Error('Oversized RVG memory record');
  // latin1 preserves high bits so record validators can reject invalid bytes.
  yield bytes.toString('latin1',at,end);at=end+1;
 }
}
