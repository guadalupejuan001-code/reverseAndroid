"""One authenticated real UI gesture, followed only by original zero-input continuation."""
import hashlib,os,pathlib,shutil,subprocess,sys
from c88_jit_priority_control import read,save,digest
from c85_peripheral_completion import verify_live_peripherals,boot_complete
from ui_response_control import parse_ui_reply,frame_difference

plan_path=pathlib.Path(sys.argv[1]);n=int(sys.argv[2]);p=read(plan_path)
assert p['schema']=='reverseandroid-real-ui-response-campaign-plan-v1' and 1<=n<=p['max_stages']==8
assert p['host_wall_ms']==900000
reports=pathlib.Path('evidence/ui-response-reports');state_path=reports/'control-state.json';st=read(state_path)
assert st['UI_exact_response_campaign'] and not st['failed'] and not st['UI_native_gesture_and_frame_ready']
assert st['events'][:8]==p['inherited_source_events'] and st['hardware_commands_completed']
assert digest('build/ic-native/epoch-clz-ic')==p['expected_native_SHA256']
source=pathlib.Path(st['source']);c=read(source/'output-contract.json');output=pathlib.Path(f'evidence/ui-response-{n}')
if n==1:
    assert not st['UI_input_sent'] and st['actual_UI_new_events']==0 and len(st['events'])==8
    assert c['checkpoint_sha256']==p['source_checkpoint_sha256'] and c['instructions']==p['source_instructions']
    input_plan=pathlib.Path(p['input_plan_file'])
    actual_plan=read(input_plan);body=(input_plan.parent/actual_plan['input_file']).resolve()
    assert actual_plan['purpose']=='ui_response_test' and digest(body)==actual_plan['input_sha256']==p['input_sha256']
else:
    assert st['UI_input_sent'] and st['actual_UI_new_events']==1 and len(st['events'])==9
    input_plan=None
budget=p['initial_instruction_budget'] if n==1 else p['continuation_instruction_budget'];assert 0<budget<=250000000
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
    st['events'].append(dict(phase=8,input_plan=str(input_plan),actual_native_event=event))
    st.update(UI_input_sent=True,actual_UI_new_events=1)
else:assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
transcript=reports/'console-all.bin'
assert hashlib.sha256(transcript.read_bytes()).hexdigest()==st['combined_original_console_bytes_sha256']
with transcript.open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
raw=transcript.read_bytes();reply=parse_ui_reply(raw)
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
after=read(output/'checkpoint-frame.json');before=read(reports/'before-frame.json')
if after['status']=='scanout_exported':
    assert after['source_checkpoint_sha256']==g['checkpoint_sha256']
    assert after['instructions']==g['instructions'] and after['original_snapshot_unchanged']
    difference=frame_difference((reports/'before-frame.ppm').read_bytes(),before,(output/'checkpoint-frame.ppm').read_bytes(),after)
else:
    difference=dict(after_frame_status=after['status'],authentic_frame_changed=False,
        meaningful_UI_response_manually_verified=False)
save(output/'ui-frame-difference.json',difference)
ready=boot_complete(reply['command_succeeded'],hardware,g) and difference['authentic_frame_changed']
st.update(source=str(output),last_admitted_sha256=g['checkpoint_sha256'],last_admitted_instructions=g['instructions'],
    complete=False,failed=reply['command_failed'],phase=9 if reply['command_succeeded'] else 8,waiting=not reply['command_succeeded'],
    UI_reply_seen=reply['reply_seen'],UI_reply_code=reply['reply_code'],UI_command_succeeded=reply['command_succeeded'],
    UI_stage_native_new_UART_bytes=provenance['new_uart_input_bytes'],UI_native_gesture_and_frame_ready=ready,
    UI_meaningful_after_frame_manually_reviewed=False,ui_response_verified=False,live_peripherals_verified=hardware,
    live_peripheral_probe_exit_code=result.returncode,boot_completed=g['android_boot_completed_verified'],
    system_server_alive=g['system_server_alive_verified'],combined_original_console_bytes_sha256=hashlib.sha256(raw).hexdigest(),
    console_observation=str(output/'console-observation.json'),native_binary_SHA256=p['expected_native_SHA256'],
    SELinux_enforcing=True,UI_frame_difference=difference)
assert st['events'][:8]==p['inherited_source_events'] and len(st['events'])==9 and st['actual_UI_new_events']==1
save(state_path,st);save(output/'ui-response-proof.json',st)
shutil.rmtree(output/'native/journal')
with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('ready='+str(ready).lower()+'\n')
print(__import__('json').dumps(dict(stage=n,reply=reply,original_gate=g,live_peripherals_verified=hardware,
    authentic_frame_difference=difference,ready_for_actual_after_frame_review=ready,actual_UI_response_verified=False)))
if st['failed']:raise SystemExit('The actual UI command failed; preserve the diagnostic and stop')
