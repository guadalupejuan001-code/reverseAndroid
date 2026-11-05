// Uses an explicitly labeled PCI protocol fixture, not an Android image.
import test from 'node:test';import assert from 'node:assert/strict';import{mkdtempSync,readFileSync,existsSync,rmSync}from'node:fs';import{tmpdir}from'node:os';import{join}from'node:path';import{spawnSync}from'node:child_process';import{createHash}from'node:crypto';
const sha=b=>createHash('sha256').update(b).digest('hex');
test('snapshot scanout export preserves the source, executes zero CPU instructions and never overwrites an existing file',()=>{
 const native=process.env.REVERSE_CHECKPOINT_FRAME,fixture=process.env.REVERSE_FRAME_FIXTURE;assert.ok(native&&fixture,'Build both native test tools and pass their explicit paths');
 const root=mkdtempSync(join(tmpdir(),'reverse-frame-fixture-'));
 try{
  const state=join(root,'fixture.rvg'),empty=join(root,'empty.rvg'),output=join(root,'fixture-only.ppm');
  const built=spawnSync(fixture,[state,empty],{encoding:'utf8',timeout:30000});assert.equal(built.status,0,built.stderr);assert.equal(JSON.parse(built.stdout).protocol_fixture_only,true);
  const before=readFileSync(state),digest=sha(before),args=[state,'-',digest,'0'.repeat(64),'0',output],run=spawnSync(native,args,{encoding:'utf8',timeout:30000});assert.equal(run.status,0,run.stderr);
  const report=JSON.parse(run.stdout),ppm=readFileSync(output);assert.equal(report.source_checkpoint_sha256,digest);assert.equal(report.cpu_instructions_executed,0);assert.equal(report.original_snapshot_unchanged,true);assert.equal(report.android_boot_verified,false);assert.equal(report.android_display_verified,false);assert.equal(report.status,'scanout_exported');assert.equal(report.width,96);assert.equal(report.height,64);assert.deepEqual(readFileSync(state),before);assert.equal(sha(ppm),report.ppm_sha256);
  const header=Buffer.from('P6\n96 64\n255\n');assert.deepEqual(ppm.subarray(0,header.length),header);const rgb=ppm.subarray(header.length);assert.equal(rgb.length,96*64*3);assert.equal(sha(rgb),report.rgb_sha256);
  for(let y=0;y<64;y++)for(let x=0;x<96;x++){const expected=x<48?(y<32?[255,64,64]:[64,128,255]):(y<32?[64,255,128]:[255,255,255]);assert.deepEqual([...rgb.subarray(3*(y*96+x),3*(y*96+x)+3)],expected);}
  const repeated=spawnSync(native,args,{encoding:'utf8',timeout:30000});assert.notEqual(repeated.status,0);assert.match(repeated.stderr,/new file/);assert.deepEqual(readFileSync(output),ppm);
  const sourceTarget=spawnSync(native,[...args.slice(0,-1),state],{encoding:'utf8',timeout:30000});assert.notEqual(sourceTarget.status,0);assert.deepEqual(readFileSync(state),before);
  const bad=spawnSync(native,[state,join(root,'nonexistent-disk'),'0'.repeat(64),'0'.repeat(64),'1',join(root,'bad.ppm')],{encoding:'utf8',timeout:30000});assert.notEqual(bad.status,0);assert.match(bad.stderr,/Wrong checkpoint SHA-256/);assert.equal(existsSync(join(root,'bad.ppm')),false);
  const emptyBytes=readFileSync(empty),none=join(root,'none.ppm'),no=spawnSync(native,[empty,'-',sha(emptyBytes),'0'.repeat(64),'0',none],{encoding:'utf8',timeout:30000});assert.equal(no.status,3,no.stderr);assert.equal(JSON.parse(no.stdout).status,'no_scanout');assert.equal(existsSync(none),false);assert.deepEqual(readFileSync(empty),emptyBytes);
 }finally{rmSync(root,{recursive:true,force:true});}
});
