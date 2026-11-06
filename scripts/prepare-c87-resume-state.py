#!/usr/bin/env python3
"""Resume the exact admitted C86 parent without any new guest input."""
import copy
import hashlib
import json
import pathlib
import sys


def read(path):
    return json.loads(pathlib.Path(path).read_text())


def sha(data):
    return hashlib.sha256(data).hexdigest()


def prepare(source, base_console, metadata_parent, reports, plan):
    source,base_console,metadata_parent,reports=map(pathlib.Path,(source,base_console,metadata_parent,reports))
    source=source.resolve()
    assert not (reports/'control-state.json').exists() and not (reports/'console-all.bin').exists()
    assert all(type(plan[key]) is int and 0<plan[key]<=1000000000 for key in
               ['initial_instruction_budget','continuation_instruction_budget'])
    assert plan['new_UART_input_planned'] is False
    contract=read(source/'output-contract.json');gate=read(source/'c46-progress.json')
    host=read(source/'process-outcome.json');kernel=read(source/'kernel-observation.json')
    original=(source/'c86-proof.json').read_bytes();state=json.loads(original)
    assert sha(original)==plan['source_control_proof_SHA256']
    assert contract['checkpoint_sha256']==gate['checkpoint_sha256']==state['last_admitted_sha256']==plan['source_checkpoint_sha256']
    assert contract['instructions']==gate['instructions']==state['last_admitted_instructions']==plan['source_instructions']
    assert contract['epoch_sha256']==plan['source_epoch_SHA256']
    with (source/'current.rvg').open('rb') as f:
        assert hashlib.file_digest(f,'sha256').hexdigest()==contract['checkpoint_sha256']
    assert gate['source_boundary_verified'] and kernel['live_kernel_security']['selinux_enforcing']
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    assert read(source/'host-policy.json')['native_binary_sha256']==state['native_binary_SHA256']==plan['expected_native_SHA256']
    assert not state['failed'] and not state['complete'] and not state['waiting']
    assert state['phase']==len(plan['inherited_phases'])==7
    assert state['hardware_commands_completed'] and state['live_peripherals_verified']
    assert state['events']==plan['inherited_source_events'] and len(state['events'])==7
    assert [e['phase'] for e in state['events']]==list(range(7))
    assert state['original_admission_and_codecs_unchanged'] and state['explicit_IC_IVAU_correction']
    assert state['IC_IVAU_validation_run']==42221035217 and not state['fresh_power_cycle']
    raw=base_console.read_bytes()
    assert sha(raw)==plan['source_base_C83_console_SHA256']
    expected=[('c84',n) for n in range(1,5)]+[('c85',n) for n in range(1,4)]+[('c86',n) for n in range(1,plan['source_stage']+1)]
    assert [(i['cycle'],i['stage']) for i in plan['source_console_stages']]==expected
    for item in plan['source_console_stages']:
        b=metadata_parent/item['artifact_alias']/'evidence'/f"{item['cycle']}-{item['stage']}"
        proof=(b/f"{item['cycle']}-proof.json").read_bytes();part=(b/'console-new.txt').read_bytes()
        assert sha(proof)==item['control_proof_SHA256'] and sha(part)==item['new_console_SHA256']
        assert (b/'source-commit.txt').read_text().strip()==item['execution_commit']
        assert read(b/'c46-progress.json')['source_boundary_verified']
        raw+=part
        assert sha(raw)==json.loads(proof)['combined_original_console_bytes_sha256']==item['combined_console_SHA256']
    assert len(raw)==plan['source_combined_console_bytes']
    assert sha(raw)==plan['source_combined_console_SHA256']==state['combined_original_console_bytes_sha256']
    resumed=copy.deepcopy(state)
    resumed.update(source=str(source),C87_same_engine_bounded_continuation=True,
                   C87_source_control_proof_SHA256=plan['source_control_proof_SHA256'],
                   C86_after_selected_parent_is_reference_after_C87_budget_selection=True,
                   pending_C87_new_admission=True,actual_C87_new_events=0)
    assert resumed['phase']==state['phase'] and resumed['waiting'] is False and resumed['events']==state['events']
    reports.mkdir(parents=True,exist_ok=True)
    for name,data in [('source-c86-proof.json',original),('console-all.bin',raw),
                      ('control-state.json',(json.dumps(resumed,indent=2)+'\n').encode())]:
        with (reports/name).open('xb') as f:f.write(data)
    result=dict(schema='reverseandroid-C87-exact-bounded-continuation-resume-v1',
                source_checkpoint_sha256=contract['checkpoint_sha256'],instructions=contract['instructions'],
                original_proof_bytes_preserved=True,original_events_preserved=7,phase=7,waiting=False,
                entire_console_SHA256=sha(raw),new_UART_input_bytes=0,snapshot_bytes_unchanged=True,
                native_binary_unchanged=True,guest_clock_seed_RAM_disk_IRQ_and_codecs_unchanged=True,
                only_future_host_instruction_budget_reduced=True,new_native_execution_not_yet_claimed=True)
    (reports/'resume-proof.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


if __name__=='__main__':
    assert len(sys.argv)==6,'Expected SOURCE BASE_C83_CONSOLE METADATA_PARENT REPORTS PLAN'
    print(json.dumps(prepare(*sys.argv[1:5],read(sys.argv[5]))))
