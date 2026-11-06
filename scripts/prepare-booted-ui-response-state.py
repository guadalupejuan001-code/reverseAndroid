"""Admit a genuinely booted, visually reviewed source before one real UI input."""
import base64,gzip,hashlib,io,pathlib,subprocess,sys
from c88_jit_priority_control import read,save,digest,parse_reply
from c85_peripheral_completion import verify_live_peripherals
from ui_response_control import parse_ui_reply,native_pixels

plan_path,source,reports=map(pathlib.Path,sys.argv[1:]);p=read(plan_path)
assert p['schema']=='reverseandroid-real-ui-response-campaign-plan-v1'
assert p['host_wall_ms']==900000 and p['max_stages']==8
assert 0<p['initial_instruction_budget']<=250000000 and 0<p['continuation_instruction_budget']<=250000000
assert p['expected_native_SHA256']=='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
assert len(p['inherited_source_events'])==8 and [e['phase'] for e in p['inherited_source_events']]==list(range(8))
c=read(source/'output-contract.json');g=read(source/'c46-progress.json');k=read(source/'kernel-observation.json')
pf=read(reports/'source-original-admission.json');st=read(source/p['source_proof_file'])
assert c==pf['contract'] and c['checkpoint_sha256']==g['checkpoint_sha256']==p['source_checkpoint_sha256']
assert c['instructions']==g['instructions']==p['source_instructions']
assert g['source_boundary_verified'] and g['android_boot_completed_verified'] and g['system_server_alive_verified']
assert digest(source/'current.rvg')==p['source_checkpoint_sha256']
assert digest(source/p['source_proof_file'])==p['source_proof_SHA256']
assert (source/'source-commit.txt').read_text().strip()==p['source_execution_commit']
assert k['kernel_sha256']==p['kernel_SHA256'] and k['live_kernel_security']['selinux_enforcing'] and k['input_unchanged']
assert st['events']==p['inherited_source_events'] and st['phase']==8 and st['waiting'] is False and not st['failed']
assert st['hardware_commands_completed'] and st['C88_guard_succeeded'] and st['C88_priority_postprobe_checked']
assert st['native_binary_SHA256']==p['expected_native_SHA256']==digest('build/ic-native/epoch-clz-ic')
frame=read(source/'checkpoint-frame.json');ppm=(source/'checkpoint-frame.ppm').read_bytes()
assert frame['status']=='scanout_exported' and frame['original_snapshot_unchanged']
assert frame['source_checkpoint_sha256']==c['checkpoint_sha256'] and frame['instructions']==c['instructions']
assert frame['ppm_sha256']==p['before_frame_ppm_SHA256'] and frame['rgb_sha256']==p['before_frame_RGB_SHA256']
native_pixels(ppm,frame)
input_file=pathlib.Path(p['input_plan_file']);planned=read(input_file)
assert planned['schema']=='reverseandroid-authenticated-console-input-plan-v1' and planned['purpose']=='ui_response_test'
assert planned['source_checkpoint_sha256']==c['checkpoint_sha256'] and planned['source_instructions']==c['instructions']
assert planned['before_frame_ppm_sha256']==frame['ppm_sha256'] and planned['before_usable_UI_manually_reviewed'] is True
assert planned['requires_actual_after_frame_review'] and not planned['actual_UI_input_or_response_claimed']
body=(input_file.parent/planned['input_file']).resolve()
assert 0<body.stat().st_size==planned['input_bytes']==p['input_bytes']<=4096
assert digest(body)==planned['input_sha256']==p['input_sha256']
subprocess.run(['sh','-n'],input=body.read_bytes(),check=True)
encoded=pathlib.Path(p['source_console_file']).read_bytes();assert hashlib.sha256(encoded).hexdigest()==p['source_console_base64_SHA256']
compressed=base64.b64decode(encoded.strip(),validate=True);assert hashlib.sha256(compressed).hexdigest()==p['source_console_gzip_SHA256']
assert 0<p['source_console_bytes']<=64*1024**2
with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
    raw=stream.read(p['source_console_bytes']+1);assert not stream.read(1)
assert len(raw)==p['source_console_bytes'] and hashlib.sha256(raw).hexdigest()==p['source_console_SHA256']==st['combined_original_console_bytes_sha256']
assert parse_reply(raw)['guard_succeeded'] and not parse_ui_reply(raw)['reply_seen']
console=read(source/'console-observation.json');assert console['raw_console_SHA256']==p['source_console_SHA256']
assert verify_live_peripherals(read(source/'peripherals-live-required.json'),console,c['checkpoint_sha256'])
reports.mkdir(parents=True,exist_ok=True);(reports/'console-all.bin').write_bytes(raw)
(reports/'before-frame.ppm').write_bytes(ppm);save(reports/'before-frame.json',frame)
(reports/'source-original-campaign-proof.json').write_bytes((source/p['source_proof_file']).read_bytes())
state=dict(st);state.update(source=str(source),complete=False,failed=False,UI_exact_response_campaign=True,
    UI_input_sent=False,actual_UI_new_events=0,UI_native_gesture_and_frame_ready=False,
    UI_before_frame_manually_reviewed=True,UI_meaningful_after_frame_manually_reviewed=False,
    ui_response_verified=False,UI_source_original_BOOT_completed=True,UI_source_original_SystemServer_alive=True)
save(reports/'control-state.json',state)
save(reports/'resume-proof.json',dict(source_checkpoint_sha256=c['checkpoint_sha256'],source_instructions=c['instructions'],
    original_epoch_preflight_passed=True,whole_RVG_SHA256_verified=True,original_BOOT_completed=True,
    original_SystemServer_alive=True,SELinux_enforcing=True,all_eight_previous_events_preserved=True,
    original_before_frame_SHA256=frame['ppm_sha256'],new_guest_execution_started=False,
    actual_UI_response_verified=False))
print('Original booted source and authentic reviewed frame admitted; one UI gesture prepared, no new guest execution yet')
