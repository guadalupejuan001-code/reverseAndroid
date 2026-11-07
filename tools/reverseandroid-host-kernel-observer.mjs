// Use the unchanged original observer export with an explicit larger host bound.
import './reverseandroid-host-transport-preload.mjs';
import {observeAndroidKernel} from './android_kernel_observer.mjs';
import {readFileSync,statSync} from 'node:fs';
import {HOST_RVG_CAP} from './reverseandroid-host-rvg-reader.mjs';
import assert from 'node:assert/strict';
try{
  const [path,sha,capacity,...flags]=process.argv.slice(2);
  assert.ok(path&&/^[0-9a-f]{64}$/.test(sha));assert.equal(Number(capacity),HOST_RVG_CAP);
  assert.ok(flags.every(f=>['--live-properties','--binder-probe','--scheduler-probe','--zygote-probe'].includes(f)));
  assert.ok(statSync(path).size<=HOST_RVG_CAP);
  console.log(JSON.stringify(observeAndroidKernel(readFileSync(path),sha,{maxSnapshotBytes:HOST_RVG_CAP,
    liveProperties:flags.includes('--live-properties'),binderDebug:flags.includes('--binder-probe'),
    schedulerProbe:flags.includes('--scheduler-probe'),zygoteProbe:flags.includes('--zygote-probe')}),null,2));
}catch(e){console.error(e.stack);process.exitCode=1;}
