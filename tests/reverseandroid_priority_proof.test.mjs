import test from 'node:test';
import assert from 'node:assert/strict';
import {assessPriority} from '../tools/reverseandroid-priority-proof.mjs';
const sha='a'.repeat(64),plan={expected_target:{pid:994,leader:567,name:'Jit thread pool',nice_before:19,nice_after:0}};
function sample(priority=139){return {read_only:true,input_unchanged:true,checkpoint_sha256:sha,instructions:42,groups:[{pid:567,thread_list_verified:true,threads:[{pid:994,tgid:567,name:'Jit thread pool',exit_state:0,priority,accounting:{sum_exec_runtime:'700000000'}}]}]};}
test('successful command does not turn unchanged nice +19 into a verified adjustment',()=>{
  const p=sample();p.command_exit_status=0;const r=assessPriority(p,plan,{checkpoint_sha256:sha});assert.equal(r.effective_priority_verified,false);assert.equal(r.observed_nice,19);
});
test('the matching live kernel thread at nice 0 verifies the adjustment only',()=>{
  const r=assessPriority(sample(120),plan,{checkpoint_sha256:sha});assert.equal(r.effective_priority_verified,true);assert.equal(r.android_boot_completed_verified,false);
});
test('a different snapshot or thread owner cannot supply the proof',()=>{
  assert.throws(()=>assessPriority(sample(),plan,{checkpoint_sha256:'b'.repeat(64)}));
  const p=sample(120);p.groups[0].threads[0].tgid=999;assert.throws(()=>assessPriority(p,plan,{checkpoint_sha256:sha}));
});
