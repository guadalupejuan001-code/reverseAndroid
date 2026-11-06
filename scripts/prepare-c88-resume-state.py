"""Authenticate the latest selected C87 source, never replaying its seven old inputs."""
import base64,gzip,hashlib,io,pathlib,subprocess,sys
from c88_jit_priority_control import read,save,digest,parse_reply,validate_plan

source,reports=map(pathlib.Path,sys.argv[1:]);p=read('state/experiments/c88-plan.json');validate_plan(p)
c=read(source/'output-contract.json');g=read(source/'c46-progress.json');k=read(source/'kernel-observation.json')
st=read(source/p['source_proof_file']);pf=read(reports/'source-original-admission.json')
assert c==pf['contract'] and c['checkpoint_sha256']==g['checkpoint_sha256']==p['source_checkpoint_sha256']
assert c['instructions']==g['instructions']==p['source_instructions'] and g['source_boundary_verified']
assert digest(source/p['source_proof_file'])==p['source_proof_SHA256']
assert digest(source/'current.rvg')==g['checkpoint_sha256']
assert (source/'source-commit.txt').read_text().strip()==p['source_execution_commit']
assert k['kernel_sha256']==p['source_kernel_SHA256'] and k['live_kernel_security']['selinux_enforcing']
main=next(t for t in k['tasks'] if t['pid']==p['source_main_PID'])
assert main['state']==8193 and main['accounting']==p['source_main_accounting'] and main['exit_state']==0
assert main['zygote_startup']['argv_verified'] and main['zygote_startup']['exe_path']=='/system/bin/app_process64'
assert st['events']==p['inherited_source_events'] and st['phase']==7 and st['waiting'] is False
assert st['hardware_commands_completed'] and st['live_peripherals_verified'] and not st['failed']
assert st['native_binary_SHA256']==p['expected_native_SHA256'] and st['actual_C87_new_events']==0
assert pf['report']['provenance']['new_uart_input_bytes']==0 and 'external_uart_input' not in pf['report']['provenance']
assert digest('build/ic-native/epoch-clz-ic')==p['expected_native_SHA256']
for directory,hardware in [('build/ic-native',False),('build/ic-hardware',True)]:
    d=pathlib.Path(directory);assert (d/'execution-commit.txt').read_text().strip()==p['validated_IC_IVAU_commit']
    tests=read(d/'ic-ivau-test.json');assert tests['IC_IVAU_cases']==2048 and tests['real_AArch64_hardware_cases']==int(hardware)
    assert all(tests[key] for key in ['all_Rt_and_unaligned_offsets','UCI_traps_exact_SYS_syndrome','neighbor_privileged_SYS_still_rejected','original_snapshot_roundtrip_and_guest_JIT_response'])
    clz=read(d/'retained-clz-test.json');assert clz['CLZ_cases']==30887 and clz['real_AArch64_hardware_cases']==(30887 if hardware else 0)
    assert read(d/'baseline-IC-refusal.json')['actual_exact_IC_IVAU_erroneously_undefined_with_UCI_enabled']
    assert read(d/'prior-el0-cache-test.json')['checks']==82 and read(d/'prior-system-test.json')['checks']==1992
    m=read(d/'extension-manifest.json');assert m['extension_SHA256']==p['IC_IVAU_extension_SHA256'] and m['all_870_imported_files_unchanged']
    assert m['snapshot_codecs_RAM_disk_clock_seed_cadence_IRQ_and_security_gates_unchanged']
    assert m['retained_CLZ_extension']['extension_SHA256']==p['extension_SHA256']
assert digest('extensions/aarch64-el0-ic-ivau.inc')==p['IC_IVAU_extension_SHA256']
for old in p['inherited_phases']:
    f=pathlib.Path(old['input_file']);assert f.stat().st_size==old['input_bytes']<=4096 and digest(f)==old['input_sha256']
body=pathlib.Path(p['input_file']);assert body.stat().st_size==p['input_bytes']<=4096 and digest(body)==p['input_sha256']
subprocess.run(['sh','-n'],input=body.read_bytes(),check=True)
encoded=pathlib.Path(p['source_console_file']).read_bytes();assert hashlib.sha256(encoded).hexdigest()==p['source_console_base64_SHA256']
compressed=base64.b64decode(encoded.strip(),validate=True);assert hashlib.sha256(compressed).hexdigest()==p['source_console_gzip_SHA256']
assert 0<p['source_console_bytes']<=64*1024**2
with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
    raw=stream.read(p['source_console_bytes']+1);assert not stream.read(1)
assert len(raw)==p['source_console_bytes'] and hashlib.sha256(raw).hexdigest()==p['source_console_SHA256']==st['combined_original_console_bytes_sha256']
assert not parse_reply(raw)['reply_seen'] and not parse_reply(raw)['actual_nice0_reply_seen']
reports.mkdir(parents=True,exist_ok=True);(reports/'console-all.bin').write_bytes(raw)
(reports/'source-c87-proof.json').write_bytes((source/p['source_proof_file']).read_bytes())
state=dict(st);state.update(source=str(source),complete=False,failed=False,C88_exact_guarded_JIT_priority_campaign=True,
    C88_input_sent=False,actual_C88_new_events=0,C88_guard_reply_seen=False,C88_actual_nice0_reply_seen=False,C88_guard_succeeded=False,
    source_C87_proof_SHA256=p['source_proof_SHA256'],C87_after_selected_source_is_reference_after_C88_input_selection=True,
    current_native_SHA256=p['expected_native_SHA256'],ui_response_verified=False)
save(reports/'control-state.json',state)
save(reports/'resume-proof.json',dict(source_checkpoint_sha256=c['checkpoint_sha256'],source_instructions=c['instructions'],
    original_epoch_preflight_passed=True,whole_RVG_SHA256_verified=True,all_old_seven_events_preserved=True,old_UART_replayed=False,
    entire_original_console_SHA256=p['source_console_SHA256'],same_validated_native_SHA256=p['expected_native_SHA256'],
    new_guarded_input_planned=True,new_guest_execution_started=False,original_BOOT_and_UI_not_yet_claimed=True))
print('Original C87 source admitted; one explicit guarded JIT priority input prepared, no guest execution yet')
