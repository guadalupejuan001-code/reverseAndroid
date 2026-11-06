"""Authenticate a complete original source and retain every previous console/input byte."""
import base64,gzip,hashlib,io,pathlib,sys
from c88_jit_priority_control import read,save,digest,parse_reply
from zero_input_continuation import validate
from c85_peripheral_completion import verify_live_peripherals

plan_path,source,reports=map(pathlib.Path,sys.argv[1:]);p=read(plan_path);validate(p)
c=read(source/'output-contract.json');g=read(source/'c46-progress.json');k=read(source/'kernel-observation.json')
pf=read(reports/'source-original-admission.json');st=read(source/p['source_proof_file'])
assert c==pf['contract'] and c['checkpoint_sha256']==g['checkpoint_sha256']==p['source_checkpoint_sha256']
assert c['instructions']==g['instructions']==p['source_instructions'] and g['source_boundary_verified']
assert digest(source/'current.rvg')==g['checkpoint_sha256']
assert digest(source/p['source_proof_file'])==p['source_proof_SHA256']
assert (source/'source-commit.txt').read_text().strip()==p['source_execution_commit']
assert c['epoch_sha256']=='bc5e6f91cc5befb13ba8f8e96ffb74cad22edda25586324a0f469e1b26fd5e75'
assert k['kernel_sha256']=='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8'
assert k['live_kernel_security']['selinux_enforcing'] and k['input_unchanged']
assert st['events']==p['inherited_source_events'] and st['phase']==len(st['events']) and st['waiting'] is False
assert st['hardware_commands_completed'] and not st['failed']
assert st['C88_guard_succeeded'] and st['C88_priority_postprobe_checked']
assert st['native_binary_SHA256']==p['expected_native_SHA256']==digest('build/ic-native/epoch-clz-ic')
encoded=pathlib.Path(p['source_console_file']).read_bytes();assert hashlib.sha256(encoded).hexdigest()==p['source_console_base64_SHA256']
compressed=base64.b64decode(encoded.strip(),validate=True);assert hashlib.sha256(compressed).hexdigest()==p['source_console_gzip_SHA256']
assert 0<p['source_console_bytes']<=64*1024**2
with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
    raw=stream.read(p['source_console_bytes']+1);assert not stream.read(1)
assert len(raw)==p['source_console_bytes'] and hashlib.sha256(raw).hexdigest()==p['source_console_SHA256']==st['combined_original_console_bytes_sha256']
assert parse_reply(raw)['guard_succeeded']
console=read(source/'console-observation.json');assert console['raw_console_SHA256']==p['source_console_SHA256']
live=read(source/'peripherals-live-required.json') if (source/'peripherals-live-required.json').exists() else None
assert verify_live_peripherals(live,console,c['checkpoint_sha256'])==st['live_peripherals_verified']
reports.mkdir(parents=True,exist_ok=True);(reports/'console-all.bin').write_bytes(raw)
(reports/'source-original-campaign-proof.json').write_bytes((source/p['source_proof_file']).read_bytes())
state=dict(st);state.update(source=str(source),complete=False,failed=False,
    zero_input_continuation_campaign=p['cycle'],actual_campaign_new_events=0,
    selected_native_unchanged_from_parent=True,original_parent_proof_SHA256=p['source_proof_SHA256'])
save(reports/'control-state.json',state)
save(reports/'resume-proof.json',dict(source_checkpoint_sha256=c['checkpoint_sha256'],source_instructions=c['instructions'],
    original_epoch_preflight_passed=True,whole_RVG_SHA256_verified=True,all_prior_events_preserved=True,
    prior_console_bytes_preserved=True,new_UART_bytes=0,selected_native_SHA256=p['expected_native_SHA256'],
    new_guest_execution_started=False,actual_UI_response_verified=False))
print('Original source admitted for bounded continuation with zero new UART; all previous history retained')
