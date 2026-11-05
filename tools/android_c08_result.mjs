// Preserve the exact native output and classify it without manufacturing a milestone.
import {readFileSync,writeFileSync,copyFileSync,existsSync,statSync} from 'node:fs';
import {join,resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
import {C07_PIN as pin,hash,validateC07Report} from './android_verified_c07_source.mjs';
const check=(ok,why)=>{if(!ok)throw Error('C08 result: '+why);};
const exact=n=>Number.isSafeInteger(n)&&n>=0;
const modules=s=>[...new Set([...s.matchAll(/init: Loaded kernel module (\/lib\/modules\/[A-Za-z0-9_.-]+)/g)].map(m=>m[1]))];

// This validates the report's claims, not guest execution in isolation.
// The CLI below separately binds PC/count/EL, clock and all snapshot bytes.
export function classifyC08Report(p,source){
  const previous=validateC07Report(source),g=p?.guest,v=p?.provenance;
  check(g&&v,'missing guest or provenance');
  check(p.initial_instructions===pin.instructions,'source count is not C07');
  check(exact(p.advanced_instructions)&&p.advanced_instructions<=pin.instructions_budget&&
    exact(g.instructions)&&g.instructions===pin.instructions+p.advanced_instructions,'instruction accounting');
  check(p.prior_user_instruction_verified===true&&p.first_user_instruction_verified===false&&
    p.verified_entry_checkpoint_sha256===pin.original_entry_checkpoint_sha256,'C06 entry root lost or reexecuted');
  check(v.input_checkpoint_sha256===pin.checkpoint_sha256&&v.stop_after_init===false,'source is not reviewed C07');
  for(const key of ['kernel_sha256','ramdisk_sha256','disk_sha256','disk_bytes'])
    check(v[key]===pin[key],'changed '+key);
  check(g.logical_ram_bytes===134217728&&g.block_image_bytes===pin.disk_bytes&&g.system_mode===true&&g.pci_bus===true,'machine profile changed');
  check([0,1].includes(g.exception_level),'unsupported terminal exception level');
  check(v.counter_latency===true&&v.timing_seed_origin==='checkpoint'&&v.counter_rate_origin==='checkpoint'&&
    v.timing_seed_sha256===pin.counter_seed_sha256&&v.counter_instructions_per_tick===pin.counter_instructions_per_tick,
    'clock provenance changed');
  check(exact(g.counter_samples)&&g.counter_samples>=pin.counter_samples&&
    g.counter_instructions_per_tick===pin.counter_instructions_per_tick&&exact(g.counter_subtick)&&
    g.counter_subtick<pin.counter_instructions_per_tick,'clock boundary invalid');
  check(typeof p.console==='string'&&Buffer.byteLength(p.console)<=4*1024*1024,'invalid UART');
  check(typeof p.error==='string'&&typeof p.stop_reason==='string','invalid stop information');
  for(const key of ['android_services_verified','android_display_verified','native_android_games_verified'])
    check(v[key]===false,'unproved capability '+key);
  const observed=modules(p.console),fresh=observed.filter(m=>!previous.includes(m));
  const panic=/Kernel panic - not syncing|Attempted to kill init!/.test(p.console);
  const clean=['instruction_budget','host_wall_budget'].includes(p.stop_reason)&&p.error===''&&
    g.halted===false&&g.waiting===false&&!panic;
  const progressed=clean&&p.advanced_instructions>0;
  return {status:progressed?'native_execution_progress':'saved_requires_diagnosis',
    source_checkpoint_sha256:pin.checkpoint_sha256,original_entry_checkpoint_sha256:pin.original_entry_checkpoint_sha256,
    checkpoint_sha256:p.checkpoint_sha256,instructions:g.instructions,advanced_instructions:p.advanced_instructions,
    pc:g.pc,exception_level:g.exception_level,stop_reason:p.stop_reason,error:p.error,
    execution_progress_verified:progressed,kernel_panic_observed:panic,
    first_stage_proved_in_source:true,previously_loaded_modules:previous,
    newly_loaded_modules:fresh,all_observed_loaded_modules:[...new Set([...previous,...observed])],
    android_services_verified:false,android_display_verified:false,native_android_games_verified:false};
}

export function bindC08Snapshot(bytes,p,pointer){
  check(Buffer.isBuffer(bytes)&&bytes.length>=360&&bytes.length<256*1024*1024,'snapshot size/type');
  const magic=bytes.toString('ascii',0,4);
  check(['RVG2','RVG3'].includes(magic)&&bytes.subarray(4,8).equals(Buffer.from([13,10,26,10])),'snapshot magic');
  const sha=hash(bytes);
  check(pointer.checkpoint_sha256===sha&&p.checkpoint_sha256===sha,'snapshot/report/pointer mismatch');
  const field=i=>{const n=bytes.readBigUInt64LE(8+i*8);check(n<=BigInt(Number.MAX_SAFE_INTEGER),'section length overflow');return Number(n);};
  const header=8+(magic==='RVG3'?44:43)*8;
  const system=field(42),fp=magic==='RVG3'?field(43):0;
  const machine=header+field(39)+field(40)+field(41);
  check(system>=64&&system<=262144&&fp<=532&&machine+system+fp+8===bytes.length,'snapshot section extents');
  check(bytes.readBigUInt64LE(40)===BigInt(p.guest.instructions)&&
    '0x'+bytes.readBigUInt64LE(24).toString(16)===p.guest.pc,'native CPU/report mismatch');
  check(bytes.readBigUInt64LE(machine)===7n&&bytes.readBigUInt64LE(machine+32)===BigInt(p.guest.exception_level),'native machine/report mismatch');
  // Hash agreement alone does not establish a valid RVG or unchanged media.
  // Validate the internal checksum and immutable original RVI3 payload as well.
  const audit=auditGkiRvg(bytes,pin.kernel_sha256,sha);
  check(audit.rvi_version==='RVI3'&&audit.immutable_image_sha256===pin.immutable_image_sha256,
    'native immutable image differs from the original C07');
  check(audit.logical_ram_bytes===pin.logical_ram_bytes&&p.guest.logical_ram_bytes===audit.logical_ram_bytes,
    'native logical RAM/report mismatch');
  check([0,1].includes(audit.native_halted)&&typeof p.guest.halted==='boolean'&&
    Boolean(audit.native_halted)===p.guest.halted&&audit.native_exit_code===p.guest.exit_code,
    'native halt/exit/report mismatch');
  check(audit.external_disk_sha256===pin.disk_sha256&&audit.external_disk_bytes===pin.disk_bytes,
    'native disk provenance mismatch');
  const clock=machine+system-64;
  check(bytes.readBigUInt64LE(clock)===2n&&hash(bytes.subarray(clock+8,clock+40))===pin.counter_seed_sha256,'native clock provenance differs');
  for(const [off,key] of [[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])
    check(bytes.readBigUInt64LE(clock+off)===BigInt(p.guest[key]),'native '+key+' differs');
  return sha;
}

if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  const [root,sourcePath]=process.argv.slice(2);
  try{
    check(root&&sourcePath&&process.argv.length===4,'expected output-directory pinned-C07-report.json');
    const sourceBytes=readFileSync(sourcePath);check(hash(sourceBytes)===pin.report_sha256,'source report changed');
    const pointerPath=join(root,'journal','latest.json');check(existsSync(pointerPath),'no committed native journal generation');
    const pointer=JSON.parse(readFileSync(pointerPath));
    check(/^generation-[1-9][0-9]*$/.test(pointer.generation),'unsafe journal generation');
    const dir=join(root,'journal',pointer.generation),statePath=join(dir,'checkpoint.rvg'),reportPath=join(dir,'report.json');
    check(statSync(statePath).size<256*1024*1024&&statSync(reportPath).size<8*1024*1024,'component size limit');
    const bytes=readFileSync(statePath),p=JSON.parse(readFileSync(reportPath));
    bindC08Snapshot(bytes,p,pointer);
    const result=classifyC08Report(p,JSON.parse(sourceBytes));
    // Preserve an authentic stopped state too, but only after all validation.
    copyFileSync(statePath,join(root,'current.rvg'));copyFileSync(reportPath,join(root,'current.json'));
    writeFileSync(join(root,'validated-pair.json'),JSON.stringify({
      generation:pointer.generation,checkpoint_sha256:hash(bytes),report_sha256:hash(readFileSync(reportPath)),
      guest_execution_performed_by_collector:false,external_disk_file_verified:false
    },null,2)+'\n');
    writeFileSync(join(root,'console.txt'),p.console);
    writeFileSync(join(root,'milestone.json'),JSON.stringify(result,null,2)+'\n');
    console.log(JSON.stringify(result));
    if(!result.execution_progress_verified)process.exitCode=2;
  }catch(e){
    console.error(e.message);
    if(root&&existsSync(root))writeFileSync(join(root,'preservation-error.json'),JSON.stringify({error:e.message,android_display_verified:false}));
    process.exitCode=1;
  }
}
