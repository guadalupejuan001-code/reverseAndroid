import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {androidPartitionReadiness as inspect} from '../tools/android_partition_readiness.mjs';
const names=['boot_a','init_boot_a','metadata','super','vbmeta_a','vbmeta_system_a','vbmeta_system_dlkm_a','vbmeta_vendor_dlkm_a'];
const disk=()=>({disk:{disk_sha256:'a'.repeat(64),partitions:names.map(name=>({name}))}});
const fstab=Buffer.from(`
# Normal boot includes logical filesystems and late encrypted data.
/dev/block/by-name/vendor_boot /vendor_boot emmc defaults recoveryonly,slotselect
system /system ext4 ro wait,logical,first_stage_mount,slotselect,avb
product /product erofs ro wait,logical,first_stage_mount,slotselect,nofail
/dev/block/by-name/metadata /metadata ext4 noatime wait,first_stage_mount
/dev/block/by-name/userdata /data ext4 noatime latemount,wait,fileencryption=aes-256-xts:aes-256-cts
/dev/block/by-name/misc /misc emmc defaults defaults
/dev/block/zram0 none swap defaults zramsize=75%
/dev/block/vdc1 /sdcard vfat defaults recoveryonly
shared /mnt/vendor/shared virtiofs defaults nofail
`);
test('eight boot inputs do not imply the runtime data partition exists',()=>{
 const r=inspect(disk(),fstab);assert.deepEqual(r.missing_required_physical_partitions,['userdata']);
 assert.deepEqual(r.missing_auxiliary_partitions,['misc']);
 assert.deepEqual(r.absent_inactive_recovery_partitions,['vendor_boot_a']);
 assert.deepEqual(r.unverified_logical_partitions,['product_a','system_a']);
 assert.equal(r.missing_required_logical_partitions,null);
 assert.equal(r.android_boot_verified,false);assert.equal(r.gpt_bytes_verified,false);
});
test('external and kernel devices are not invented as physical partitions',()=>{
 const r=inspect(disk(),fstab);for(const device of ['/dev/block/zram0','/dev/block/vdc1','shared']){
  const m=r.mounts.find(x=>x.device===device);assert.equal(m.category,'kernel_or_external_device');assert.equal(m.present,null);
 }
});
test('adding userdata does not establish filesystem mounts or Android boot',()=>{
 const d=disk();d.disk.partitions.push({name:'userdata'},{name:'misc'});
 const r=inspect(d,fstab);assert.deepEqual(r.missing_required_physical_partitions,[]);
 assert.equal(r.android_mounts_verified,false);assert.equal(r.logical_filesystems_verified,false);assert.equal(r.avb_verified,false);
});
test('logical inventories respect slots and optional mount entries',()=>{
 let r=inspect(disk(),fstab,{logicalPartitions:[]});assert.deepEqual(r.missing_required_logical_partitions,['system_a']);
 r=inspect(disk(),fstab,{slotSuffix:'_b',logicalPartitions:['system_b']});
 assert.deepEqual(r.missing_required_logical_partitions,[]);assert.equal(r.logical_inventory_provided,true);
 assert.equal(r.logical_filesystems_verified,false);
});
test('the selected fstab and manifest identity are pinned independently',()=>{
 const expectedFstabSha256=createHash('sha256').update(fstab).digest('hex');
 assert.equal(inspect(disk(),fstab,{expectedFstabSha256,expectedDiskSha256:'a'.repeat(64)}).fstab_sha256,expectedFstabSha256);
 assert.throws(()=>inspect(disk(),fstab,{expectedFstabSha256:'0'.repeat(64)}),/fstab SHA/);
 assert.throws(()=>inspect(disk(),fstab,{expectedDiskSha256:'0'.repeat(64)}),/manifest identity/);
});
test('ambiguous or malformed inventories fail without mutating the input',()=>{
 const d=disk(),before=JSON.stringify(d),bytes=Buffer.from(fstab);inspect(d,fstab);
 assert.equal(JSON.stringify(d),before);assert.deepEqual(fstab,bytes);
 const duplicate=disk();duplicate.disk.partitions.push({name:'super'});assert.throws(()=>inspect(duplicate,fstab),/duplicate/);
 assert.throws(()=>inspect(d,Buffer.from('system /system ext4 ro')),/five fields/);
 assert.throws(()=>inspect(d,fstab,{slotSuffix:'../../x'}),/slot suffix/);
});
