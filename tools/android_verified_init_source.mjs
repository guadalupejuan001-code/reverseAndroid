// Read-only validation. Native XI/Ether remains the sole execution authority.
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {auditGkiRvg} from './audit_gki_rvg.mjs';
const hash=b=>createHash('sha256').update(b).digest('hex');
const require=(v,m)=>{if(!v)throw Error('Verified init source: '+m);};
export function verifyInitSource(state,reportBytes,pin){
  require(hash(reportBytes)===pin.report_sha256,'report hash differs');
  require(hash(state)===pin.checkpoint_sha256,'checkpoint hash differs');
  const p=JSON.parse(reportBytes);
  require(p.first_user_instruction_verified===true&&p.stop_reason==='first_user_instruction_verified'&&p.error==='','original instruction was not proved');
  require(p.checkpoint_sha256===pin.checkpoint_sha256&&p.guest.instructions===pin.instructions&&p.guest.pc===pin.pc&&p.guest.exception_level===0,'report boundary differs');
  require(p.provenance.disk_sha256===pin.disk_sha256&&p.guest.block_image_bytes===pin.disk_bytes,'disk differs');
  require(p.provenance.timing_seed_origin==='checkpoint'&&p.provenance.timing_seed_sha256===pin.counter_seed_sha256,'counter provenance differs');
  require(p.guest.counter_samples===pin.counter_samples&&p.guest.counter_instructions_per_tick===pin.counter_instructions_per_tick&&p.guest.counter_subtick===pin.counter_subtick,'counter report differs');
  const audit=auditGkiRvg(state,'f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8',pin.checkpoint_sha256);
  require(audit.instructions===pin.instructions&&audit.external_disk_sha256===pin.disk_sha256&&audit.external_disk_bytes===pin.disk_bytes,'native source differs');
  require('0x'+state.readBigUInt64LE(24).toString(16)===pin.pc,'native PC differs');
  const machine=8+(audit.rvg_version==='RVG3'?44:43)*8+[39,40,41].reduce((sum,i)=>sum+Number(state.readBigUInt64LE(8+i*8)),0);
  require(state.readBigUInt64LE(machine)===7n&&state.readBigUInt64LE(machine+4*8)===0n,'native machine version or EL differs');
  const clock=machine+Number(state.readBigUInt64LE(8+42*8))-64;
  require(state.readBigUInt64LE(clock)===2n&&hash(state.subarray(clock+8,clock+40))===pin.counter_seed_sha256,'native clock seed differs');
  for(const [at,key] of [[40,'counter_samples'],[48,'counter_instructions_per_tick'],[56,'counter_subtick']])require(Number(state.readBigUInt64LE(clock+at))===pin[key],'native '+key+' differs');
  return {...audit,original_init_first_instruction_verified:true,pc:pin.pc,source_report_sha256:pin.report_sha256,android_services_verified:false,android_display_verified:false,native_android_games_verified:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{const [s,r,c='experiments/android-post-init-cycle.json']=process.argv.slice(2);console.log(JSON.stringify(verifyInitSource(readFileSync(s),readFileSync(r),JSON.parse(readFileSync(c)))));}
  catch(e){console.error(e.message);process.exitCode=1;}
}
