// Bounded read-only candidate decoding. No guest execution or state changes.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';

const hx=value=>'0x'+value.toString(16);
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');

export function readDebuggerdSignalCandidate({memory,futexAddress,leaderPid,observedChildTid,mapping}) {
  assert.ok(Number.isSafeInteger(leaderPid)&&leaderPid>0);
  assert.ok(Number.isSafeInteger(observedChildTid)&&observedChildTid>0&&observedChildTid<4194304);
  assert.ok(futexAddress>=4n&&futexAddress<0x1000000000000n);
  // AOSP debugger_thread_info prefix: two pid_t values, siginfo*, ucontext*.
  // This layout is a hypothesis until these independent constraints pass.
  const base=futexAddress-4n,prefix=memory.read(base,24);
  assert.equal(prefix.readInt32LE(0),leaderPid,'Candidate crashing TID does not match the selected leader');
  assert.equal(prefix.readInt32LE(4),observedChildTid,'Candidate child TID does not match the observed futex');
  const signalPointer=prefix.readBigUInt64LE(8),contextPointer=prefix.readBigUInt64LE(16);
  for(const pointer of [signalPointer,contextPointer])assert.ok(pointer>0n&&pointer<0x1000000000000n&&(pointer&7n)===0n,'Invalid bounded user pointer');
  const signal=memory.read(signalPointer,128),context=memory.read(contextPointer,456);
  const signo=signal.readInt32LE(0),code=signal.readInt32LE(8);
  assert.ok(signo>0&&signo<=64,'Invalid candidate signal number');
  // AArch64 Linux/bionic ucontext: 176-byte prefix, followed by sigcontext.
  const faultAddress=context.readBigUInt64LE(176);
  const registers=Array.from({length:31},(_,i)=>context.readBigUInt64LE(184+i*8));
  const sp=context.readBigUInt64LE(432),pc=context.readBigUInt64LE(440),pstate=context.readBigUInt64LE(448);
  assert.equal(pstate&15n,0n,'Candidate context is not EL0');
  const pcMapping=mapping(pc),spMapping=mapping(sp);
  assert.ok(pcMapping&&(BigInt(pcMapping.flags)&4n)!==0n,'Candidate PC is not in an executable user mapping');
  assert.ok(spMapping&&(BigInt(spMapping.flags)&2n)!==0n,'Candidate SP is not in a writable user mapping');
  const result={read_only:true,candidate_layout_constraints_passed:true,layout_source_version_locked:false,
    interpretation_requires_guest_build_and_signal_validation:true,
    thread_info_address:hx(base),thread_info_prefix_le_hex:prefix.toString('hex'),
    observed_crashing_tid:leaderPid,observed_pseudothread_tid:observedChildTid,
    siginfo_address:hx(signalPointer),siginfo_le_hex:signal.toString('hex'),siginfo_SHA256:sha(signal),
    signal_number:signo,signal_errno:signal.readInt32LE(4),signal_code:code,
    signal_union_first_16_bytes_le_hex:signal.subarray(16,32).toString('hex'),
    ucontext_address:hx(contextPointer),ucontext_prefix_le_hex:context.toString('hex'),ucontext_prefix_SHA256:sha(context),
    saved_fault_address:hx(faultAddress),saved_registers:registers.map(hx),saved_SP:hx(sp),saved_PC:pcMapping,saved_PSTATE:hx(pstate)};
  try{result.saved_PC_code_le_hex=memory.read(pc,64).toString('hex');}catch(error){result.saved_PC_code_unverified=error.message;}
  return result;
}
