#!/usr/bin/env python3
import json, math, platform, statistics, subprocess, sys
repeats=21
report={'repetitions':repeats,'warmups':1,'platform':platform.platform(),'timing':'128 individual XI X gates; initialization excluded; steady_clock','memory':'Linux /proc/self/status VmRSS and VmHWM KiB after exec','cases':{}}
for active in [1,64,256,1024,4096]:
    rows={'baseline':[],'candidate':[]}
    def run(label): return json.loads(subprocess.check_output([sys.argv[1 if label=='baseline' else 2],'--bench',str(active)],text=True))
    for label in rows: run(label)
    for i in range(repeats):
        for label in (['baseline','candidate'] if i%2==0 else ['candidate','baseline']):rows[label].append(run(label))
    report['cases'][str(active)]={}
    for label,samples in rows.items():
        summary={}
        for key in ['ms','rss_kib','peak_rss_kib']:
            values=sorted(s[key] for s in samples);summary[key]={'median':statistics.median(values),'p95':values[math.ceil(.95*repeats)-1]}
        report['cases'][str(active)][label]={'summary':summary,'state':samples[-1],'samples':samples}
print(json.dumps(report,indent=2))
