// Read-only interpretation of the real shared PL011 output stream.
// Kernel printk can interrupt a userspace write before its terminating newline.
// Original bytes, native execution and original admission gates remain unchanged.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,statSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
export function normalizeMultiplexedConsole(bytes){
  assert.ok(Buffer.isBuffer(bytes)&&bytes.length<=64*1024*1024);
  const before=hash(bytes),raw=bytes.toString('latin1');
  const kernel=/\[\s*[0-9]+\.[0-9]{1,9}\]\[\s*[TC]\s*[0-9]+\][^\r\n]{0,8192}(?:\r+\n|\n)/g;
  let removed=0;
  const stdout=raw.replace(kernel,()=>{removed++;return '';});
  const lines=stdout.split(/[\r\n]+/).map(line=>line.trim());
  assert.equal(hash(bytes),before);
  return {before,stdout,lines,removed};
}
export function observeMultiplexedConsole(bytes,expectedSHA,guestPath){
  assert.match(expectedSHA,/^[0-9a-f]{64}$/);
  assert.match(guestPath,/^\/data\/local\/tmp\/reverseandroid-peer-(?:e11a|abi2|abi3)\.elf$/);
  const {before,stdout,lines,removed}=normalizeMultiplexedConsole(bytes);
  const wanted=expectedSHA+'  '+guestPath;
  const responseCodes={};
  for(const line of lines){
    const match=/^REVERSE_(PTS_[A-Z0-9_]+) RC_([0-9]+)$/.exec(line);
    if(match)(responseCodes[match[1]]??=[]).push(Number(match[2]));
  }
  assert.equal(hash(bytes),before);
  return {
    schema:'reverseandroid-readonly-multiplexed-console-observation-v1',
    raw_console_bytes:bytes.length,raw_console_SHA256:before,
    normalized_stdout_SHA256:hash(Buffer.from(stdout,'latin1')),
    only_complete_timestamp_and_TID_tagged_kernel_records_removed:true,
    removed_kernel_records:removed,
    original_input_bytes_unchanged:true,
    expected_guest_ELF_SHA256:expectedSHA,guest_ELF_path:guestPath,
    actual_exact_guest_hash_and_path_observed:lines.includes(wanted),
    actual_peer_READY_observed:lines.includes('REVERSE_PERIPHERAL_PEER_READY'),
    actual_response_codes:responseCodes,
    actual_loaded_policy_unchanged_hashes:lines.flatMap(line=>{
      const match=/^REVERSE_PTS_POLICY_UNCHANGED SHA_([0-9a-f]{64})$/.exec(line);
      return match?[match[1]]:[];
    }),
    actual_private_device_filesystem_mounts:lines.flatMap(line=>{
      const parts=line.split(/\s+/);
      if(parts.length!==6||parts[0]!=='devpts'||parts[1]!=='/dev/ra-pts'||
         parts[2]!=='devpts'||parts[4]!=='0'||parts[5]!=='0')return [];
      const options=parts[3].split(',');
      if(!options.includes('nosuid')||!options.includes('noexec')||
         !options.some(x=>/^fscontext="?u:object_r:tmpfs:s0"?$/.test(x)))return [];
      return [{source:parts[0],mountpoint:parts[1],filesystem:parts[2],options}];
    }),
    original_HAL_Binder_found:Object.fromEntries([
      'android.hardware.oemlock.IOemLock/default','android.hardware.sensors.ISensors/default'
    ].map(name=>[name,lines.includes('Service '+name+': found')])),
    original_C46_or_boot_or_UI_gates_modified:false,
    boot_or_UI_completion_claimed:false
  };
}

if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  const [file,sha,path,out]=process.argv.slice(2);
  assert.ok(file&&out&&statSync(file).size<=64*1024*1024);
  const result=observeMultiplexedConsole(readFileSync(file),sha,path);
  writeFileSync(out,JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify(result));
}
