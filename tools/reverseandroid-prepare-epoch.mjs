// First authenticate the latest old-epoch source with its unchanged admission.
import assert from 'node:assert/strict';
import {openSync,readSync,closeSync,readFileSync as read,mkdirSync,writeFileSync as write} from 'node:fs';
import {join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {preflight} from './android_c27_profile_resume.mjs';
import {deriveLightsProfile} from './reverseandroid-lights-profile.mjs';
import {finalizeEpochRoot} from './reverseandroid-epoch.mjs';
try{
  const [parent,disk,native,profileRoot,output]=process.argv.slice(2);assert.ok(parent&&disk&&native&&profileRoot&&output);
  const contract=JSON.parse(read(join(parent,'output-contract.json'))),proof=preflight(contract,parent,{maxTransportBytes:2147483648});
  assert.equal(contract.checkpoint_sha256,'7ecff7c2d460c582eeb3cbdf47fdd6686790cff2add75cd6c7437af7c9b9c3ec');
  assert.equal(proof.report.guest.instructions,88895950532);global.gc?.();
  mkdirSync(profileRoot);const fd=openSync(join(parent,'current.rvg'),'r');let rd;
  try{const h=Buffer.alloc(408);assert.equal(readSync(fd,h,0,h.length,0),h.length);const at=360,k=Number(h.readBigUInt64LE(at+8)),dt=Number(h.readBigUInt64LE(at+16)),n=Number(h.readBigUInt64LE(at+24));rd=Buffer.alloc(n);assert.equal(readSync(fd,rd,0,n,at+48+k+dt),n);}finally{closeSync(fd);}
  const profile=deriveLightsProfile(rd);write(join(profileRoot,'initrd-lights.bin'),profile.initrd,{flag:'wx'});write(join(profileRoot,'profile.json'),JSON.stringify(profile.report,null,2)+'\n',{flag:'wx'});
  const out=openSync(join(profileRoot,'power-cycle-stdout.json'),'wx'),err=openSync(join(profileRoot,'power-cycle-stderr.log'),'wx');let result;
  try{result=spawnSync(native,[join(parent,'current.rvg'),disk,join(profileRoot,'initrd-lights.bin'),output,contract.checkpoint_sha256,profile.report.derived_initrd_sha256],{stdio:['ignore',out,err],timeout:1200000});}finally{closeSync(out);closeSync(err);}
  assert.ifError(result.error);assert.equal(result.status,0,'Hardware power-cycle failed; inspect the separate native log');
  console.log(JSON.stringify(finalizeEpochRoot(output,join(profileRoot,'profile.json'),join(parent,'output-contract.json')),null,2));
}catch(error){console.error(error.stack);process.exitCode=1;}
