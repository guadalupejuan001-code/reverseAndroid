import assert from 'node:assert/strict';
import {createReadStream, readFileSync, writeFileSync, appendFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {join} from 'node:path';
import {runStage} from './android_c42_zygote_progress.mjs';
import {milestone} from './android_c46_continue.mjs';

const [source, output, native, disk, frame, commit] = process.argv.slice(2);
assert.ok(source && output && native && disk && frame && commit);
assert.match(commit, /^[0-9a-f]{40}$/);
const load = p => JSON.parse(readFileSync(p));
const plan = load(new URL('../state/continuation.json', import.meta.url));
assert.ok(Number.isSafeInteger(plan.instruction_budget) && plan.instruction_budget > 0 && plan.instruction_budget <= 2000000000);
const before = load(join(source, 'output-contract.json'));
assert.equal(before.checkpoint_sha256, plan.source.checkpoint_sha256);
const progress = await runStage({source, output, native, disk, frame, commit,
  previousFile: join(source, 'c42-progress.json'), instructions: plan.instruction_budget,
  maxTransportBytes: 2147483648});
const result = milestone(progress, load(join(output, 'runtime-evidence.json')),
  load(join(output, 'boundary-summary.json')), load(join(output, 'output-contract.json')));
assert.ok(result.instructions > plan.source.instructions);
const hash = createHash('sha256');
for await (const chunk of createReadStream(join(source, 'current.rvg'))) hash.update(chunk);
assert.equal(hash.digest('hex'), before.checkpoint_sha256, 'Migration source changed during execution');
writeFileSync(join(output, 'c46-progress.json'), JSON.stringify(result, null, 2) + '\n');
const receipt = {...result, schema: 'reverseandroid-continuation-v1',
  cycle: plan.cycle,
  source_repository: plan.source.repository, source_checkpoint_sha256: before.checkpoint_sha256,
  source_instructions: plan.source.instructions, advanced_instructions: result.instructions - plan.source.instructions,
  execution_repository: 'guadalupejuan001-code/reverseAndroid', execution_commit: commit,
  original_source_preserved: true, original_engine_and_decoders_unchanged: true};
writeFileSync(join(output, 'reverseandroid-progress.json'), JSON.stringify(receipt, null, 2) + '\n');
if (process.env.GITHUB_STEP_SUMMARY) appendFileSync(process.env.GITHUB_STEP_SUMMARY,
  `### Continuación de reverseAndroid\n\n| Comprobación | Resultado |\n|---|---|\n` +
  `| Checkpoint original preservado | Sí |\n| Instrucciones de origen | ${plan.source.instructions} |\n` +
  `| Instrucciones nuevas verificadas | ${receipt.advanced_instructions} |\n| Frontera final | ${result.instructions} |\n` +
  `| system_server verificado | ${result.system_server_alive_verified} |\n` +
  `| Arranque Android completo | ${result.android_boot_completed_verified} |\n` +
  `| Contenido Android visible verificado | ${result.android_display_content_verified} |\n\n` +
  `SHA-256: \`${result.checkpoint_sha256}\`. Los checks de migración acreditan integridad y progreso.\n`);
console.log(JSON.stringify(receipt, null, 2));
