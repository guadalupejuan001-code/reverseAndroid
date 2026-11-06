#!/usr/bin/env python3
"""Describe observed ID lifetimes without inventing earlier creation history."""
import argparse
import collections
import hashlib
import json
import pathlib

def analyze(raw):
    prefix='REVERSE_GPU_TRACE '
    events=[json.loads(line[len(prefix):]) for line in raw.decode().splitlines() if line.startswith(prefix)]
    assert events and not any(e['event']=='trace_limit' for e in events)
    assert [e['sequence'] for e in events]==list(range(len(events)))
    pending=None;last_ids=None;history={};invalid=[];anomalies=[]
    for e in events:
        ids={i for i in e['resource_ids'] if i}
        if e['event']=='source':
            assert pending is None
            for i in ids:history[i]='present_at_selected_source'
            last_ids=ids
        elif e['event']=='reset':
            assert pending is None
            for i in ids:history[i]='cleared_by_observed_reset'
            last_ids=set()
        elif e['event']=='request':
            assert pending is None
            if last_ids is not None and ids!=last_ids:
                anomalies.append({'sequence':e['sequence'],'reason':'unexplained_table_change','expected':sorted(last_ids),'observed':sorted(ids)})
            pending=e
        elif e['event']=='response':
            assert pending is not None
            assert all(e[k]==pending[k] for k in ('instructions','queue','command','resource_id','resource_id_present'))
            rid=e['resource_id']
            if e['response']==0x1100:
                if e['command']==0x101:history[rid]='created_in_observed_window'
                if e['command']==0x102:history[rid]='retired_by_observed_unref'
            if e['response']==0x1203:
                assert e['resource_id_present'] and (not rid or rid not in ids)
                invalid.append({'instructions':e['instructions'],'command':hex(e['command']),
                                'resource_id':rid,'resource_id_hex':hex(rid),'active_ids':sorted(ids),
                                'last_observed_lifetime':history.get(rid,'not_present_or_created_in_observed_window') if rid else 'reserved_zero'})
            last_ids=ids;pending=None
    assert pending is None,'Unpaired request; incomplete observation'
    counts=collections.Counter((e['resource_id'],e['last_observed_lifetime']) for e in invalid)
    return {'schema':'reverseandroid-observed-gpu-resource-lifetimes-v1',
            'native_log_sha256':hashlib.sha256(raw).hexdigest(),'events':len(events),
            'source_resource_ids':events[0]['resource_ids'],
            'missing_id_counts':[{'resource_id':rid,'resource_id_hex':hex(rid),'last_observed_lifetime':reason,'count':n} for (rid,reason),n in sorted(counts.items())],
            'invalid_resource_events':invalid,'table_transition_anomalies':anomalies,
            'events_require_separate_original_native_admission':True,
            'historical_creation_before_selected_source_not_inferred':True,
            'GPU_cause_of_Android_boot_wait_established':False}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('log');p.add_argument('output');a=p.parse_args()
    result=analyze(pathlib.Path(a.log).read_bytes())
    with pathlib.Path(a.output).open('x') as f:f.write(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
