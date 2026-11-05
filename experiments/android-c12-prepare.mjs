// Deterministic C12 preparation from the exact published/tested C11 files.
import {readFileSync as read,writeFileSync as write,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const hash=b=>createHash('sha256').update(b).digest('hex');
const expected={
  "src/minimal_guest_simd.inc": "c3666c55af9ded8960107f09dc233eeb8c201c3b37f3a59f64ac37724f61a59b",
  "tools/guest_c10_resume.cpp": "4e085634a3f3eae5e39258af4658724765a33f1a00b9331e444932b06cf8a2a0",
  "tools/android_c11_audit.mjs": "95f95180a38c4a8bcb2d8e75097c6240f7ab477cea24466d28086c2653742841",
  "tools/android_c11_supervised.mjs": "5a2ff392177d519e6710422880508b43bdbe27115e5fc8e124203326717a0027",
  "tests/android_c11_audit.test.mjs": "c6334ec0770709749eafebfefeeb02e0c1fb14356d47e232bb9146cf65f2b133"
};
const source={};for(const [path,sha] of Object.entries(expected)){const b=read(path);assert.equal(hash(b),sha,'Unexpected C11 source: '+path);source[path]=b.toString();}
function once(s,a,b){assert.equal(s.split(a).length,2,'Ambiguous/missing anchor: '+a);return s.replace(a,b);}
const files={};
files['src/minimal_guest_simd.inc']=once(source['src/minimal_guest_simd.inc'],
  '  // LD1/ST1 sequential registers and LD2/3/4-ST2/3/4 interleaved structures.',
  '  if((i&0xfffffc00u)==0x5ef1b800u){ // ADDP Dd,Vn.2D: integer pairwise scalar reduction.\n'+
  '    fp_access();const auto source=floating_.v[rn];\n'+
  '    // Unsigned addition wraps modulo 2^64; copy both lanes before aliasing Rd.\n'+
  '    // Scalar writes clear the upper 64 bits and leave NZCV/FPCR/FPSR unchanged.\n'+
  '    fp_set(rd,source.low+source.high,64);return true;\n  }\n'+
  '  // LD1/ST1 sequential registers and LD2/3/4-ST2/3/4 interleaved structures.');
const shift=s=>s.replace(/(?<![A-Za-z0-9])C11(?![A-Za-z0-9])/g,'C12').replace(/(?<![A-Za-z0-9])c11(?![A-Za-z0-9])/g,'c12').replace(/(?<![A-Za-z0-9])C10(?![A-Za-z0-9])/g,'C11').replace(/(?<![A-Za-z0-9])c10(?![A-Za-z0-9])/g,'c11');
const pins=s=>s.replaceAll('dc1683ae2cf0f469852785f83d12d18e99cb9631a3b1d4ba4600c60993353307','f93a5f85f892916c61772cd33d3361c6c961acb43fc43e7df1a92b38889c65f4')
 .replaceAll('3f7e28bb92a652ac6a791566cdb2c120ddc83dbd2469c9b6126df6758d57188d','af7c8c022f790307f2ccc4d5a6b9ca7672f7602364719218a12fa1e27b5dbd61')
 .replaceAll('6456253347','6484672081').replaceAll('0x4ce568','0x4cb8e4').replaceAll('0x4c9f0192','0x5ef1b800');
let host=pins(shift(source['tools/guest_c10_resume.cpp']));
host=once(host,'clock.subtick==3','clock.subtick==49');
files['tools/guest_c11_resume.cpp']=host;
let audit=pins(shift(source['tools/android_c11_audit.mjs']));
audit=once(audit,'subtick:3,','subtick:49,');
audit=once(audit,"parent09:","parent10:'3f7e28bb92a652ac6a791566cdb2c120ddc83dbd2469c9b6126df6758d57188d',parent09:");
audit=once(audit,"[['parent-c09.json',PIN.parent09]","[['parent-c10.json',PIN.parent10],['parent-c09.json',PIN.parent09]");
audit=once(audit,'assert.equal(r.provenance.input_report_sha256,PIN.parent09);',"assert.equal(r.provenance.input_report_sha256,PIN.parent10);\n  assert.equal(parents['parent-c10.json'].provenance.input_report_sha256,PIN.parent09);");
audit=once(audit,"source.parents['parent-c09.json'].console+source.report.console","source.parents['parent-c09.json'].console+source.parents['parent-c10.json'].console+source.report.console");
audit=once(audit,"for(const name of ['parent-c09.json'","for(const name of ['parent-c10.json','parent-c09.json'");
files['tools/android_c12_audit.mjs']=audit;
files['tools/android_c12_supervised.mjs']=shift(source['tools/android_c11_supervised.mjs']);
files['tests/android_c12_audit.test.mjs']=shift(source['tests/android_c11_audit.test.mjs']);
for(const [path,text] of Object.entries(files)){if(path!=='src/minimal_guest_simd.inc')assert.ok(!existsSync(path),'Refuse to overwrite '+path);write(path,text);console.log(hash(text)+'  '+path);}
