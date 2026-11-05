#!/usr/bin/env node
// Read-only comparison of the selected fstab with acquisition metadata.
// Does not read GPT sectors, verify AVB, mount filesystems or execute Android.
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const need=(v,m)=>{if(!v)throw Error('Android partition readiness: '+m);};
const sha=b=>createHash('sha256').update(b).digest('hex');
export function androidPartitionReadiness(acquisition,fstab,{slotSuffix='_a',expectedFstabSha256,expectedDiskSha256,logicalPartitions}={}){
 need(Buffer.isBuffer(fstab)&&fstab.length>0&&fstab.length<=65536,'fstab must be 1..65536 bytes');
 need(/^_[ab]$/.test(slotSuffix),'unsupported Android slot suffix');
 const fstabSha=sha(fstab),disk=acquisition?.disk??acquisition;
 if(expectedFstabSha256)need(fstabSha===expectedFstabSha256,'selected fstab SHA-256 mismatch');
 need(disk&&Array.isArray(disk.partitions)&&disk.partitions.length>0&&disk.partitions.length<=128,'missing partition inventory');
 if(expectedDiskSha256)need(disk.disk_sha256===expectedDiskSha256,'disk manifest identity mismatch');
 const names=new Set();for(const p of disk.partitions){need(/^[a-z][a-z0-9_]{0,35}$/.test(p.name)&&!names.has(p.name),'invalid or duplicate physical partition');names.add(p.name);}
 const logical=logicalPartitions===undefined?null:new Set(logicalPartitions);
 if(logical)need(Array.isArray(logicalPartitions)&&logicalPartitions.every(n=>typeof n==='string'&&/^[a-z][a-z0-9_]{0,35}$/.test(n)),'invalid logical inventory');
 const mounts=[];
 for(const raw of fstab.toString('utf8').split(/\r?\n/)){
  const line=raw.replace(/\s*#.*$/,'').trim();if(!line)continue;
  const fields=line.split(/\s+/);need(fields.length===5,'fstab record must have five fields');
  const [device,mount,filesystem,,manager]=fields,flags=new Set(manager.split(','));
  const recovery=flags.has('recoveryonly'),logicalEntry=flags.has('logical');
  const physical=/^\/dev\/block\/by-name\/([a-z][a-z0-9_]*)$/.exec(device)?.[1];
  const slotted=n=>flags.has('slotselect')?n+slotSuffix:n;
  const name=logicalEntry?slotted(device):physical?slotted(physical):null;
  mounts.push({device,mount,filesystem,flags:manager,normal_boot_active:!recovery,
   first_stage:flags.has('first_stage_mount'),required:!recovery&&!flags.has('nofail')&&mount!=='/misc',
   category:logicalEntry?'logical_in_super':physical?'physical_by_name':'kernel_or_external_device',
   partition_name:name,present:logicalEntry?(logical?logical.has(name):null):physical?names.has(name):null});
 }
 need(mounts.length>0,'selected fstab has no records');
 const unique=xs=>[...new Set(xs)].sort();
 const active=mounts.filter(m=>m.normal_boot_active),physical=active.filter(m=>m.category==='physical_by_name');
 return {status:'read_only_fstab_inventory_comparison',fstab_sha256:fstabSha,slot_suffix:slotSuffix,
  disk_sha256:disk.disk_sha256??null,disk_identity_source:'acquisition_manifest',
  physical_partitions:[...names].sort(),mounts,
  missing_required_physical_partitions:unique(physical.filter(m=>m.required&&!m.present).map(m=>m.partition_name)),
  missing_auxiliary_partitions:unique(physical.filter(m=>!m.required&&!m.present).map(m=>m.partition_name)),
  absent_inactive_recovery_partitions:unique(mounts.filter(m=>!m.normal_boot_active&&m.category==='physical_by_name'&&!m.present).map(m=>m.partition_name)),
  unverified_logical_partitions:logical?[]:unique(active.filter(m=>m.category==='logical_in_super').map(m=>m.partition_name)),
  missing_required_logical_partitions:logical?unique(active.filter(m=>m.category==='logical_in_super'&&m.required&&!m.present).map(m=>m.partition_name)):null,
  logical_inventory_provided:logical!==null,gpt_bytes_verified:false,logical_filesystems_verified:false,
  android_mounts_verified:false,avb_verified:false,android_boot_verified:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
 try{const [manifest,fstab,expectedFstabSha256,expectedDiskSha256]=process.argv.slice(2);
  need(manifest&&fstab&&/^[a-f0-9]{64}$/.test(expectedFstabSha256??'')&&/^[a-f0-9]{64}$/.test(expectedDiskSha256??''),'usage: acquisition.json selected-fstab expected-fstab-sha256 expected-disk-sha256');
  console.log(JSON.stringify(androidPartitionReadiness(JSON.parse(readFileSync(manifest)),readFileSync(fstab),{expectedFstabSha256,expectedDiskSha256}),null,2));
 }catch(e){console.error(e.message);process.exitCode=1;}
}
