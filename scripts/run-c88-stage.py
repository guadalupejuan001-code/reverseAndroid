"""One original-admitted native cycle; one guarded priority input in the entire campaign."""
import hashlib,os,pathlib,shutil,subprocess,sys
from c85_peripheral_completion import verify_live_peripherals,boot_complete
from c88_jit_priority_control import read,save,digest,parse_reply,validate_plan

n=int(sys.argv[1]);assert 1<=n<=16
p=read('state/experiments/c88-plan.json');validate_plan(p)
reports=pathlib.Path('evidence/c88-reports');state_path=reports/'control-state.json';st=read(state_path)
assert st['C88_exact_guarded_JIT_priority_campaign'] and not st['failed'] and not st['complete']
assert st['events'][:7]==p['inherited_source_events'] and st['hardware_commands_completed']
assert digest('build/ic-native/epoch-clz-ic')==p['expected_native_SHA256']
source=pathlib.Path(st['source']);c=read(source/'output-contract.json');output=pathlib.Path(f'evidence/c88-{n}')
input_plan=None
if n==1:
    assert not st['C88_input_sent'] and st['actual_C88_new_events']==0 and len(st['events'])==7
    assert c['checkpoint_sha256']==p['source_checkpoint_sha256'] and c['instructions']==p['source_instructions']
    body=pathlib.Path(p['input_file']);assert body.stat().st_size==p['input_bytes'] and digest(body)==p['input_sha256']
    input_plan=reports/'stage-1-input-plan.json'
    save(input_plan,dict(schema='reverseandroid-authenticated-console-input-plan-v1',purpose='diagnostic_console',
        input_file=os.path.relpath(body.resolve(),input_plan.parent.resolve()),input_bytes=p['input_bytes'],input_sha256=p['input_sha256'],
        source_checkpoint_sha256=c['checkpoint_sha256'],source_instructions=c['instructions'],scope=p['input_scope']))
else:
    assert st['C88_input_sent'] and st['actual_C88_new_events']==1 and len(st['events'])==8
budget=p['initial_instruction_budget'] if n==1 else p['continuation_instruction_budget']
command=['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs','run',str(source),str(output),
    'build/ic-native/epoch-clz-ic','build/media/android-c27.raw','build/reverseandroid/frame',os.environ['GITHUB_SHA'],str(budget),
    str(input_plan) if input_plan else '-']
with (reports/f'stage-{n}.json').open('x') as log:subprocess.run(command,stdout=log,check=True)
g=read(output/'c46-progress.json');host=read(output/'process-outcome.json');k=read(output/'kernel-observation.json')
assert g['source_boundary_verified'] and digest(output/'current.rvg')==g['checkpoint_sha256']==read(output/'output-contract.json')['checkpoint_sha256']
assert c['instructions']<g['instructions']<=c['instructions']+budget
assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
assert read(output/'host-policy.json')['host_wall_ms']==900000 and k['live_kernel_security']['selinux_enforcing']
provenance=read(output/'current.json')['provenance']
if n==1:
    event=provenance['external_uart_input'];assert provenance['new_uart_input_bytes']==p['input_bytes']
    assert event['instructions']==c['instructions'] and event['bytes']==p['input_bytes'] and event['sha256']==p['input_sha256']
    st['events'].append(dict(phase=7,input_plan=str(input_plan),actual_native_event=event))
    st.update(C88_input_sent=True,actual_C88_new_events=1)
else:assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
transcript=reports/'console-all.bin'
with transcript.open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
raw=transcript.read_bytes();reply=parse_reply(raw)
with (reports/f'stage-{n}-console-observer.log').open('x') as log:
    subprocess.run(['node','tools/reverseandroid-console-proof.mjs',str(transcript),p['ELF_SHA256'],p['guest_ELF_path'],str(output/'console-observation.json')],stdout=log,check=True)
console=read(output/'console-observation.json')
assert console['original_input_bytes_unchanged'] and console['raw_console_SHA256']==hashlib.sha256(raw).hexdigest()
assert not console['original_C46_or_boot_or_UI_gates_modified']
probe=output/'peripherals-live-required.json'
with (reports/f'stage-{n}-required-peripherals.log').open('x') as log:
    result=subprocess.run(['node','--max-old-space-size=5000','tools/reverseandroid-peripheral-probe.mjs',str(output/'current.rvg'),g['checkpoint_sha256'],str(probe),p['guest_ELF_path']],stdout=log)
live=read(probe) if result.returncode==0 and probe.exists() else None
hardware=verify_live_peripherals(live,console,g['checkpoint_sha256'])
if reply['guard_succeeded'] and not st.get('C88_priority_postprobe_checked'):
    path=output/'threads-priority-readonly.json'
    with (reports/f'stage-{n}-priority-readonly.log').open('x') as log:
        subprocess.run(['node','--max-old-space-size=5000','tools/reverseandroid-thread-probe.mjs',str(output/'current.rvg'),g['checkpoint_sha256'],str(path),'--all-scheduling'],stdout=log,check=True)
    observed=read(path);assert observed['input_unchanged'] and observed['checkpoint_sha256']==g['checkpoint_sha256'] and observed['kernel_layout_pinned']
    group=next(group for group in observed['groups'] if group['pid']==5629)
    assert group['thread_count']==len(group['threads']) and not group.get('error')
    targets=[t for t in group['threads'] if t['pid']==5922]
    if targets:
        t=targets[0];assert t['tgid']==5629 and t['name']=='Jit thread pool' and t['priority']==120
        assert t['scheduling']['credentials']['uid']=='0' and t['scheduling']['credentials']['euid']=='0'
    st.update(C88_priority_postprobe_checked=True,C88_priority_postprobe_checkpoint=g['checkpoint_sha256'],
        C88_target_live_nice0_verified=bool(targets),C88_target_exited_after_real_nice0_reply=not targets,
        C88_priority_postprobe_file=str(path))
st.update(source=str(output),last_admitted_sha256=g['checkpoint_sha256'],last_admitted_instructions=g['instructions'],
    failed=reply['guard_failed'],phase=8 if reply['guard_succeeded'] else 7,waiting=not reply['guard_succeeded'],
    C88_guard_reply_seen=reply['reply_seen'],C88_guard_reply_code=reply['reply_code'],C88_actual_nice0_reply_seen=reply['actual_nice0_reply_seen'],
    C88_guard_succeeded=reply['guard_succeeded'],C88_current_stage_new_UART_bytes=provenance['new_uart_input_bytes'],
    live_peripherals_verified=hardware,live_peripheral_probe_exit_code=result.returncode,
    boot_completed=g['android_boot_completed_verified'],system_server_alive=g['system_server_alive_verified'],
    combined_original_console_bytes_sha256=hashlib.sha256(raw).hexdigest(),console_observation=str(output/'console-observation.json'),
    native_binary_SHA256=p['expected_native_SHA256'],SELinux_enforcing=True,ui_response_verified=False)
st['complete']=boot_complete(reply['guard_succeeded'] and bool(st.get('C88_priority_postprobe_checked')),hardware,g)
assert st['events'][:7]==p['inherited_source_events'] and len(st['events'])==8 and st['actual_C88_new_events']==1
save(state_path,st);save(output/'c88-proof.json',st)
shutil.rmtree(output/'native/journal')
with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('complete='+str(st['complete']).lower()+'\n')
print(__import__('json').dumps(dict(stage=n,guard=reply,live_peripherals_verified=hardware,gate=g,complete=st['complete'])))
if st['failed']:raise SystemExit('The actual guarded guest command failed; preserve the diagnostic and stop')
