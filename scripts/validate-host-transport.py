#!/usr/bin/env python3
"""Independent old/new byte controls and an actual >2GiB original admission."""
import base64,gzip,hashlib,json,os,pathlib,shutil,subprocess,sys
from c88_jit_priority_control import read,save,parse_reply
from c85_peripheral_completion import verify_live_peripherals
root=pathlib.Path(__file__).resolve().parents[1];os.chdir(root)
p=read(root/'state/experiments/host-transport-plan.json');cap=3840*1024**2
assert p['schema']=='reverseandroid-host-only-transport-validation-plan-v1'
assert p['new_UART_bytes']==0 and p['host_snapshot_capacity_bytes']==cap and p['host_wall_ms']==900000
assert p['control_instruction_budget']==50000000 and p['large_instruction_budget']==300000000
assert p['baseline_native_SHA256']=='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
reports=pathlib.Path('evidence/host-transport-validation');reports.mkdir(parents=True,exist_ok=True)
def digest(path):
    with pathlib.Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
native='build/host-transport-native/epoch-clz-ic-host';frame='build/host-transport-native/frame'
epoch='build/host-transport-runtime/tools/reverseandroid-epoch.mjs'
assert digest('build/ic-native/epoch-clz-ic')==p['baseline_native_SHA256']
manifest=read('build/host-transport-runtime/host-transport-manifest.json')
assert manifest['host_RVG_file_capacity_bytes']==cap and manifest['original_870_files_unchanged']
assert manifest['retained_IC_CLZ_extension']==read('build/ic-native/extension-manifest.json')
assert manifest['CPU_instruction_decoder_RAM_disk_clock_seed_cadence_IRQ_RVG_RVI_MXIM_and_original_milestones_unchanged']
def source(which):
    selected=p[which];path=pathlib.Path(selected['directory']);c=read(path/'output-contract.json');st=read(path/'zero-input-proof.json')
    assert c['checkpoint_sha256']==selected['checkpoint_sha256'] and c['instructions']==selected['instructions']
    assert digest(path/'current.rvg')==c['checkpoint_sha256'] and digest(path/'zero-input-proof.json')==selected['proof_SHA256']
    assert (path/'source-commit.txt').read_text().strip()==selected['execution_commit']
    assert st['native_binary_SHA256']==p['baseline_native_SHA256'] and st['events']==p['inherited_source_events']
    assert st['phase']==8 and st['waiting'] is False and not st['failed']
    assert st['C88_guard_succeeded'] and st['C88_priority_postprobe_checked'] and st['hardware_commands_completed']
    assert read(path/'c46-progress.json')['source_boundary_verified'] and read(path/'kernel-observation.json')['live_kernel_security']['selinux_enforcing']
    return path
def node(args,log):
    with pathlib.Path(log).open('x') as stream:subprocess.run(['node','--expose-gc','--max-old-space-size=5000',*args],stdout=stream,check=True)
def run(which,out,extended,budget):
    src=source(which);selected_epoch=epoch if extended else 'tools/reverseandroid-epoch.mjs'
    node([selected_epoch,'run',str(src),out,native if extended else 'build/ic-native/epoch-clz-ic','build/media/android-c27.raw',frame if extended else 'build/reverseandroid/frame',os.environ['GITHUB_SHA'],str(budget),'-'],reports/(pathlib.Path(out).name+'.json'))
    directory=pathlib.Path(out);c=read(directory/'output-contract.json');g=read(directory/'c46-progress.json');r=read(directory/'current.json');h=read(directory/'process-outcome.json')
    assert g['source_boundary_verified'] and h['status']=='process_completed' and h['exit_code']==0 and not h['forced_kill']
    assert c['instructions']==p[which]['instructions']+budget and digest(directory/'current.rvg')==c['checkpoint_sha256']==g['checkpoint_sha256']
    assert r['provenance']['new_uart_input_bytes']==0 and 'external_uart_input' not in r['provenance']
    assert r['provenance']['host_snapshot_transport_bytes']==(cap if extended else 2147483648)
    assert r['guest']['logical_ram_bytes']==2147483648 and r['guest']['counter_instructions_per_tick']==64
    assert read(directory/'kernel-observation.json')['live_kernel_security']['selinux_enforcing']
    shutil.rmtree(directory/'native/journal')
    return directory
mode=sys.argv[1]
if mode=='--baseline':
    run('control_source','evidence/host-control-baseline',False,p['control_instruction_budget'])
