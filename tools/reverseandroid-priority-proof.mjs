import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

export function assessPriority(probe,plan,contract){
  assert.equal(probe.read_only,true);assert.equal(probe.input_unchanged,true);
  assert.equal(probe.checkpoint_sha256,contract.checkpoint_sha256,'Priority observation belongs to another snapshot');
  assert.ok(Number.isSafeInteger(probe.instructions)&&probe.instructions>0);
  const target=plan.expected_target,group=probe.groups.find(x=>x.pid===target.leader);
  const out={schema:'reverseandroid-effective-guest-priority-v1',checkpoint_sha256:probe.checkpoint_sha256,
    instructions:probe.instructions,target_pid:target.pid,owner_pid:target.leader,
    requested_nice:target.nice_after,source_nice:target.nice_before,read_only:true,input_unchanged:true,
    effective_priority_verified:false,android_boot_completed_verified:false};
  if(!group)return {...out,reason:'original_target_group_no_longer_observed'};
  assert.equal(group.thread_list_verified,true,'Unverified thread ownership');
  const thread=group.threads.find(t=>t.pid===target.pid);
  if(!thread)return {...out,reason:'original_target_thread_no_longer_observed'};
  assert.equal(thread.tgid,target.leader);assert.equal(thread.name,target.name);
  assert.equal(thread.exit_state,0,'Target thread has exited');
  assert.ok(Number.isInteger(thread.priority)&&thread.priority>=100&&thread.priority<=139);
  const nice=thread.priority-120;
  return {...out,observed_nice:nice,observed_static_priority:thread.priority,
    thread_runtime_ns:thread.accounting.sum_exec_runtime,
    effective_priority_verified:nice===target.nice_after,
    reason:nice===target.nice_after?'actual_kernel_niceness_matches_request':'actual_kernel_niceness_does_not_match_request'};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  const [probePath,planPath,contractPath,output]=process.argv.slice(2);assert.ok(output);
  const json=p=>JSON.parse(readFileSync(p));const result=assessPriority(json(probePath),json(planPath),json(contractPath));
  writeFileSync(output,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
}
