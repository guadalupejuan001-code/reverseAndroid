"""One authenticated, isolated read of existing Android traces; never promotion authority."""
import base64,gzip,hashlib,io,os,pathlib,re,shutil,subprocess,sys
from c88_jit_priority_control import read,save,digest

ROOT=pathlib.Path(__file__).resolve().parents[1]
PLAN=ROOT/'state/experiments/c89-anr-read.json'
REPORTS=ROOT/'evidence/c89-anr-read-reports'
SOURCE=ROOT/'build/source-c89-anr-read'

def parse_response(raw):
    lines=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',raw).replace(b'\r',b'').splitlines()
    starts=[i for i,line in enumerate(lines) if line==b'REVERSE_ANR_READ_BEGIN']
    ends=[(i,int(match[1])) for i,line in enumerate(lines) if (match:=re.fullmatch(rb'REVERSE_ANR_READ_END RC_([0-9]{1,3})',line))]
    assert len(starts)<=1 and len(ends)<=1,'Duplicate actual read response'
    if ends:assert starts and ends[0][0]>starts[0],'Read end without an earlier real begin'
    return dict(begin_seen=bool(starts),end_after_begin_seen=bool(ends),reply_code=ends[0][1] if ends else None,
                completed_read_query=bool(ends and ends[0][1]==0),failed=bool(ends and ends[0][1]!=0))

def validate_plan(p):
    assert p['schema']=='reverseandroid-isolated-ANR-read-plan-v1' and p['primary_or_promotion_authority'] is False
    assert p['max_stages']==8 and p['instruction_budget']==500000000 and p['host_wall_ms']==900000
    assert p['expected_native_SHA256']=='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
    assert len(p['inherited_source_events'])==8 and [e['phase'] for e in p['inherited_source_events']]==list(range(8))
    assert digest(ROOT/'build/ic-native/epoch-clz-ic')==p['expected_native_SHA256']

def prepare(p):
    c=read(SOURCE/'output-contract.json');g=read(SOURCE/'c46-progress.json');k=read(SOURCE/'kernel-observation.json')
    pf=read(REPORTS/'source-original-admission.json');st=read(SOURCE/'zero-input-proof.json')
    assert c==pf['contract'] and c['checkpoint_sha256']==g['checkpoint_sha256']==p['source_checkpoint_sha256']
    assert c['instructions']==g['instructions']==p['source_instructions'] and g['source_boundary_verified']
    assert digest(SOURCE/'current.rvg')==c['checkpoint_sha256'] and digest(SOURCE/'zero-input-proof.json')==p['source_proof_SHA256']
    assert (SOURCE/'source-commit.txt').read_text().strip()==p['source_execution_commit']
    assert k['kernel_sha256']==p['kernel_SHA256'] and k['input_unchanged'] and k['live_kernel_security']['selinux_enforcing']
    assert st['events']==p['inherited_source_events'] and st['phase']==8 and not st['waiting'] and not st['failed']
    assert st['native_binary_SHA256']==p['expected_native_SHA256'] and st['actual_campaign_new_events']==0
    ip=ROOT/p['input_plan_file'];q=read(ip);body=ip.parent/q['input_file']
    assert q['purpose']=='diagnostic_console' and q['source_checkpoint_sha256']==c['checkpoint_sha256'] and q['source_instructions']==c['instructions']
    assert 0<body.stat().st_size==q['input_bytes']==p['input_bytes']<=4096
    assert digest(body)==q['input_sha256']==p['input_sha256'];subprocess.run(['sh','-n'],input=body.read_bytes(),check=True)
    encoded=(ROOT/p['source_console_file']).read_bytes();assert hashlib.sha256(encoded).hexdigest()==p['source_console_base64_SHA256']
    compressed=base64.b64decode(encoded.strip(),validate=True);assert hashlib.sha256(compressed).hexdigest()==p['source_console_gzip_SHA256']
    assert 0<p['source_console_bytes']<=64*1024**2
    with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
        raw=stream.read(p['source_console_bytes']+1);assert not stream.read(1)
    assert len(raw)==p['source_console_bytes'] and hashlib.sha256(raw).hexdigest()==p['source_console_SHA256']==st['combined_original_console_bytes_sha256']
    assert not parse_response(raw)['begin_seen']
    (REPORTS/'console-all.bin').write_bytes(raw)
    save(REPORTS/'control-state.json',dict(schema='reverseandroid-isolated-ANR-read-state-v1',source=str(SOURCE),
         source_checkpoint_sha256=c['checkpoint_sha256'],source_instructions=c['instructions'],inherited_source_events=st['events'],
         isolated_new_events=[],completed_read_query=False,failed=False,combined_console_SHA256=p['source_console_SHA256'],
         primary_or_promotion_authority=False,emulator_completion_claimed=False))

