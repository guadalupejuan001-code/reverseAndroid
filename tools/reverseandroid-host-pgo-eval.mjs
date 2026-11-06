// Isolated compiler experiment. It never selects or modifies the primary guest.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync,existsSync,linkSync,unlinkSync,rmSync,appendFileSync} from 'node:fs';
import {createReadStream} from 'node:fs';
import {createHash} from 'node:crypto';
import {join,resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {performance} from 'node:perf_hooks';
import {preflightEpoch,runEpochStage} from './reverseandroid-epoch.mjs';

const [sourceArg,rootArg,diskArg,commit]=process.argv.slice(2);
assert.ok(sourceArg&&rootArg&&diskArg);assert.match(commit,/^[0-9a-f]{40}$/);
const source=resolve(sourceArg),root=resolve(rootArg),disk=resolve(diskArg);
const plan=JSON.parse(readFileSync('state/experiments/host-pgo-plan.json'));
const json=p=>JSON.parse(readFileSync(p)),save=(p,x)=>writeFileSync(p,JSON.stringify(x,null,2)+'\n');
const hashFile=async p=>{const h=createHash('sha256');for await(const part of createReadStream(p))h.update(part);return h.digest('hex');};
const output=(key,value)=>{if(process.env.GITHUB_OUTPUT)appendFileSync(process.env.GITHUB_OUTPUT,`${key}=${value}\n`);};
assert.ok(!existsSync(root));mkdirSync(root,{recursive:true});
const admitted=preflightEpoch(source);assert.equal(admitted.contract.checkpoint_sha256,plan.source_checkpoint_sha256);
assert.equal(admitted.contract.instructions,plan.source_instructions);global.gc?.();
const build=resolve('build/reverseandroid'),frame=join(build,'frame'),records=[];
let reference=null;
function buildMode(mode){
  const result=spawnSync('bash',['scripts/build-reverseandroid-epoch-pgo.sh',mode],{stdio:'inherit',timeout:600000});
  assert.ifError(result.error);assert.equal(result.status,0,`Compiler stage ${mode} failed`);
}
async function control(label,binary,steps,compare){
  const directory=join(root,label),started=performance.now();
  const gate=await runEpochStage(source,directory,binary,disk,frame,commit,steps,null,900000);
  assert.equal(gate.source_boundary_verified,true);
  const report=json(join(directory,'current.json'));
  assert.equal(report.stop_reason,'instruction_budget');assert.equal(report.advanced_instructions,steps);
  const sha=await hashFile(join(directory,'current.rvg'));assert.equal(sha,gate.checkpoint_sha256);
  assert.ok(Number.isFinite(report.host_elapsed_ms)&&report.host_elapsed_ms>0);
  const record={label,binary_sha256:await hashFile(binary),source_checkpoint_sha256:admitted.contract.checkpoint_sha256,
    instructions:gate.instructions,advanced_instructions:steps,checkpoint_sha256:sha,original_C46_source_boundary_verified:true,
    native_elapsed_ms:report.host_elapsed_ms,whole_stage_elapsed_ms:Math.round(performance.now()-started),
    canonical_state_reference:label,benchmark_state_is_not_primary_authority:true};
  rmSync(join(directory,'native','journal'),{recursive:true});records.push(record);
  save(join(root,'records.json'),records);
  if(compare){
    if(!reference)reference=record;
    else if(sha!==reference.checkpoint_sha256){
      output('preserve_'+label.replaceAll('-','_'),'true');
      save(join(root,'comparison.json'),{status:'rejected_different_original_snapshot',source_checkpoint_sha256:plan.source_checkpoint_sha256,records,candidate_is_primary:false});
      assert.fail(`Complete original snapshot differs for ${label}; preserve the distinct control and reject the candidate`);
    }else{
      // Preserve one exact byte state for equal controls, with every original
      // report, process binding and admission result retained separately.
      unlinkSync(join(directory,'current.rvg'));linkSync(join(root,reference.label,'current.rvg'),join(directory,'current.rvg'));
      record.canonical_state_reference=reference.label;save(join(root,'records.json'),records);
    }
  }
  console.log(JSON.stringify(record));return record;
}
try{
  buildMode('instrument');
  await control('train',join(build,'pgo-eval','epoch-pgo-instrumented'),plan.training_instructions,false);
  buildMode('use');
  for(const [label,binary] of [['baseline-a',join(build,'epoch')],['candidate-a',join(build,'pgo-eval','epoch-pgo')],
      ['baseline-b',join(build,'epoch')],['candidate-b',join(build,'pgo-eval','epoch-pgo')]]){
    await control(label,binary,plan.measured_instructions,true);
  }
  const base=records.filter(r=>r.label.startsWith('baseline-')).map(r=>r.native_elapsed_ms);
  const candidate=records.filter(r=>r.label.startsWith('candidate-')).map(r=>r.native_elapsed_ms);
  const mean=a=>a.reduce((s,x)=>s+x,0)/a.length,speedup=mean(base)/mean(candidate);
  const comparison={schema:'reverseandroid-isolated-host-pgo-evaluation-v1',source_checkpoint_sha256:plan.source_checkpoint_sha256,
    source_instructions:plan.source_instructions,records,all_measured_original_RVG_SHA256_equal:true,
    measured_speedup:speedup,minimum_required_speedup:plan.minimum_required_speedup,
    recommended_for_further_validation:speedup>=plan.minimum_required_speedup&&Math.max(...candidate)<Math.min(...base),
    candidate_is_primary:false,original_guest_RAM_clock_seed_cadence_IRQ_and_codecs_unchanged:true,
    new_guest_input_events:0,original_decoder_and_C46_admission_used_for_every_control:true,
    state_recovery:'Equal measured controls use the exact baseline-a RVG bytes and their own original metadata. Training has its own distinct full transport. Distinct failing controls are separately preserved.'};
  save(join(root,'comparison.json'),comparison);console.log(JSON.stringify(comparison));
}catch(error){console.error(error.stack);process.exitCode=1;}
