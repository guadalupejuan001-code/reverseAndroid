#!/usr/bin/env python3
"""Compare a host-only trace against the admitted original native on one source."""
import argparse
import collections
import hashlib
import json
import pathlib
import subprocess
import sys

p=argparse.ArgumentParser()
p.add_argument('--source',required=True)
p.add_argument('--artifact-lock',required=True)
p.add_argument('--output',required=True)
p.add_argument('--commit',required=True)
p.add_argument('--budget',type=int,default=50_000_000)
args=p.parse_args()
assert 0 < args.budget <= 50_000_000
source=pathlib.Path(args.source);out=pathlib.Path(args.output)
out.mkdir(parents=True,exist_ok=False)
read=lambda p:json.loads(pathlib.Path(p).read_text())
def digest(p):
    with pathlib.Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
contract=read(source/'output-contract.json')
assert digest(source/'current.rvg')==contract['checkpoint_sha256']
native=pathlib.Path('build/ic-native/epoch-clz-ic')
assert digest(native)=='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
with (out/'source-original-preflight.json').open('x') as log:
    subprocess.run(['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs',
                    'preflight',str(source)],stdout=log,check=True)
outputs={}
for name,engine in [('control',str(native)),('trace','build/gpu-resource-trace-tests/epoch-gpu-trace')]:
    target=out/name
    with (out/(name+'-controller.json')).open('x') as log:
        subprocess.run(['node','--expose-gc','--max-old-space-size=5000','tools/reverseandroid-epoch.mjs',
                        'run',str(source),str(target),engine,'build/media/android-c27.raw',
                        'build/reverseandroid/frame',args.commit,str(args.budget),'-'],stdout=log,check=True)
    host=read(target/'process-outcome.json');report=read(target/'current.json')
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    assert all(not host['logs'][stream]['truncated'] for stream in ('stdout','stderr')),'Incomplete native log capture'
    assert report['stop_reason']=='instruction_budget' and not report['error']
    assert report['guest']['instructions']==contract['instructions']+args.budget
    assert report['provenance']['new_uart_input_bytes']==0
    assert not report['provenance']['source_cpu_modified_before_resume']
    outputs[name]=read(target/'output-contract.json')
assert digest(source/'current.rvg')==contract['checkpoint_sha256']
assert outputs['control']['checkpoint_sha256']==outputs['trace']['checkpoint_sha256']
assert digest(out/'control/current.rvg')==digest(out/'trace/current.rvg')==outputs['control']['checkpoint_sha256']
assert (out/'control/console-new.txt').read_bytes()==(out/'trace/console-new.txt').read_bytes()
assert (out/'control/checkpoint-frame.ppm').exists()==(out/'trace/checkpoint-frame.ppm').exists()
if (out/'control/checkpoint-frame.ppm').exists():
    assert digest(out/'control/checkpoint-frame.ppm')==digest(out/'trace/checkpoint-frame.ppm')
events=[];prefix='REVERSE_GPU_TRACE '
for line in (out/'trace/progress.log').read_text().splitlines():
    if line.startswith(prefix):events.append(json.loads(line[len(prefix):]))
assert events and events[0]['event']=='source'
assert not any(e['event']=='trace_limit' for e in events),'Incomplete trace; retain evidence without claiming full coverage'
responses=[e for e in events if e['event']=='response']
invalid=[e for e in responses if e['response']==0x1203]
for event in invalid:
    assert event['resource_id_present'] and (event['resource_id']==0 or event['resource_id'] not in event['resource_ids'])
counts=collections.Counter((e['command'],e['response']) for e in responses)
summary={'schema':'reverseandroid-original-native-gpu-trace-comparison-v1',
         'source_checkpoint_sha256':contract['checkpoint_sha256'],'source_instructions':contract['instructions'],
         'source_artifact_lock':args.artifact_lock,'source_snapshot_unchanged':True,
         'budget_per_run':args.budget,'final_checkpoint_sha256':outputs['control']['checkpoint_sha256'],
         'final_instructions':outputs['control']['instructions'],
         'whole_original_RVG_identical':True,'whole_original_new_console_identical':True,
         'complete_native_logs_retained':True,'original_frame_bytes_identical':True,
         'original_admission_passed_both':True,'zero_new_UART_input':True,
         'source_resource_ids':events[0]['resource_ids'],'trace_events':len(events),
         'response_counts':[{'command':cmd,'response':response,'count':count} for (cmd,response),count in sorted(counts.items())],
         'invalid_resource_events':invalid,'original_final_boot_gate':read(out/'control/c46-progress.json'),
         'observer_is_not_a_GPU_fix':True,'primary_continuation_modified':False,
         'futex_owner_or_graphics_causality_established':False}
(out/'gpu-trace-comparison.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary))