def stage(p,n):
    assert 1<=n<=p['max_stages'];state_path=REPORTS/'control-state.json';st=read(state_path)
    assert not st['completed_read_query'] and not st['failed'] and st['inherited_source_events']==p['inherited_source_events']
    source=pathlib.Path(st['source']);c=read(source/'output-contract.json');output=ROOT/f'evidence/c89-anr-read-{n}'
    assert len(st['isolated_new_events'])==(0 if n==1 else 1)
    if n==1:assert c['checkpoint_sha256']==p['source_checkpoint_sha256'] and c['instructions']==p['source_instructions']
    command=['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs','run',str(source),str(output),
             'build/ic-native/epoch-clz-ic','build/media/android-c27.raw','build/reverseandroid/frame',os.environ['GITHUB_SHA'],
             str(p['instruction_budget']),p['input_plan_file'] if n==1 else '-']
    with (REPORTS/f'stage-{n}.json').open('x') as log:subprocess.run(command,cwd=ROOT,stdout=log,check=True)
    g=read(output/'c46-progress.json');r=read(output/'current.json');k=read(output/'kernel-observation.json');host=read(output/'process-outcome.json')
    assert g['source_boundary_verified'] and digest(output/'current.rvg')==g['checkpoint_sha256']==read(output/'output-contract.json')['checkpoint_sha256']
    assert c['instructions']<g['instructions']<=c['instructions']+p['instruction_budget']
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    assert k['kernel_sha256']==p['kernel_SHA256'] and k['input_unchanged'] and k['live_kernel_security']['selinux_enforcing']
    policy=read(output/'host-policy.json');assert policy['host_wall_ms']==p['host_wall_ms'] and policy['native_binary_sha256']==p['expected_native_SHA256']
    provenance=r['provenance'];assert provenance['input_checkpoint_sha256']==c['checkpoint_sha256']
    if n==1:
        event=provenance['external_uart_input'];assert provenance['new_uart_input_bytes']==p['input_bytes']
        assert event['instructions']==c['instructions'] and event['bytes']==p['input_bytes'] and event['sha256']==p['input_sha256']
        st['isolated_new_events'].append(event)
    else:assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
    transcript=REPORTS/'console-all.bin';assert digest(transcript)==st['combined_console_SHA256']
    with transcript.open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
    raw=transcript.read_bytes();reply=parse_response(raw)
    st.update(source=str(output),last_admitted_stage=n,last_admitted_sha256=g['checkpoint_sha256'],last_admitted_instructions=g['instructions'],
              combined_console_SHA256=hashlib.sha256(raw).hexdigest(),**reply,
              original_SystemServer_gate=g['system_server_alive_verified'],original_BOOT_gate=g['android_boot_completed_verified'],
              SELinux_enforcing=True,primary_or_promotion_authority=False,emulator_completion_claimed=False)
    save(state_path,st);save(output/'isolated-anr-proof.json',st);shutil.rmtree(output/'native/journal')
    with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('complete='+str(st['completed_read_query']).lower()+'\n')
    print(__import__('json').dumps(dict(stage=n,reply=reply,checkpoint_sha256=g['checkpoint_sha256'],instructions=g['instructions'],
        isolated_new_UART_bytes=provenance['new_uart_input_bytes'],primary_or_promotion_authority=False,emulator_completion_claimed=False)))
    if st['failed']:raise SystemExit('Actual read query failed; preserve this isolated diagnostic and stop')

if __name__=='__main__':
    p=read(PLAN);validate_plan(p)
    if sys.argv[1:]==['--prepare']:prepare(p)
    else:stage(p,int(sys.argv[1]))
