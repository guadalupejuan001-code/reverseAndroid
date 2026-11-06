#!/usr/bin/env python3
"""One bounded original-admitted continuation with the same native and zero UART."""
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

from c85_peripheral_completion import verify_live_peripherals,boot_complete


def read(path):return json.loads(pathlib.Path(path).read_text())
def save(path,value):pathlib.Path(path).write_text(json.dumps(value,indent=2)+'\n')
def digest(path):
    with pathlib.Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def main():
    n=int(sys.argv[1]);assert 1<=n<=16
    plan=read('state/experiments/c87-plan.json')
    reports=pathlib.Path('evidence/c87-reports');state_path=reports/'control-state.json'
    state=read(state_path);source=pathlib.Path(state['source']);contract=read(source/'output-contract.json')
    assert state['C87_same_engine_bounded_continuation'] and not state['failed'] and not state['complete']
    assert state['phase']==7 and state['waiting'] is False and state['hardware_commands_completed']
    assert state['events']==plan['inherited_source_events'] and len(state['events'])==7
    if n==1:
        assert contract['checkpoint_sha256']==plan['source_checkpoint_sha256']
        assert contract['instructions']==plan['source_instructions']
    budget=plan['initial_instruction_budget'] if n==1 else plan['continuation_instruction_budget']
    assert type(budget) is int and 0<budget<=1000000000 and plan['new_UART_input_planned'] is False
    assert digest('build/ic-native/epoch-clz-ic')==plan['expected_native_SHA256']
    output=pathlib.Path(f'evidence/c87-{n}')
    command=['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs','run',
             str(source),str(output),'build/ic-native/epoch-clz-ic','build/media/android-c27.raw',
             'build/reverseandroid/frame',os.environ['GITHUB_SHA'],str(budget),'-']
    with (reports/f'stage-{n}.json').open('x') as log:subprocess.run(command,stdout=log,check=True)
    gate=read(output/'c46-progress.json');host=read(output/'process-outcome.json');kernel=read(output/'kernel-observation.json')
    assert gate['source_boundary_verified'] and gate['checkpoint_sha256']==read(output/'output-contract.json')['checkpoint_sha256']
    assert digest(output/'current.rvg')==gate['checkpoint_sha256']
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    assert read(output/'host-policy.json')['host_wall_ms']==900000
    assert kernel['live_kernel_security']['selinux_enforcing']
    provenance=read(output/'current.json')['provenance']
    assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
    transcript=reports/'console-all.bin'
    with transcript.open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
    raw=transcript.read_bytes();observation=output/'console-observation.json'
    with (reports/f'stage-{n}-console-observer.log').open('x') as log:
        subprocess.run(['node','tools/reverseandroid-console-proof.mjs',str(transcript),plan['ELF_SHA256'],
                        plan['guest_ELF_path'],str(observation)],stdout=log,check=True)
    observed=read(observation)
    assert observed['original_input_bytes_unchanged'] and observed['raw_console_SHA256']==hashlib.sha256(raw).hexdigest()
    assert not observed['original_C46_or_boot_or_UI_gates_modified']
    probe=output/'peripherals-live-required.json'
    with (reports/f'stage-{n}-required-peripherals.log').open('x') as log:
        result=subprocess.run(['node','--max-old-space-size=5000','tools/reverseandroid-peripheral-probe.mjs',
            str(output/'current.rvg'),gate['checkpoint_sha256'],str(probe),plan['guest_ELF_path']],stdout=log)
    actual=read(probe) if result.returncode==0 and probe.exists() else None
    state.update(source=str(output),last_admitted_sha256=gate['checkpoint_sha256'],
                 last_admitted_instructions=gate['instructions'],pending_C87_new_admission=False,
                 live_peripheral_probe_exit_code=result.returncode,
                 live_peripherals_verified=verify_live_peripherals(actual,observed,gate['checkpoint_sha256']),
                 boot_completed=gate['android_boot_completed_verified'],system_server_alive=gate['system_server_alive_verified'],
                 combined_original_console_bytes_sha256=hashlib.sha256(raw).hexdigest(),
                 console_observation=str(observation),admitted_parent_primary=plan['source_artifact_lock'],
                 native_binary_SHA256=plan['expected_native_SHA256'],actual_C87_new_events=0,
                 C87_selected_native_unchanged=True,C87_no_new_UART=True,
                 C86_after_selected_parent_is_reference_after_C87_budget_selection=True,
                 original_HAL_registration_seen_in_actual_console=observed['original_HAL_Binder_found'],
                 actual_AArch64_peer_ready_line_seen=observed['actual_peer_READY_observed'],
                 actual_guest_ELF_hash_line_seen=observed['actual_exact_guest_hash_and_path_observed'],
                 actual_private_device_filesystem_mount_seen=bool(observed['actual_private_device_filesystem_mounts']),
                 actual_loaded_policy_unchanged_hashes=observed['actual_loaded_policy_unchanged_hashes'],
                 SELinux_enforcing=True,ui_response_verified=False)
    state['complete']=boot_complete(True,state['live_peripherals_verified'],gate)
    assert state['events']==plan['inherited_source_events'] and state['phase']==7 and state['waiting'] is False
    save(state_path,state);save(output/'c87-proof.json',state)
    shutil.rmtree(output/'native/journal')
    with open(os.environ['GITHUB_OUTPUT'],'a') as f:f.write('complete='+str(state['complete']).lower()+'\n')
    print(json.dumps(dict(stage=n,complete=state['complete'],gate=gate,live_peripherals_verified=state['live_peripherals_verified'])))


if __name__=='__main__':main()
