#!/usr/bin/env python3
"""Replay the historical allocation boundary with explicit, distinct GPU profiles."""
import argparse
import collections
import hashlib
import json
import pathlib
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--source',required=True)
p.add_argument('--output',required=True)
p.add_argument('--commit',required=True)
args=p.parse_args()
source=pathlib.Path(args.source);out=pathlib.Path(args.output)
out.mkdir(parents=True,exist_ok=False)
read=lambda p:json.loads(pathlib.Path(p).read_text())
def digest(p):
    with pathlib.Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
contract=read(source/'output-contract.json')
assert contract['instructions']==47_000_000_000
assert contract['checkpoint_sha256']=='bfd5feee5264a2e43f21684a25b7ceb7b27222c94264f188983b9cff1f8ac801'
assert digest(source/'current.rvg')==contract['checkpoint_sha256']
budget=1_000_000_000
results={}
for name,native in [('control','build/gpu-capacity/epoch-control'),('candidate','build/gpu-capacity/epoch')]:
    target=out/name
    with (out/(name+'-controller.json')).open('x') as log:
        subprocess.run(['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs',
                        'run',str(source),str(target),native,'build/media/android-c27.raw',
                        'build/gpu-capacity/frame',args.commit,str(budget),'-'],stdout=log,check=True)
    host=read(target/'process-outcome.json');report=read(target/'current.json')
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    assert all(not host['logs'][stream]['truncated'] for stream in ('stdout','stderr'))
    assert report['stop_reason']=='instruction_budget' and not report['error']
    assert report['guest']['instructions']==contract['instructions']+budget
    provenance=report['provenance']
    assert provenance['new_uart_input_bytes']==0 and not provenance['source_cpu_modified_before_resume']
    assert not provenance['live_android_properties_written']
    with (target/'gpu-capacity.json').open('x') as log:
        subprocess.run(['node','tools/android_gpu_capacity_evidence.mjs',str(target)],stdout=log,check=True)
    profile=read('build/gpu-capacity-src/gpu-capacity-manifest.json') if name=='candidate' else {
        'schema':'reverseandroid-original-four-resource-device-profile-v1',
        'resource_capacity':4,'GPU_PCI_subformat_version':1}
    profile.update({'native_binary_sha256':digest(native),
                    'frame_binary_sha256':digest('build/gpu-capacity/frame'),
                    'execution_commit':args.commit,'primary_promotion_authority':False})
    (target/'device-profile.json').write_text(json.dumps(profile,indent=2)+'\n')
    events=[]
    for line in (target/'progress.log').read_text().splitlines():
        if line.startswith('REVERSE_GPU_TRACE '):events.append(json.loads(line[len('REVERSE_GPU_TRACE '):]))
    assert events and events[0]['event']=='source'
    assert not any(e['event']=='trace_limit' for e in events)
    requests={};responses=[]
    for i,event in enumerate(events):
        assert event['sequence']==i
        if event['event']=='request':
            assert not requests;requests[event['sequence']]=event
        elif event['event']=='response':
            assert len(requests)==1
            request=requests.popitem()[1]
            for key in ('instructions','command','resource_id','queue'):
                assert event[key]==request[key]
            responses.append(event)
    assert not requests
    counts=collections.Counter((e['command'],e['response']) for e in responses)
    results[name]={'contract':read(target/'output-contract.json'),
                   'GPU':read(target/'gpu-capacity.json'),
                   'boot_gate':read(target/'c46-progress.json'),
                   'trace_events':len(events),'responses':responses,
                   'response_counts':[{'command':c,'response':r,'count':n} for (c,r),n in sorted(counts.items())]}
assert digest(source/'current.rvg')==contract['checkpoint_sha256']
# Prove the actual guest allocation failure, and its successful counterpart.
failed=[e for e in results['control']['responses'] if e['command']==0x101 and e['response']==0x1201]
assert failed,'Historical failure was not reproduced; no causal repair claim is admitted'
for error in failed:
    assert len(error['resource_ids'])==4 and all(error['resource_ids'])
    assert any(e['command']==0x101 and e['resource_id']==error['resource_id'] and e['response']==0x1100
               for e in results['candidate']['responses']),error
first_repaired=next(e for e in results['candidate']['responses']
                    if e['command']==0x101 and e['resource_id']==failed[0]['resource_id'])
assert first_repaired['instructions']==failed[0]['instructions']
assert first_repaired['resource_ids'][:4]==failed[0]['resource_ids']
assert not any(e['response']==0x1201 for e in results['candidate']['responses'])
assert not any(e['response']==0x1203 for e in results['candidate']['responses'])
assert results['control']['GPU']['pci_state_version']==1
assert results['candidate']['GPU']['pci_state_version']==2
summary={'schema':'reverseandroid-explicit-gpu-capacity-comparison-v1',
         'historical_diagnostic_reference_only':True,
         'source_checkpoint_sha256':contract['checkpoint_sha256'],
         'source_instructions':contract['instructions'],'budget_per_run':budget,
         'source_unchanged':True,'primary_checkpoint_or_execution_modified':False,
         'new_UART_input_bytes':0,'both_original_epoch_admission_gates_passed':True,
         'RAM_disk_seed_cadence_IRQ_security_and_outer_compact_codecs_preserved':True,
         'old_native_cannot_decode_new_GPU_PCI_subformat_2':True,
         'actual_failed_create_resource_ids':[e['resource_id'] for e in failed],
         'allocation_failure_reproduced_and_repaired':True,
         'first_failed_create_and_repair_at_same_original_instruction':True,
         'control_matches_historical_C79_stage18_full_RVG':results['control']['contract']['checkpoint_sha256']=='fbe8a42ad41cd98cf20d3e595e10389d626ef21ea0aa74da49049b1635c6d60e',
         'native_GPU_renderer_or_QSSD_speedup_claimed':False,
         'control':results['control'],'candidate':results['candidate']}
(out/'gpu-capacity-comparison.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary))
