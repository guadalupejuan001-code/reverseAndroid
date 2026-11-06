"""Bind supplementary child-process observations to the original admitted source."""
import hashlib,json,os,pathlib,sys

source,reports=map(pathlib.Path,sys.argv[1:])
read=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
expected='f64b98e77cbcca9f8ac93007778b9006c515cc103389cae3faef1b9986ed1c54'
c=read(source/'output-contract.json');gate=read(source/'c46-progress.json')
pf=read(reports/'original-preflight.json');threads=read(reports/'threads.json')
st=read(source/'c87-proof.json');host=read(source/'process-outcome.json')
assert c==pf['contract'] and c['checkpoint_sha256']==expected==gate['checkpoint_sha256']
assert digest(source/'current.rvg')==expected and gate['source_boundary_verified']
assert c['instructions']==117147727998==threads['instructions']
assert threads['checkpoint_sha256']==expected and threads['input_unchanged']
assert threads['kernel_layout_pinned'] and threads['original_observer_file_unchanged']
assert threads['original_CPU_and_codecs_unchanged']
assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
assert read(source/'kernel-observation.json')['live_kernel_security']['selinux_enforcing']
assert pf['report']['provenance']['new_uart_input_bytes']==0
assert st['native_binary_SHA256']=='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
assert st['phase']==7 and st['waiting'] is False and st['actual_C87_new_events']==0
assert len(st['events'])==7
groups=threads['groups'];parent=next(g for g in groups if g['pid']==5629)
assert parent['thread_count']==len(parent['threads']) and not parent.get('error')
main=next(t for t in parent['threads'] if t['pid']==5629)
idmaps=[g for g in groups if g['name']=='idmap2']
assert len(idmaps)==1 and idmaps[0]['pid']==6387
assert idmaps[0]['thread_count']==len(idmaps[0]['threads']) and not idmaps[0].get('error')
children=[]
for group in groups:
    members=group.get('threads',[])
    leader=next((t for t in members if t['pid']==group['pid']),None)
    if leader and leader.get('signals',{}).get('real_parent',{}).get('pid')==5629:
        children.append(group)
summary=dict(schema='reverseandroid-original-readonly-parent-child-inspection-v1',source_checkpoint_sha256=expected,instructions=c['instructions'],source_native_execution_commit=(source/'source-commit.txt').read_text().strip(),diagnostic_execution_commit=os.environ['GITHUB_SHA'],whole_original_transport_and_original_epoch_preflight_verified=True,original_RVG_SHA256_reverified_after_diagnostic=True,guest_instructions_executed_by_diagnostic=0,new_guest_UART_bytes=0,original_observer_file_unchanged=True,kernel_BTF_layout_verified=True,SELinux_enforcing=True,source_original_C46_gate=gate,source_native_SHA256=st['native_binary_SHA256'],main_zygote=main,actual_observed_direct_children=children,idmap2_groups=idmaps,usable_UI_or_response_claimed=False)
(reports/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(dict(source_checkpoint_sha256=expected,read_only=True,direct_children=[dict(pid=g['pid'],name=g['name']) for g in children],source_boot_completed=gate['android_boot_completed_verified'])))
