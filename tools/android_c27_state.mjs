// Independent binary audit for the new blank-data board, not the old C21 disk.
import assert from 'node:assert/strict';import {auditGkiRvg} from './audit_gki_rvg.mjs';import {sha} from './android_c13_avb.mjs';import {DISK_SHA,DISK_BYTES} from './android_c27_media.mjs';
export const SOURCE='8c26488967ee3a9b2a55313810e92bce5ee2a182dbe81e8374bd2f6d3007ea05',KERNEL='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8',INITRD='5278d736d20865d82be9783593690350496d1bcccb157769795316c5e527eefb',IMAGE='01d583917392fd19438d82d7b135db3bd530949e57bc568181aba54b7a92c75d',CAPACITY=1073741824;
export function verifyC27Pins(p){
 assert.equal(p.schema,'reverse-c27-native-pins-v1');assert.equal(p.source_checkpoint_sha256,SOURCE);assert.equal(p.kernel_sha256,KERNEL);assert.equal(p.initrd_sha256,INITRD);assert.equal(p.disk_sha256,DISK_SHA);assert.equal(p.disk_bytes,DISK_BYTES);assert.equal(p.logical_ram_bytes,2147483648);assert.equal(p.host_snapshot_transport_bytes,CAPACITY);return {compiled_native_pins_verified:true};
}
export function verifyC27State(state,r,digest){
 const a=auditGkiRvg(state,KERNEL,digest,{maxSnapshotBytes:CAPACITY}),g=r.guest,v=r.provenance;
 assert.equal(a.immutable_image_sha256,IMAGE);assert.equal(a.logical_ram_bytes,2147483648);assert.equal(a.rvi_version,'RVI3');assert.equal(a.external_disk_sha256,DISK_SHA);assert.equal(a.external_disk_bytes,DISK_BYTES);
 for(const [b,k] of [['instructions','instructions'],['native_pc','pc'],['native_sp','sp']])assert.equal(a[b],g[k]);assert.equal(a.native_halted,g.halted?1:0);assert.equal(a.native_exit_code,g.exit_code);
 assert.equal(digest,r.checkpoint_sha256);assert.equal(r.initial_instructions,0);assert.equal(r.advanced_instructions,g.instructions);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
 assert.equal(g.logical_ram_bytes,2147483648);assert.equal(g.block_image_bytes,DISK_BYTES);assert.equal(g.system_mode,true);assert.equal(g.pci_bus,true);
 assert.equal(v.experiment,'c27-fresh-blank-data-profile');assert.equal(v.source_media_checkpoint_sha256,SOURCE);assert.equal(v.kernel_sha256,KERNEL);assert.equal(v.ramdisk_sha256,INITRD);assert.equal(v.original_ramdisk_sha256,INITRD);assert.equal(v.disk_sha256,DISK_SHA);assert.equal(v.disk_bytes,DISK_BYTES);assert.equal(v.ram_bytes,2147483648);assert.equal(v.bootargs,a.bootargs);
 for(const key of ['fresh_machine','source_cpio_payload_unchanged','source_initrd_unchanged'])assert.equal(v[key],true);
 for(const key of ['original_checkpoint_cpu_restored','prior_init_proof_reused','bootconfig_derived','host_transport_policy_serialized'])assert.equal(v[key],false);
 assert.equal(v.userdata_policy,'new_blank_zero_2GiB_guest_formats_and_encrypts');assert.equal(v.host_snapshot_transport_bytes,CAPACITY);assert.equal(v.timing_seed_origin,'host_os_getrandom_new_experiment');assert.match(v.timing_seed_sha256,/^[0-9a-f]{64}$/);
 const f=i=>Number(state.readBigUInt64LE(8+8*i)),head=8+(state[3]===51?44:43)*8,kn=Number(state.readBigUInt64LE(head+8)),dt=Number(state.readBigUInt64LE(head+16)),rd=Number(state.readBigUInt64LE(head+24));assert.equal(sha(state.subarray(head+48+kn+dt,head+48+kn+dt+rd)),INITRD);
 const m=head+f(39)+f(40)+f(41),clock=m+f(42)-64;
 assert.equal(state.readBigUInt64LE(m),7n);assert.equal(state.readBigUInt64LE(m+8),1n);assert.equal(Number(state.readBigUInt64LE(m+16)),g.waiting?1:0);assert.equal(Number(state.readBigUInt64LE(m+32)),g.exception_level);assert.equal(state.readBigUInt64LE(m+56),2n);assert.equal(Boolean(state.readBigUInt64LE(m+80)&1n),g.mmu_enabled);
 assert.equal(state.readBigUInt64LE(clock),2n);assert.equal(sha(state.subarray(clock+8,clock+40)),v.timing_seed_sha256);
 for(const [offset,key]of[[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])assert.equal(Number(state.readBigUInt64LE(clock+offset)),g[key]);assert.equal(g.counter_instructions_per_tick,64);assert.ok(Number.isInteger(g.counter_subtick)&&g.counter_subtick>=0&&g.counter_subtick<64);
 assert.equal(typeof r.console,'string');assert.ok(Buffer.byteLength(r.console)<=4*1024*1024);return a;
}
