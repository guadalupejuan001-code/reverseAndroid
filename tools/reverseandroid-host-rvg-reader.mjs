// Host file I/O only. RVG/RVI/MXIM bytes and all guest capacities are unchanged.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {syncBuiltinESMExports} from 'node:module';

export const HOST_RVG_CAP=3840*1024*1024;
const MAX_CHUNK=64*1024*1024;
export function readBoundedRvg(path,{maxBytes=HOST_RVG_CAP,chunkBytes=MAX_CHUNK}={}) {
  assert.ok(Number.isSafeInteger(maxBytes)&&maxBytes>=1&&maxBytes<=HOST_RVG_CAP,'Invalid finite host RVG bound');
  assert.ok(Number.isSafeInteger(chunkBytes)&&chunkBytes>=1&&chunkBytes<=MAX_CHUNK,'Invalid bounded host read chunk');
  const fd=fs.openSync(path,'r');
  try{
    const info=fs.fstatSync(fd);assert.ok(info.isFile()&&Number.isSafeInteger(info.size)&&info.size<=maxBytes,'RVG exceeds explicit host file bound');
    const out=Buffer.allocUnsafeSlow(info.size);
    for(let at=0;at<out.length;){
      const n=fs.readSync(fd,out,at,Math.min(chunkBytes,out.length-at),at);
      assert.ok(n>0,'Unexpected EOF in bounded RVG read');at+=n;
    }
    const after=fs.fstatSync(fd);assert.equal(after.size,info.size,'RVG file extent changed during read');
    assert.equal(after.mtimeMs,info.mtimeMs,'RVG file changed during read');return out;
  }finally{fs.closeSync(fd);}
}

export function installLargeRvgReader(){
  const key=Symbol.for('reverseandroid.bounded-host-rvg-reader-3840MiB');
  if(globalThis[key]){assert.equal(fs.readFileSync,globalThis[key]);return;}
  const original=fs.readFileSync;
  const replacement=function(path,options){
    const name=typeof path==='string'?path:path instanceof URL&&path.protocol==='file:'?path.pathname:null;
    if(name?.endsWith('.rvg')&&fs.statSync(path).size>0x7fffffff){
      assert.ok(options===undefined||options===null||typeof options==='object'&&!options.encoding,'Large RVG input requires raw bytes');
      return readBoundedRvg(path);
    }
    return original.apply(this,arguments);
  };
  globalThis[key]=replacement;fs.readFileSync=replacement;syncBuiltinESMExports();
}
