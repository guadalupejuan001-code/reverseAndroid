// Deterministically advance the reviewed C10 host/auditor to the exact C10 stop.
// No guest state is created or modified. Generated sources are hash-pinned too.
import {readFileSync as read,writeFileSync as write} from 'node:fs';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const hash=x=>createHash('sha256').update(x).digest('hex');
const oldState='773277104d567e2f72a6e1c66907dfcf5f87f17d77ee1b2dafcee64e87ab0ad3';
const newState='dc1683ae2cf0f469852785f83d12d18e99cb9631a3b1d4ba4600c60993353307';
const oldReport='2f50ba79ef1bcfd0838a3bbd344b92d749576671cd6f76e357159c91c73a6d0c';
const newReport='3f7e28bb92a652ac6a791566cdb2c120ddc83dbd2469c9b6126df6758d57188d';
function produce(input,output,inputHash,outputHash,edit){
 const original=read(input,'utf8');assert.equal(hash(original),inputHash,'Changed source: '+input);
 const result=edit(original);assert.equal(hash(result),outputHash,'Generated source differs: '+output);
 write(output,result);console.log(outputHash+'  '+output);
}
const replace=(s,pairs)=>pairs.reduce((t,[a,b])=>t.replaceAll(a,b),s);
produce('tools/guest_c09_resume.cpp','tools/guest_c10_resume.cpp',
 'ea15b97ce6554c8a99ee609edb597e5fad5719fb0ebc593398cd5b8761633fec','4e085634a3f3eae5e39258af4658724765a33f1a00b9331e444932b06cf8a2a0',
 s=>replace(s,[['C10','C11'],['C09','C10'],['c09','c10'],[oldState,newState],[oldReport,newReport],['6456253329','6456253347'],['0x4ce520','0x4ce568'],['subtick==49','subtick==3'],['0x0e612804u','0x4c9f0192u']]));
produce('tools/android_c10_audit.mjs','tools/android_c11_audit.mjs',
 'f1c75d917864759916141c1996b54401ee7047ca8b6fae444a603651ed7cecd8','95f95180a38c4a8bcb2d8e75097c6240f7ab477cea24466d28086c2653742841',s=>{
 s=replace(s,[['C10','C11'],['C09','C10'],['c09-descendant','c10-descendant'],['source_c09','source_c10'],[oldState,newState],[oldReport,newReport],['6456253329','6456253347'],['0x4ce520','0x4ce568'],['subtick:49','subtick:3'],['0xe612804','0x4c9f0192']]);
 s=s.replace('parent08:',`parent09:'${oldReport}',parent08:`);
 return replace(s,[
  ["[['parent-c08.json',PIN.parent08]","[['parent-c09.json',PIN.parent09],['parent-c08.json',PIN.parent08]"],
  ['assert.equal(r.provenance.input_report_sha256,PIN.parent08);',"assert.equal(r.provenance.input_report_sha256,PIN.parent09);\n  assert.equal(parents['parent-c09.json'].provenance.input_report_sha256,PIN.parent08);"],
  ["source.parents['parent-c08.json'].console+source.report.console","source.parents['parent-c08.json'].console+source.parents['parent-c09.json'].console+source.report.console"],
  ["copy(resolve(input,'current.json'),resolve(out,'parent-c09.json'));","copy(resolve(input,'current.json'),resolve(out,'parent-c10.json'));"],
  ["for(const name of ['parent-c08.json'","for(const name of ['parent-c09.json','parent-c08.json'"]
 ]);
});
produce('tools/android_c10_supervised.mjs','tools/android_c11_supervised.mjs',
 '3dc28fe42e6c69f31a1b327369065da1a4bfb411f2d0e117baec60457d055169','5a2ff392177d519e6710422880508b43bdbe27115e5fc8e124203326717a0027',
 s=>replace(s,[['C10','C11'],['C09','C10'],['android_c10_audit','android_c11_audit']]));
produce('tests/android_c10_audit.test.mjs','tests/android_c11_audit.test.mjs',
 '0893ca3b4d85f3159675ca7990081552da347277f7b9226a9c949eb09723e307','c6334ec0770709749eafebfefeeb02e0c1fb14356d47e232bb9146cf65f2b133',
 s=>replace(s,[['C09','C10'],['c09-descendant','c10-descendant'],['android_c10_audit','android_c11_audit']]));
