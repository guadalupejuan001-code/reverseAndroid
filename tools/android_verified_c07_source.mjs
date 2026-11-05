// Independent read-only evidence. Never restores, edits or executes a guest.
import {readFileSync,statSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
export const C07_PIN=Object.freeze(JSON.parse(readFileSync(
  new URL('../experiments/android-c08-reviewed-c07.json',import.meta.url))));
export const hash=b=>createHash('sha256').update(b).digest('hex');
const check=(ok,why)=>{if(!ok)throw Error('Verified C07 source: '+why);};

// Semantic checks are separate so corruption tests can exercise every gate.
// The public verifier additionally requires BOTH immutable byte hashes.
export function validateC07Report(p){
  const c=C07_PIN,g=p?.guest,v=p?.provenance;
  check(g&&v,'missing guest or provenance');
  check(p.checkpoint_sha256===c.checkpoint_sha256,'report snapshot identity');
  check(p.initial_instructions===6250795356&&p.advanced_instructions===100000000&&
    p.initial_instructions+p.advanced_instructions===g.instructions,'instruction accounting');
  check(g.instructions===c.instructions&&g.pc===c.pc&&g.exception_level===c.exception_level,'report boundary');
  check(g.system_mode===true&&g.halted===false&&g.waiting===false&&g.pci_bus===true,'inactive or different machine');
  check(g.logical_ram_bytes===134217728,'RAM profile changed');
  check(p.stop_reason==='instruction_budget'&&p.error==='','source has no clean budget boundary');
  check(p.first_user_instruction_verified===false&&p.prior_user_instruction_verified===true&&
    p.verified_entry_checkpoint_sha256===c.original_entry_checkpoint_sha256,'original entry root');
  check(v.input_checkpoint_sha256===c.original_entry_checkpoint_sha256&&v.stop_after_init===false,'C06 to C07 link');
  check(v.kernel_sha256===c.kernel_sha256&&v.ramdisk_sha256===c.ramdisk_sha256,'original Android media');
  check(v.disk_sha256===c.disk_sha256&&v.disk_bytes===c.disk_bytes&&g.block_image_bytes===c.disk_bytes,'original disk');
  check(v.counter_latency===true&&v.timing_seed_origin==='checkpoint'&&v.counter_rate_origin==='checkpoint'&&
    v.timing_seed_sha256===c.counter_seed_sha256,'original timing stream');
  check(g.counter_samples===c.counter_samples&&g.counter_subtick===c.counter_subtick&&
    g.counter_instructions_per_tick===c.counter_instructions_per_tick&&
    v.counter_instructions_per_tick===c.counter_instructions_per_tick,'counter boundary');
  check(typeof p.console==='string'&&p.console.includes('init: init first stage started!'),'first-stage UART missing');
  check(!/Kernel panic - not syncing|Attempted to kill init!/.test(p.console),'kernel panic');
  for(const k of ['android_services_verified','android_display_verified','native_android_games_verified'])
    check(v[k]===false,'unproved capability claim: '+k);
  return [...new Set([...p.console.matchAll(/init: Loaded kernel module (\/lib\/modules\/[A-Za-z0-9_.-]+)/g)].map(m=>m[1]))];
}

export function verifyC07Source(state,reportBytes){
  check(Buffer.isBuffer(state)&&state.length<256*1024*1024,'invalid state size/type');
  check(Buffer.isBuffer(reportBytes)&&reportBytes.length<=65536,'invalid report size/type');
  check(hash(reportBytes)===C07_PIN.report_sha256,'report hash differs');
  check(hash(state)===C07_PIN.checkpoint_sha256,'checkpoint hash differs');
  const p=JSON.parse(reportBytes),modules=validateC07Report(p),c=C07_PIN;
  const a=auditGkiRvg(state,c.kernel_sha256,c.checkpoint_sha256);
  check(a.rvi_version==='RVI3'&&a.external_disk_sha256===c.disk_sha256&&a.external_disk_bytes===c.disk_bytes,'native disk differs');
  check(a.instructions===c.instructions&&'0x'+state.readBigUInt64LE(24).toString(16)===c.pc,'native CPU boundary differs');
  const n=i=>Number(state.readBigUInt64LE(8+i*8));
  const header=8+(a.rvg_version==='RVG3'?44:43)*8;
  const machine=header+n(39)+n(40)+n(41),clock=machine+n(42)-64;
  check(state.readBigUInt64LE(machine)===7n&&state.readBigUInt64LE(machine+32)===1n,'native machine/EL differs');
  check(state.readBigUInt64LE(clock)===2n&&hash(state.subarray(clock+8,clock+40))===c.counter_seed_sha256,'native timing seed differs');
  for(const [off,key] of [[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])
    check(state.readBigUInt64LE(clock+off)===BigInt(c[key]),'native '+key+' differs');
  const ramdisk=header+48+a.kernel_bytes+a.dtb_bytes;
  check(hash(state.subarray(ramdisk,ramdisk+a.initrd_bytes))===c.ramdisk_sha256,'native ramdisk differs');
  return {status:'reviewed_C07_resume_source_verified',checkpoint_sha256:c.checkpoint_sha256,
    report_sha256:c.report_sha256,original_entry_checkpoint_sha256:c.original_entry_checkpoint_sha256,
    instructions:c.instructions,pc:c.pc,exception_level:1,checkpoint_bytes:state.length,
    loaded_kernel_modules:modules,internal_checksum_verified:true,
    embedded_kernel_and_ramdisk_verified:true,external_disk_identity_verified:true,external_disk_file_verified:false,
    original_counter_verified:true,guest_execution_performed:false,
    android_services_verified:false,android_display_verified:false,native_android_games_verified:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{
    const args=process.argv.slice(2);check(args.length===2,'expected checkpoint.rvg report.json');
    check(statSync(args[0]).size<256*1024*1024&&statSync(args[1]).size<=65536,'file size limit');
    console.log(JSON.stringify(verifyC07Source(readFileSync(args[0]),readFileSync(args[1])),null,2));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
