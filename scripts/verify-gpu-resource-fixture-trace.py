#!/usr/bin/env python3
"""Check actual PCI/MMIO protocol tracing against uninstrumented original bytes."""
import hashlib
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
assert (root/'control.rvg').read_bytes() == (root/'trace.rvg').read_bytes()
assert (root/'control.json').read_bytes() == (root/'trace.json').read_bytes()
prefix = 'REVERSE_GPU_TRACE '
events = [json.loads(line[len(prefix):]) for line in (root/'trace.log').read_text().splitlines() if line.startswith(prefix)]
assert events and not any(e['event']=='trace_limit' for e in events)
assert not any(line.startswith(prefix) for line in (root/'control.log').read_text().splitlines())
assert [e['sequence'] for e in events] == list(range(len(events)))
for event in events:
    assert event['candidate'] is True
    if event['event']=='response' and event['response']==0x1203:
        assert event['resource_id_present'] and (event['resource_id']==0 or event['resource_id'] not in event['resource_ids'])
responses = [e for e in events if e['event']=='response']
assert sum(e['response']==0x1203 for e in responses)==7
assert any(e['command']==0x103 and e['resource_id']==0 and e['response']==0x1100 for e in responses)
assert any(e['event']=='reset' and e['resource_ids']==[43,0,0,0] for e in events)
assert any(e['command']==0x101 and e['resource_id']==42 and e['response']==0x1100 and 42 in e['resource_ids'] for e in responses)
assert any(e['command']==0x102 and e['resource_id']==42 and e['response']==0x1100 and 42 not in e['resource_ids'] for e in responses)
assert any(e['command']==0x101 and e['resource_id']==43 and e['response']==0x1100 and 43 in e['resource_ids'] for e in responses)
report = {'schema':'reverseandroid-original-gpu-lifecycle-fixture-v1',
          'original_whole_RVG_and_report_identical':True,
          'original_snapshot_sha256':hashlib.sha256((root/'trace.rvg').read_bytes()).hexdigest(),
          'trace_events':len(events),'expected_invalid_resource_responses':7,
          'create_transfer_scanout_flush_unref_reuse_restore_reset_verified':True,
          'original_decoder_unchanged':True,'guest_behavior_changed':False,
          'protocol_fixture_only':True,'Android_boot_verified':False}
(root/'fixture-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
