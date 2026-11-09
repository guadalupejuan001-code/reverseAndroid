// Host delimiter search only; the original RVG/MXIM records remain unchanged.
// Node's numeric Buffer.indexOf uses signed 32-bit offsets/results on affected
// hosts. Search a bounded view and decode local offsets, including past 2 GiB.
export function* rvgMemoryLines(bytes,{maxLineBytes=16384}={}){
 if(!Buffer.isBuffer(bytes)||!Number.isSafeInteger(maxLineBytes)||maxLineBytes<1||maxLineBytes>65536)throw TypeError('Invalid bounded memory-line input');
 for(let at=0;at<bytes.length;){
  const window=bytes.subarray(at,Math.min(bytes.length,at+maxLineBytes+1));
  const newline=window.indexOf(10);
  const length=newline<0?bytes.length-at:newline;
  if(length>maxLineBytes)throw Error('Oversized RVG memory record');
  // latin1 preserves high bits so the unchanged record validators reject them.
  yield window.toString('latin1',0,length);at+=length+1;
 }
}
