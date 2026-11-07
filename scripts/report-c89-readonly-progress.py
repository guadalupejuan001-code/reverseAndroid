"""Authenticate and summarize a pinned original snapshot without running a guest."""
import hashlib,json,os,pathlib,re,subprocess,sys

root=pathlib.Path(__file__).resolve().parents[1]
read=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
plan=read(root/'state/experiments/c89-readonly-progress.json')
assert plan['guest_execution'] is False and plan['new_UART_input'] is False
assert digest(root/'tools/reverseandroid-debuggerd-context.mjs')==plan['debuggerd_candidate_module_SHA256']
assert digest(root/'tools/reverseandroid-mmap-lock.mjs')==plan['mmap_lock_module_SHA256']
assert digest(root/'tools/reverseandroid-syscall-io.mjs')==plan['syscall_io_module_SHA256']
assert plan['read_only_io_context_PID']==plan['system_server_PID']
assert plan['read_only_debuggerd_context_PID']==plan['system_server_PID']
assert re.fullmatch(r'[0-9a-f]{64}',plan['source_checkpoint_sha256'])
assert type(plan['source_stage']) is int and 1<=plan['source_stage']<=16
assert re.fullmatch(r'state/experiments/c89-stage-[1-9][0-9]?-artifacts.json',plan['source_artifact_lock'])
source=root/'build/source-current-readonly';out=root/'evidence/c89-current-readonly-progress'
if sys.argv[1:] == ['--prepare']:
    out.mkdir(parents=True,exist_ok=True)
    (out/'selected-source-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    subprocess.run([sys.executable,'scripts/fetch-reverseandroid-artifact.py','seed',str(source),plan['source_artifact_lock']],cwd=root,check=True)
    with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('sha='+plan['source_checkpoint_sha256']+'\n')
elif sys.argv[1:] == ['--report']:
    c=read(source/'output-contract.json');g=read(source/'c46-progress.json');k=read(source/'kernel-observation.json');st=read(source/'zero-input-proof.json')
    pf=read(out/'original-preflight.json');d=read(out/'threads.json');host=read(source/'process-outcome.json')
    assert c==pf['contract'] and c['checkpoint_sha256']==g['checkpoint_sha256']==plan['source_checkpoint_sha256']==d['checkpoint_sha256']
    assert digest(source/'current.rvg')==c['checkpoint_sha256'] and g['source_boundary_verified']
    assert c['instructions']==g['instructions']==plan['instructions']==d['instructions']
    assert d['input_unchanged'] and d['kernel_layout_pinned'] and d['original_observer_file_unchanged'] and d['original_CPU_and_codecs_unchanged']
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    assert k['live_kernel_security']['selinux_enforcing']
    assert st['events']==plan['inherited_source_events'] and len(st['events'])==8
    assert st['phase']==8 and st['waiting'] is False and st['actual_campaign_new_events']==0 and st['zero_input_continuation_campaign']=='c89'
    assert st['C88_guard_succeeded'] and st['C88_priority_postprobe_checked'] and not st['failed']
    assert digest(source/'zero-input-proof.json')==plan['source_proof_SHA256']
    assert pf['report']['provenance']['new_uart_input_bytes']==0 and 'external_uart_input' not in pf['report']['provenance']
    assert st['native_binary_SHA256']==plan['expected_native_SHA256']
    assert (source/'source-commit.txt').read_text().strip()==plan['source_execution_commit']
    groups=d['groups'];parent=next(g for g in groups if g['pid']==plan['zygote_PID'])
    assert parent['thread_list_verified'] and parent['thread_count']==len(parent['threads']) and not parent.get('error')
    main=next(t for t in parent['threads'] if t['pid']==parent['pid'])
    target=next((t for t in parent['threads'] if t['pid']==plan['target_JIT_TID']),None)
    if target:
        assert target['tgid']==plan['zygote_PID'] and target['name']=='Jit thread pool' and target['priority']==120
        assert target['scheduling']['credentials']['uid']=='0' and target['scheduling']['credentials']['euid']=='0'
    system_server=None
    if plan.get('system_server_PID'):
        # A specific live PID is reported separately; the original nonunique-name gate remains unchanged.
        system_server=next(x for x in groups if x['pid']==plan['system_server_PID'])
        assert system_server['thread_list_verified'] and system_server['thread_count']==len(system_server['threads']) and not system_server.get('error')
        leader=next(t for t in system_server['threads'] if t['pid']==plan['system_server_PID'])
        assert leader['tgid']==leader['pid'] and leader['name']=='system_server' and leader['exit_state']==0
        assert leader['scheduling']['credentials']['uid']=='1000' and leader['scheduling']['credentials']['euid']=='1000'
        assert leader['signals']['real_parent']['pid']==plan['zygote_PID']
    candidate=leader.get('saved_user_frame',{}).get('debuggerd_signal_candidate')
    candidate_unverified=leader.get('saved_user_frame',{}).get('debuggerd_signal_candidate_unverified')
    children=[]
    for group in groups:
        leader=next((t for t in group.get('threads',[]) if t['pid']==group['pid']),None)
        if leader and leader.get('signals',{}).get('real_parent',{}).get('pid')==parent['pid']:children.append(group)
    summary=dict(schema='reverseandroid-original-readonly-current-startup-v1',source_checkpoint_sha256=c['checkpoint_sha256'],instructions=c['instructions'],source_native_execution_commit=(source/'source-commit.txt').read_text().strip(),diagnostic_execution_commit=os.environ['GITHUB_SHA'],source_original_C46_gate=g,whole_original_transport_and_epoch_preflight_verified=True,original_RVG_SHA256_reverified_after_diagnostic=True,guest_instructions_executed_by_diagnostic=0,new_guest_UART_bytes=0,kernel_BTF_layout_verified=True,original_observer_file_unchanged=True,SELinux_enforcing=True,main_zygote=main,target_JIT=target,zygote_group=parent,actual_SystemServer_group=system_server,actual_main_debuggerd_signal_candidate=candidate,actual_main_debuggerd_signal_candidate_unverified=candidate_unverified,actual_observed_direct_children=children,all_observed_zygote_or_system_server_groups=[x for x in groups if x['name'] in ['main','zygote64','system_server']],usable_UI_or_response_claimed=False)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(dict(instructions=c['instructions'],read_only=True,main_state=main['state'],main_static_priority=main['priority'],source_boot_completed=g['android_boot_completed_verified'])))
else:raise SystemExit('Expected --prepare or --report')
