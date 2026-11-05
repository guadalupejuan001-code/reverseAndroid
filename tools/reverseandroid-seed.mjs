// Repository migration only; original admission remains the authority.
import assert from 'node:assert/strict';
import {readFileSync, writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {preflight} from './android_c27_profile_resume.mjs';

const [source, receipt] = process.argv.slice(2);
assert.ok(source && receipt, 'Expected source directory and receipt path');
const expected = JSON.parse(readFileSync(new URL('../state/continuation.json', import.meta.url)));
const contract = JSON.parse(readFileSync(join(source, 'output-contract.json')));
assert.equal(contract.checkpoint_sha256, expected.source.checkpoint_sha256);
assert.equal(contract.source_commit, expected.source.source_commit);
const proof = preflight(contract, source, {maxTransportBytes: 2147483648});
assert.equal(proof.report.guest.instructions, expected.source.instructions);
const previous = JSON.parse(readFileSync(join(source, 'c42-progress.json')));
assert.equal(previous.checkpoint_sha256, contract.checkpoint_sha256);
assert.equal(previous.instructions, proof.report.guest.instructions);
const result = {schema: 'reverseandroid-migration-admission-v1',
  source_repository: expected.source.repository, source_commit: contract.source_commit,
  checkpoint_sha256: contract.checkpoint_sha256, instructions: previous.instructions,
  full_original_admission_passed: true, source_state_modified: false,
  system_server_alive_verified: previous.system_server_alive_verified === true,
  android_boot_completed_verified: false, android_display_content_verified: false};
writeFileSync(receipt, JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result));
