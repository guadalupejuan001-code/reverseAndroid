// Local host negative controls; these tests deliberately execute NO guest code.
import test,{after} from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdtempSync,writeFileSync,mkdirSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve} from 'node:path';
assert.ok(process.env.C13_NATIVE,'Set C13_NATIVE to the tested C13 native runner');
const executable=resolve(process.env.C13_NATIVE),root=mkdtempSync(join(tmpdir(),'c13-negative-'));
after(()=>rmSync(root,{recursive:true,force:true}));
const args='androidboot.vbmeta.size=22912 androidboot.vbmeta.hash_alg=sha256 androidboot.vbmeta.digest=92f4ce73ee87fb244fee21aee3de3ddef2fd52d7631e12fc850be8348f3c9ed3';
const good=join(root,'good.txt');writeFileSync(good,args);let counter=0;
function reject(argv,pattern){const r=spawnSync(executable,argv,{encoding:'utf8',timeout:5000});assert.equal(r.error,undefined);assert.equal(r.status,1);assert.match(r.stderr,pattern);assert.equal(r.stdout,'');}
for(const budget of ['0','-1','1x','1200001','1.0',''])test('reject invalid wall budget '+JSON.stringify(budget),()=>reject(['/missing-c12','/missing-disk',join(root,'journal-'+(++counter)),budget,good],/Wall|wall/));
for(const [name,value] of [['size',args.replace('22912','22913')],['algorithm',args.replace('sha256','sha512')],['digest',args.slice(0,-1)+'4'],['extra newline',args+'\n'],['disable request',args+' androidboot.veritymode=disabled']])test('reject altered handoff '+name+' before touching disk',()=>{const path=join(root,'args-'+(++counter));writeFileSync(path,value);reject(['/missing-c12','/missing-disk',join(root,'journal-'+counter),'1',path],/Unverified AVB handoff/);});
test('refuse to reuse an existing journal',()=>{const path=join(root,'existing');mkdirSync(path);reject(['/missing-c12','/missing-disk',path,'1',good],/Fresh experiment needs a new journal/);});
test('reject a fabricated source before disk access',()=>{const path=join(root,'fake.rvg');writeFileSync(path,'not a checkpoint');reject([path,'/missing-disk',join(root,'new'),'1',good],/C12 evidence hash mismatch/);});
test('require all source and handoff arguments',()=>reject([],/Expected C12-evidence/));
