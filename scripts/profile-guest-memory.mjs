#!/usr/bin/env node
// Read-only density audit of a real RVG snapshot. No guest execution, no
// invented memory, no XI/QuantumRuntime state modifications or dependencies.
import {readFileSync} from "node:fs";
import {createHash} from "node:crypto";

const filename=process.argv[2];
if(!filename)throw Error("Usage: node scripts/profile-guest-memory.mjs checkpoint.rvg");
const bytes=readFileSync(filename);
const version=bytes.toString("ascii",0,4);
if(!["RVG1","RVG2","RVG3"].includes(version)||
   !bytes.subarray(4,8).equals(Buffer.from([13,10,26,10])))
  throw Error("Unsupported RVG snapshot version");
const wordCount=42+(version!=="RVG1"?1:0)+(version==="RVG3"?1:0);
const readWord=i=>{
  const n=bytes.readBigUInt64LE(8+i*8);
  if(n>BigInt(Number.MAX_SAFE_INTEGER))throw Error("RVG field exceeds exact numeric range");
  return Number(n);
};
const imageBytes=readWord(39),coreBytes=readWord(40),memoryBytes=readWord(41);
const systemBytes=version==="RVG1"?0:readWord(42);
const fpBytes=version==="RVG3"?readWord(43):0;
const memoryStart=8+wordCount*8+imageBytes+coreBytes;
const memoryEnd=memoryStart+memoryBytes;
if(memoryEnd+systemBytes+fpBytes+8!==bytes.length)
  throw Error("RVG segment lengths do not match file");
let checksum=14695981039346656037n;
const mask=0xffffffffffffffffn;
for(let i=0;i<bytes.length-8;i++)
  checksum=((checksum^BigInt(bytes[i]))*1099511628211n)&mask;
if(checksum!==bytes.readBigUInt64LE(bytes.length-8))
  throw Error("RVG snapshot checksum mismatch");

const memory=bytes.toString("utf8",memoryStart,memoryEnd);
const lines=memory.split("\n");
let cursor=0,revision=null;
if(lines[cursor].startsWith("MXIM2 ")){
  revision=lines[cursor].slice(6);++cursor;
}
if(!lines[cursor]?.startsWith("MXIM1 "))throw Error("Missing InverseMemory snapshot");
++cursor;
const pages=new Map();
const values=new Uint32Array(256);
let overrides=0,previous=-2,run=0,maximumRun=0,contiguousRuns=0;
let minimum=Number.MAX_SAFE_INTEGER,maximum=0;
for(;cursor<lines.length;cursor++){
  const line=lines[cursor];
  if(!line)continue;
  const parts=line.split(" ");
  if(parts[0]==="Q")continue; // Quantum bindings stay in their original authority.
  if(parts.length!==3||parts[0]!=="O")throw Error("Malformed InverseMemory override");
  const address=Number(parts[1]),value=Number(parts[2]);
  if(!Number.isSafeInteger(address)||address<0||!Number.isInteger(value)||value<0||value>255)
    throw Error("Invalid override address or byte");
  if(address<=previous)throw Error("Overrides are not strictly ordered");
  if(address===previous+1)run++;
  else{contiguousRuns++;run=1;}
  maximumRun=Math.max(maximumRun,run);
  previous=address;minimum=Math.min(minimum,address);maximum=Math.max(maximum,address);
  overrides++;values[value]++;
  const page=Math.floor(address/4096);
  pages.set(page,(pages.get(page)||0)+1);
}
const ranked=[...pages.entries()].sort((a,b)=>b[1]-a[1]||a[0]-b[0]);
const result={
  checkpoint:{
    version,checksum_verified:true,sha256:createHash("sha256").update(bytes).digest("hex"),
    bytes:bytes.length,guest_instructions:readWord(4),max_overrides:readWord(1),
    image_bytes:imageBytes,memory_snapshot_bytes:memoryBytes,revision
  },
  memory:{
    overrides,nonzero_deltas:overrides-values[0],zero_deltas:values[0],
    pages_touched:pages.size,full_pages:ranked.filter(x=>x[1]===4096).length,
    dense_pages_90pct:ranked.filter(x=>x[1]>=3687).length,
    dense_pages_50pct:ranked.filter(x=>x[1]>=2048).length,
    contiguous_override_runs:contiguousRuns,longest_run_bytes:maximumRun,
    first_physical_address:overrides?"0x"+minimum.toString(16):null,
    last_physical_address:overrides?"0x"+maximum.toString(16):null
  },
  candidate_page_representation:{
    hypothetical_bytes_if_each_touched_page_were_4096_bytes:pages.size*4096,
    warning:"This estimates page payload only; metadata, transactions and quantum bindings must also be preserved. No page compression is implemented."
  },
  most_common_delta_bytes:[...values.entries()].sort((a,b)=>b[1]-a[1]).slice(0,8).map(([value,count])=>({value,count})),
  densest_pages:ranked.slice(0,16).map(([page,count])=>({page_base:"0x"+(page*4096).toString(16),overrides:count,fill_fraction:count/4096}))
};
process.stdout.write(JSON.stringify(result,null,2)+"\n");
