import test from 'node:test';
import assert from 'node:assert/strict';
import {observeMultiplexedConsole as observe} from '../tools/reverseandroid-console-proof.mjs';
const sha='548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b';
const path='/data/local/tmp/reverseandroid-peer-abi3.elf';
const line=sha+'  '+path;
const printk="[ 1066.539643][    T1] init: Control message: Could not find 'aidl/activity'\r\n";
const run=s=>observe(Buffer.from(s),sha,path);

test('recognize the exact hash and path even when real-format printk interrupts the newline',()=>{
  assert.equal(run(line+'\r\n').actual_exact_guest_hash_and_path_observed,true);
  assert.equal(run(line+printk+'\r\n').actual_exact_guest_hash_and_path_observed,true);
  assert.equal(run(line.slice(0,25)+printk+line.slice(25)+'\r\n').actual_exact_guest_hash_and_path_observed,true);
});
test('restore interrupted response evidence without recognizing a marker inside printk',()=>{
  assert.deepEqual(run('REVERSE_PTS_CO'+printk+'PY RC_0\r\n').actual_response_codes,{PTS_COPY:[0]});
  assert.deepEqual(run('[ 12.123][ T425] REVERSE_PTS_COPY RC_0\n').actual_response_codes,{});
});
test('keep prompts, quoted input and wrong or incomplete digests ineligible',()=>{
  for(const text of ["printf '%s' '"+line+"'\n",'$ '+line+'\n',line+'x\n',line.replace(sha,'0'.repeat(64))+'\n',line.slice(0,-1)+'\n',line.slice(0,12)+'\b'+line.slice(12)+'\n']){
    assert.equal(run(text).actual_exact_guest_hash_and_path_observed,false);
  }
});
test('do not erase arbitrary bracketed output or mutate any original bytes',()=>{
  const bytes=Buffer.from('[not kernel] '+line+'\n');const copy=Buffer.from(bytes);
  const result=observe(bytes,sha,path);assert.equal(result.removed_kernel_records,0);
  assert.deepEqual(bytes,copy);assert.equal(result.original_C46_or_boot_or_UI_gates_modified,false);
});
test('require an actual six-field mount record with the private path and security context',()=>{
  const mount='devpts /dev/ra-pts devpts rw,nosuid,noexec,fscontext=u:object_r:tmpfs:s0,mode=600,ptmxmode=600 0 0';
  assert.equal(run(mount+'\n').actual_private_device_filesystem_mounts.length,1);
  assert.equal(run(mount.slice(0,50)+printk+mount.slice(50)+'\n').actual_private_device_filesystem_mounts.length,1);
  for(const invalid of ['$ '+mount,"printf '"+mount+"'",mount.replace('/dev/ra-pts','/dev/pts'),mount.replace('tmpfs:s0','devpts:s0'),mount.replace('nosuid,','')]){
    assert.equal(run(invalid+'\n').actual_private_device_filesystem_mounts.length,0);
  }
});
