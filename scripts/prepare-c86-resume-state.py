#!/usr/bin/env python3
"""Bind one guarded cleanup-priority input to the latest admitted C85 source."""
import copy
import hashlib
import json
import pathlib
import sys

from c86_cleanup_eligibility import verify


def read(path):
    return json.loads(pathlib.Path(path).read_text())


def sha(data):
    return hashlib.sha256(data).hexdigest()


def prepare(source, base_console, metadata_parent, reports, plan, threads_path):
    source,base_console,metadata_parent,reports,threads_path=map(pathlib.Path,(source,base_console,metadata_parent,reports,threads_path))
    source=source.resolve()
    assert not (reports/'control-state.json').exists() and not (reports/'console-all.bin').exists()
    original=(source/'c85-proof.json').read_bytes()
    assert sha(original)==plan['source_control_proof_SHA256']
    state=json.loads(original)
    contract=read(source/'output-contract.json')
    with (source/'current.rvg').open('rb') as f:
        assert hashlib.file_digest(f,'sha256').hexdigest()==plan['source_checkpoint_sha256']
    assert read(source/'host-policy.json')['native_binary_sha256']==plan['expected_extended_native_SHA256']
    assert sha(threads_path.read_bytes())==plan['source_thread_proof_SHA256']
    eligibility=verify(contract,read(source/'c46-progress.json'),read(source/'process-outcome.json'),
                       read(source/'kernel-observation.json'),state,read(threads_path),plan)
    raw=base_console.read_bytes()
    assert sha(raw)==plan['source_base_C83_console_SHA256']
    for item in plan['source_console_stages']:
        directory=metadata_parent/item['artifact_alias']/'evidence'/f"{item['cycle']}-{item['stage']}"
        proof=(directory/f"{item['cycle']}-proof.json").read_bytes()
        part=(directory/'console-new.txt').read_bytes()
        assert sha(proof)==item['control_proof_SHA256'] and sha(part)==item['new_console_SHA256']
        assert (directory/'source-commit.txt').read_text().strip()==item['execution_commit']
        assert read(directory/'c46-progress.json')['source_boundary_verified']
        raw+=part
        assert sha(raw)==json.loads(proof)['combined_original_console_bytes_sha256']==item['combined_console_SHA256']
    assert len(raw)==plan['source_combined_console_bytes']
    assert sha(raw)==plan['source_combined_console_SHA256']==state['combined_original_console_bytes_sha256']
    resumed=copy.deepcopy(state)
    resumed.update(source=str(source),C86_explicit_cleanup_priority_continuation=True,
                   C86_source_control_proof_SHA256=plan['source_control_proof_SHA256'],
                   C85_after_stage3_is_reference_after_explicit_C86_input_selection=True,
                   pending_C86_input_admission=True,actual_C86_new_events=0)
    assert resumed['events']==state['events'] and resumed['phase']==state['phase'] and resumed['waiting'] is False
    reports.mkdir(parents=True,exist_ok=True)
    for name,data in [('source-c85-proof.json',original),('console-all.bin',raw),
                      ('control-state.json',(json.dumps(resumed,indent=2)+'\n').encode())]:
        with (reports/name).open('xb') as f:
            f.write(data)
    eligibility.update(schema='reverseandroid-C86-exact-source-cleanup-eligibility-v1',
                       original_proof_bytes_preserved=True,new_UART_input_bytes=0,
                       input_not_yet_executed=True,snapshot_bytes_unchanged=True)
    (reports/'resume-proof.json').write_text(json.dumps(eligibility,indent=2)+'\n')
    return eligibility


if __name__=='__main__':
    assert len(sys.argv)==7,'Expected SOURCE BASE_C83_CONSOLE METADATA_PARENT REPORTS PLAN THREADS'
    print(json.dumps(prepare(*sys.argv[1:5],read(sys.argv[5]),sys.argv[6])))
