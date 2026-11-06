import test from 'node:test';
import assert from 'node:assert/strict';
import {observeSystemServerDiagnostic as observe} from '../tools/reverseandroid-diagnostic-console.mjs';
const run=s=>observe(Buffer.from(s));
const begin='REVERSE_PTS_SS_DIAG_BEGIN\r\n';
const end='REVERSE_PTS_SS_DIAG RC_0\r\n';
test('require actual ordered begin and successful completion, including kernel-interrupted output',()=>{
  assert.equal(run(begin+end).diagnostic_completed,true);
  assert.equal(run(begin+'REVERSE_PTS_SS_[ 1.123][ T1] init: log\r\nDIAG RC_0\r\n').diagnostic_completed,true);
  for(const raw of [begin,end,end+begin,"printf '"+begin.trim()+"'\n",begin+"$ "+end])assert.equal(run(raw).diagnostic_completed,false);
});
test('a real nonzero response prevents success; echoed or kernel-only output does not count',()=>{
  assert.equal(run(begin+'REVERSE_PTS_SS_DIAG RC_1\n'+end).diagnostic_failed,true);
  assert.equal(run(begin+'REVERSE_PTS_SS_DIAG RC_1\n'+end).diagnostic_completed,false);
  assert.equal(run('[ 1.23][ T9] REVERSE_PTS_SS_DIAG RC_0\n').diagnostic_completed,false);
});
