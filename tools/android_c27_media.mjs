// New empty writable media follows Cuttlefish's CreateBlankEmptyImage policy.
// The old disk and all eight original partition payloads remain untouched.
import assert from 'node:assert/strict';
import {open,writeFile,mkdir} from 'node:fs/promises';
import {join,resolve} from 'node:path';import {pathToFileURL} from 'node:url';
import {extendAndroidUserdataDisk} from './android_partition_disk.mjs';
export const BLANK_SHA='a7c744c13cc101ed66c29f672f92455547889cc586ce6d44fe76ae824958ea51';
export const DISK_SHA='4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e',DISK_BYTES=10888413184;
export async function createC27Media(original,directory){
 await mkdir(directory);const input=join(directory,'userdata-blank.img'),target=join(directory,'android-c27.raw');
 const f=await open(input,'wx');try{await f.truncate(2147483648);}finally{await f.close();}
 const r=await extendAndroidUserdataDisk(original,input,target,{expectedUserdataSha256:BLANK_SHA});
 assert.equal(r.disk_sha256,DISK_SHA);assert.equal(r.disk_bytes,DISK_BYTES);
 const report={...r,userdata_input_policy:'new_blank_zero_2GiB_guest_formats_and_encrypts',official_userdata_source_preserved:true,cuttlefish_policy_reference:'google/android-cuttlefish@c857a805bd20fe32a791eb3552ed0c43d48b6452:base/cvd/cuttlefish/host/libs/config/data_image.cpp'};
 await writeFile(join(directory,'media.json'),JSON.stringify(report,null,2)+'\n',{flag:'wx'});return report;
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{const [original,dir]=process.argv.slice(2);assert.ok(original&&dir);console.log(JSON.stringify(await createC27Media(original,dir),null,2));}catch(e){console.error(e.stack);process.exitCode=1;}}
