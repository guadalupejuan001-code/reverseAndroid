"""A full original-admitted native boundary, with zero UART in every stage."""
import hashlib,os,pathlib,shutil,subprocess,sys
from c88_jit_priority_control import read,save,digest,parse_reply
from zero_input_continuation import validate
from c85_peripheral_completion import verify_live_peripherals,boot_complete

plan_path=pathlib.Path(sys.argv[1]);n=int(sys.argv[2]);p=read(plan_path);validate(p);assert 1<=n<=p['max_stages']
cycle=p['cycle'];reports=pathlib.Path(f'evidence/{cycle}-reports');state_path=reports/'control-state.json';st=read(state_path)
assert st['zero_input_continuation_campaign']==cycle and not st['failed'] and not st['complete']
assert st['events']==p['inherited_source_events'] and st['phase']==len(st['events']) and st['waiting'] is False
assert st['hardware_commands_completed'] and st['actual_campaign_new_events']==0
assert digest('build/ic-native/epoch-clz-ic')==p['expected_native_SHA256']
source=pathlib.Path(st['source']);c=read(source/'output-contract.json');output=pathlib.Path(f'evidence/{cycle}-{n}')
if n==1:assert c['checkpoint_sha256']==p['source_checkpoint_sha256'] and c['instructions']==p['source_instructions']
budget=p['initial_instruction_budget'] if n==1 else p['continuation_instruction_budget']
command=['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs','run',str(source),str(output),
    'build/ic-native/epoch-clz-ic','build/media/android-c27.raw','build/reverseandroid/frame',os.environ['GITHUB_SHA'],str(budget),'-']
with (reports/f'stage-{n}.json').open('x') as log:subprocess.run(command,stdout=log,check=True)
g=read(output/'c46-progress.json');host=read(output/'process-outcome.json');k=read(output/'kernel-observation.json')
assert g['source_boundary_verified'] and digest(output/'current.rvg')==g['checkpoint_sha256']==read(output/'output-contract.json')['checkpoint_sha256']
assert c['instructions']<g['instructions']<=c['instructions']+budget
assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
assert read(output/'host-policy.json')['host_wall_ms']==900000 and k['live_kernel_security']['selinux_enforcing']
provenance=read(output/'current.json')['provenance'];assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
transcript=reports/'console-all.bin';assert hashlib.sha256(transcript.read_bytes()).hexdigest()==st['combined_original_console_bytes_sha256']
with transcript.open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
raw=transcript.read_bytes();assert parse_reply(raw)['guard_succeeded']
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
st.update(source=str(output),last_admitted_sha256=g['checkpoint_sha256'],last_admitted_instructions=g['instructions'],
    zero_input_continuation_campaign=cycle,actual_campaign_new_events=0,current_stage_new_UART_bytes=0,
    live_peripherals_verified=hardware,live_peripheral_probe_exit_code=result.returncode,
    boot_completed=g['android_boot_completed_verified'],system_server_alive=g['system_server_alive_verified'],
    combined_original_console_bytes_sha256=hashlib.sha256(raw).hexdigest(),console_observation=str(output/'console-observation.json'),
    native_binary_SHA256=p['expected_native_SHA256'],selected_native_unchanged_from_parent=True,SELinux_enforcing=True)
st['complete']=boot_complete(True,hardware,g)
assert st['events']==p['inherited_source_events'] and st['phase']==len(st['events']) and st['waiting'] is False
save(state_path,st);save(output/'zero-input-proof.json',st)
shutil.rmtree(output/'native/journal')
with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('complete='+str(st['complete']).lower()+'\n')
print(__import__('json').dumps(dict(cycle=cycle,stage=n,new_UART_bytes=0,original_gate=g,live_peripherals_verified=hardware,
    complete_original_BOOT_and_peripherals=st['complete'],actual_UI_response_verified=False)))
