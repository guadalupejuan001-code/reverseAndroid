import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,appendFileSync,existsSync} from 'node:fs';
import {join} from 'node:path';
import {runStage} from './android_c42_zygote_progress.mjs';
import {milestone} from './android_c46_continue.mjs';
import {validateCampaignInput,verifyCampaignInputEvent} from './reverseandroid-campaign-input.mjs';

const [stageText,source,output,native,disk,frame,commit,planPath='state/experiments/c71-plan.json']=process.argv.slice(2);
assert.match(planPath,/^state\/experiments\/[a-z0-9-]+\.json$/);
const stage=Number(stageText),plan=JSON.parse(readFileSync(new URL('../'+planPath,import.meta.url)));
assert.ok(Number.isInteger(stage)&&stage>=1&&stage<=plan.stage_budgets.length);
assert.match(commit,/^[0-9a-f]{40}$/);
const json=p=>JSON.parse(readFileSync(p)),contract=json(join(source,'output-contract.json'));
validateCampaignInput(plan,stage,process.env.REVERSE_PC_UART_INPUT_FILE);
if(stage===1){
  assert.equal(contract.checkpoint_sha256,plan.source_checkpoint_sha256);
  assert.equal(json(join(source,'current.json')).guest.instructions,plan.source_instructions);
}else{
  const prior=json(join(source,existsSync(join(source,'campaign-progress.json'))?'campaign-progress.json':'c71-campaign-progress.json'));assert.equal(prior.stage,stage-1);
  assert.equal(prior.checkpoint_sha256,contract.checkpoint_sha256);
  assert.equal(prior.root_source_checkpoint_sha256,plan.source_checkpoint_sha256);
  assert.equal(prior.recorded_input_sha256,plan.input_sha256);
}
const progress=await runStage({source,output,native,disk,frame,commit,
  previousFile:join(source,'c42-progress.json'),instructions:plan.stage_budgets[stage-1],maxTransportBytes:2147483648});
const result=milestone(progress,json(join(output,'runtime-evidence.json')),json(join(output,'boundary-summary.json')),json(join(output,'output-contract.json')));
verifyCampaignInputEvent(plan,stage,json(join(output,'current.json')).provenance.external_uart_input);
if(stage===1){
  assert.equal(result.instructions,plan.first_clean_boundary_instructions);
  if(plan.first_clean_boundary_sha256)assert.equal(result.checkpoint_sha256,plan.first_clean_boundary_sha256,'Independent replay differs from the complete admitted local RVG');
}
if(plan.independent_stage_checks?.[stageText]){
  const expected=plan.independent_stage_checks[stageText];
  assert.equal(result.instructions,expected.instructions);
  assert.equal(result.checkpoint_sha256,expected.checkpoint_sha256,'Independent later-stage full RVG differs from the admitted local execution');
}
writeFileSync(join(output,'c46-progress.json'),JSON.stringify(result,null,2)+'\n');
const receipt={...result,schema:'reverseandroid-boot-campaign-stage-v1',cycle:plan.cycle??'C71',stage,root_source_checkpoint_sha256:plan.source_checkpoint_sha256,
  input_checkpoint_sha256:contract.checkpoint_sha256,recorded_input_sha256:plan.input_sha256,
  original_engine_and_codecs_unchanged:true,guest_input_repeated:false,source_commit:commit,
  temporary_guest_priority_experiment:plan.input_file!==null,guest_adjustment_executed_verified:false,
  independent_whole_RVG_reference_verified:Boolean(stage===1&&plan.first_clean_boundary_sha256||plan.independent_stage_checks?.[stageText])};
writeFileSync(join(output,'campaign-progress.json'),JSON.stringify(receipt,null,2)+'\n');
if(process.env.GITHUB_OUTPUT)appendFileSync(process.env.GITHUB_OUTPUT,`ready=${result.ready_for_visual_review}\ninstructions=${result.instructions}\n`);
if(process.env.GITHUB_STEP_SUMMARY)appendFileSync(process.env.GITHUB_STEP_SUMMARY,
  `### ${plan.cycle??'C71'}, etapa ${stage}\n\nFrontera limpia: ${result.instructions}. SHA-256: \`${result.checkpoint_sha256}\`.\n\n`+
  `system_server: ${result.system_server_alive_verified}; arranque completo: ${result.android_boot_completed_verified}.\n\n`);
console.log(JSON.stringify(receipt,null,2));
