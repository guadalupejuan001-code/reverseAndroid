#!/usr/bin/env python3
"""Compare identical native probes, alternating order, one process per sample."""
import argparse, json, math, platform, statistics, subprocess
p=argparse.ArgumentParser();p.add_argument('baseline');p.add_argument('candidate');p.add_argument('--repeats',type=int,default=21);a=p.parse_args()
report={'platform':platform.platform(),'repeats':a.repeats,'warmups_per_case':1,'timing':'native steady_clock; construction and execution; oracle excluded','memory':'Linux /proc/self/status VmRSS and VmHWM, KiB, per exec; allocator and library overhead included. getrusage lifetime high water also retained separately because it may include pre-exec launcher RSS','cases':{}}
for case in ['uncompute','history','replay','replay_noncancelling','entangled','random','product']:
    rows={label:[] for label in ['baseline','candidate']}
    def run(label):
        return json.loads(subprocess.check_output([getattr(a,label),case],text=True))
    for label in rows: run(label)
    for i in range(a.repeats):
        for label in (['baseline','candidate'] if i%2==0 else ['candidate','baseline']): rows[label].append(run(label))
    for label in rows:
        samples=rows[label];summary={}
        for key in ['ms','fallback_ms','rss_kib','peak_rss_kib','payload_bytes_estimate']:
            values=sorted(row[key] for row in samples)
            summary[key]={'median':statistics.median(values),'p95':values[math.ceil(.95*len(values))-1],'min':values[0],'max':values[-1]}
        summary['state']=samples[-1];rows[label]={'summary':summary,'samples':samples}
    report['cases'][case]=rows
print(json.dumps(report,indent=2))
