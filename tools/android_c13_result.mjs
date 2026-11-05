// Read-only fresh-experiment evidence binding; no inherited C06/C12 success.
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,copyFileSync as copy,statSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {analyzeGuestConsole} from './android_guest_outcome.mjs';
import {PIN,deriveHandoff,readDirectory,sha} from './android_c13_avb.mjs';
const kernel='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8';
const ramdisk='c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da';
const original='console=ttyAMA0 earlycon=pl011,0x09000000 keep_bootcon nokaslr maxcpus=1 printk.devkmsg=on audit=1 panic=-1 8250.nr_uarts=1 binder.impl=rust cma=0 firmware_class.path=/vendor/etc/ loop.max_part=7 init=/init bootconfig androidboot.force_normal_boot=1 androidboot.fstab_suffix=cf.ext4.cts androidboot.slot_suffix=_a initcall_blacklist=scx_init androidboot.boot_devices=30000000.pcie';
const bounded=(p,max)=>{assert.ok(statSync(p).size<=max,'Oversized evidence');return read(p);};
export function verifyC13(state,r,pointer,handoff){
  const v=r.provenance,g=r.guest;assert.equal(v.experiment,'c13-fresh-avb-handoff');
  assert.equal(v.terminal_c12_restored,false);assert.equal(v.prior_init_proof_reused,false);
  assert.equal(v.immutable_media_source_sha256,'88f890e0237a3723cadf308ee5fcd23778d1f101e36c87e40e6ca13eb8aa0117');
  assert.equal(v.timing_seed_origin,'host_os_getrandom_new_experiment');assert.equal(v.ram_bytes,134217728);assert.equal(v.counter_instructions_per_tick,64);
  assert.equal(typeof r.first_user_instruction_verified,'boolean');
  assert.equal(r.initial_instructions,0);assert.equal(r.prior_user_instruction_verified,false);assert.equal(r.verified_entry_checkpoint_sha256,'');
  const digest=sha(state);assert.equal(digest,pointer.checkpoint_sha256);assert.equal(digest,r.checkpoint_sha256);
  const a=auditGkiRvg(state,kernel,digest);assert.equal(a.logical_ram_bytes,134217728);assert.equal(a.rvi_version,'RVI3');
  assert.equal(a.external_disk_bytes,PIN.diskBytes);assert.equal(a.external_disk_sha256,PIN.diskSha);
  assert.equal(a.bootargs,original+' '+handoff.bootargs);assert.equal(v.bootargs,a.bootargs);
  assert.equal(a.instructions,g.instructions);assert.equal(a.native_pc,g.pc);assert.equal(r.advanced_instructions,g.instructions);
  assert.equal(a.native_halted,g.halted?1:0);assert.equal(g.system_mode,true);
  assert.equal(g.logical_ram_bytes,a.logical_ram_bytes);assert.equal(g.block_image_bytes,PIN.diskBytes);assert.equal(g.pci_bus,true);
  assert.equal(v.kernel_sha256,kernel);assert.equal(v.ramdisk_sha256,ramdisk);assert.equal(v.disk_sha256,PIN.diskSha);assert.equal(v.disk_bytes,PIN.diskBytes);
  const field=i=>Number(state.readBigUInt64LE(8+i*8)),head=8+(state[3]===51?44:43)*8;
  const kn=Number(state.readBigUInt64LE(head+8)),dt=Number(state.readBigUInt64LE(head+16)),rd=Number(state.readBigUInt64LE(head+24));
  assert.equal(sha(state.subarray(head+48+kn+dt,head+48+kn+dt+rd)),ramdisk);
  const machine=head+field(39)+field(40)+field(41),system=field(42),clock=machine+system-64;
  assert.equal(state.readBigUInt64LE(machine),7n);assert.equal(Number(state.readBigUInt64LE(machine+32)),g.exception_level);
  assert.equal(state.readBigUInt64LE(machine+8),1n);assert.equal(Number(state.readBigUInt64LE(machine+16)),g.waiting?1:0);
  assert.equal(state.readBigUInt64LE(clock),2n);assert.equal(sha(state.subarray(clock+8,clock+40)),v.timing_seed_sha256);
  for(const [offset,key] of [[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])assert.equal(Number(state.readBigUInt64LE(clock+offset)),g[key]);
  // WFI can jump to a timer deadline and reset subtick without executing an
  // instruction. Bind the persisted phase, not instructions modulo cadence.
  assert.equal(g.counter_instructions_per_tick,64);assert.ok(Number.isInteger(g.counter_subtick)&&g.counter_subtick>=0&&g.counter_subtick<64);
  assert.equal(typeof r.console,'string');assert.ok(Buffer.byteLength(r.console)<=4*1024*1024);
  const outcome=analyzeGuestConsole(r.console),clean=['host_wall_budget','instruction_budget'].includes(r.stop_reason)&&!r.error&&!outcome.guest_boot_failure&&!outcome.guest_reboot_requested&&g.instructions>0&&!g.halted&&!g.waiting;
  return {schema:'reverse-c13-result-v1',fresh_experiment:true,terminal_c12_restored:false,checkpoint_sha256:digest,instructions:g.instructions,pc:g.pc,stop_reason:r.stop_reason,error:r.error,first_original_init_instruction_verified:r.first_user_instruction_verified,verified_avb_arguments_in_saved_image:true,execution_progress_accepted:clean,...outcome,system_mount_verified:false,android_framework_verified:false,android_display_verified:false,native_android_games_verified:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){try{
  const [root,metadata]=process.argv.slice(2);assert.ok(root&&metadata,'Expected result-root metadata-directory');
  const handoff=deriveHandoff(await readDirectory(metadata)),pointer=JSON.parse(bounded(join(root,'journal/latest.json'),65536));assert.match(pointer.generation,/^generation-[1-9][0-9]*$/);
  const dir=join(root,'journal',pointer.generation),state=bounded(join(dir,'checkpoint.rvg'),256*1024*1024),report=bounded(join(dir,'report.json'),8*1024*1024),r=JSON.parse(report);
  const result=verifyC13(state,r,pointer,handoff);copy(join(dir,'checkpoint.rvg'),join(root,'current.rvg'));copy(join(dir,'report.json'),join(root,'current.json'));
  write(join(root,'console.txt'),r.console);write(join(root,'milestone.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result,null,2));if(!result.execution_progress_accepted)process.exitCode=2;
}catch(e){console.error(e.message);process.exitCode=1;}}