elif mode=='--candidate':
    out=run('control_source','evidence/host-control-candidate',True,p['control_instruction_budget']);old=pathlib.Path('evidence/host-control-baseline')
    subprocess.run(['cmp','--silent',str(old/'current.rvg'),str(out/'current.rvg')],check=True)
    assert (old/'console-new.txt').read_bytes()==(out/'console-new.txt').read_bytes()
    assert read(old/'c46-progress.json')==read(out/'c46-progress.json')
    assert read(old/'kernel-observation.json')==read(out/'kernel-observation.json')
    equality=dict(schema='reverseandroid-host-transport-byte-equality-v1',control_source=p['control_source'],instructions=p['control_instruction_budget'],
        baseline_native_SHA256=p['baseline_native_SHA256'],candidate_native_SHA256=digest(native),
        whole_original_RVG_SHA256=digest(out/'current.rvg'),whole_original_RVG_bytes=(out/'current.rvg').stat().st_size,
        actual_cmp_every_RVG_byte_equal=True,entire_console_bytes_equal=True,original_C46_gate_equal=True,entire_original_kernel_observation_equal=True,
        host_transport_policy_not_serialized=True,new_UART_bytes=0,control_is_not_primary_authority=True)
    save(reports/'byte-equality.json',equality)
    # Identical full state bytes are retained by the baseline transport; retain
    # this execution's own bound report/contract/process evidence separately.
    (out/'current.rvg').unlink();os.link(old/'current.rvg',out/'current.rvg');save(out/'identical-state-reference.json',equality)
elif mode=='--large':
    assert read(reports/'byte-equality.json')['actual_cmp_every_RVG_byte_equal']
    out=run('canonical_source','evidence/host-large',True,p['large_instruction_budget'])
    size=(out/'current.rvg').stat().st_size;assert 2147483648<size<=cap,'Actual larger checkpoint must cross the old host bound'
    encoded=(root/p['source_console_file']).read_bytes();assert hashlib.sha256(encoded).hexdigest()==p['source_console_base64_SHA256']
    zipped=base64.b64decode(encoded.strip(),validate=True);assert hashlib.sha256(zipped).hexdigest()==p['source_console_gzip_SHA256']
    raw=gzip.decompress(zipped);assert len(raw)==p['source_console_bytes'] and hashlib.sha256(raw).hexdigest()==p['source_console_SHA256']
    assert hashlib.sha256(raw).hexdigest()==read(source('canonical_source')/'zero-input-proof.json')['combined_original_console_bytes_sha256']
    raw+=(out/'console-new.txt').read_bytes();assert parse_reply(raw)['guard_succeeded'];(reports/'console-all.bin').write_bytes(raw)
    node(['tools/reverseandroid-console-proof.mjs',str(reports/'console-all.bin'),p['ELF_SHA256'],p['guest_ELF_path'],str(out/'console-observation.json')],reports/'large-console-observer.json')
    node(['build/host-transport-runtime/tools/reverseandroid-peripheral-probe.mjs',str(out/'current.rvg'),read(out/'output-contract.json')['checkpoint_sha256'],str(out/'peripherals-live-required.json'),p['guest_ELF_path']],reports/'large-peripherals.json')
    console=read(out/'console-observation.json');gate=read(out/'c46-progress.json');assert verify_live_peripherals(read(out/'peripherals-live-required.json'),console,gate['checkpoint_sha256'])
    proof=dict(schema='reverseandroid-host-transport-large-original-admission-v1',source=p['canonical_source'],host_snapshot_capacity_bytes=cap,
        native_SHA256=digest(native),frame_native_SHA256=digest(frame),checkpoint_SHA256=gate['checkpoint_sha256'],instructions=gate['instructions'],
        checkpoint_bytes=size,actual_checkpoint_exceeds_previous_2GiB_host_bound=True,
        full_original_native_roundtrip_and_original_C46_boundary_verified=True,fresh_live_peripherals_verified=True,SELinux_enforcing=True,
        inherited_source_events=p['inherited_source_events'],new_UART_bytes=0,
        combined_entire_original_console_SHA256=hashlib.sha256(raw).hexdigest(),original_BOOT=gate['android_boot_completed_verified'],
        usable_UI_or_response_claimed=False,experimental_validation_is_not_yet_primary=True)
    save(out/'host-transport-validation-proof.json',proof);save(reports/'large-original-admission.json',proof)
elif mode=='--report':
    equality=read(reports/'byte-equality.json');large=read(reports/'large-original-admission.json')
    assert equality['actual_cmp_every_RVG_byte_equal'] and large['actual_checkpoint_exceeds_previous_2GiB_host_bound']
    result=dict(schema='reverseandroid-validated-host-only-transport-extension-v1',execution_commit=os.environ['GITHUB_SHA'],CI_run_id=int(os.environ['GITHUB_RUN_ID']),
        baseline_native_SHA256=p['baseline_native_SHA256'],candidate_native_SHA256=digest(native),candidate_frame_SHA256=digest(frame),
        host_snapshot_capacity_bytes=cap,host_extension_manifest_SHA256=digest('build/host-transport-runtime/host-transport-manifest.json'),
        original_870_source_files_unchanged=True,full_original_control=equality,full_larger_original_admission=large,
        guest_RAM_disk_seed_clock_cadence_IRQ_and_snapshot_codecs_unchanged=True,original_BOOT_and_UI_gates_unchanged=True,
        new_UART_bytes=0,validated=True,candidate_is_not_yet_primary=True)
    save(reports/'validated-host-transport.json',result);print(json.dumps(result))
else:raise SystemExit('Expected --baseline, --candidate, --large or --report')
