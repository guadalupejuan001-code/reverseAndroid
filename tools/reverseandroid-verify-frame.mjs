// Compare our locally rebuilt original decoder with the authenticated source export.
import assert from 'node:assert/strict';
import {readFileSync, writeFileSync} from 'node:fs';
const [source, candidate, output] = process.argv.slice(2);
const a = JSON.parse(readFileSync(source)), b = JSON.parse(readFileSync(candidate));
for (const x of [a,b]) {
  assert.equal(x.status, 'scanout_exported');
  assert.equal(x.cpu_instructions_executed, 0);
  assert.equal(x.original_snapshot_unchanged, true);
}
for (const key of ['source_checkpoint_sha256','instructions','pc','width','height','rgb_sha256','ppm_sha256'])
  assert.equal(b[key], a[key], key);
writeFileSync(output, JSON.stringify({original_decoder_rebuilt_from_verified_source: true,
  full_snapshot_unchanged: true, cpu_instructions_executed: 0,
  source_checkpoint_sha256: b.source_checkpoint_sha256, matching_rgb_sha256: b.rgb_sha256,
  matching_ppm_sha256: b.ppm_sha256, android_display_content_verified: false}, null, 2) + '\n');
