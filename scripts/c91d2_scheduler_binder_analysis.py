#!/usr/bin/env python3
"""Summarize C91D2 read-only scheduling/Binder evidence without guest writes."""
from __future__ import annotations
from collections import Counter
import json
from pathlib import Path
import sys

source=Path(sys.argv[1]); selection=Path(sys.argv[2]); deep=Path(sys.argv[3]); threads=Path(sys.argv[4]); output=Path(sys.argv[5])
read=lambda p:json.loads(Path(p).read_text())
sel,k,t=read(selection),read(deep),read(threads)
current=read(source/"current.json"); gate=read(source/"c46-progress.json")
assert sel["checkpoint_sha256"]==k["checkpoint_sha256"]==t["checkpoint_sha256"]==gate["checkpoint_sha256"]
assert k["input_unchanged"] is True and t["input_unchanged"] is True
assert k["live_kernel_security"]["selinux_enforcing"] is True
groups=t["all_thread_scheduling"]
by_pid={g["pid"]:g for g in groups}
ss=by_pid[7354]
ss_threads=ss["threads"]
targets=[g for g in groups if g["name"] in {"cppreopts.sh","preopt2cachenam","mv","cp","fsync"}]
target_threads=[x for g in targets for x in g.get("threads",[])]
static=Counter(x.get("static_prio") for x in target_threads)
nice_minus20=sum(1 for x in target_threads if x.get("static_prio")==100)
nice0=sum(1 for x in target_threads if x.get("static_prio")==120)
runnable_targets=[x for x in target_threads if x.get("on_rq")==1]
activity_lines=[line for line in current.get("console","").splitlines() if "'activity' could not be found" in line]
binder=k.get("binder_live_queues")
summary={
 "schema":"reverseandroid-c91d2-readonly-scheduler-binder-summary-v1",
 "source":sel,
 "boot":{
   "sys_boot_completed":gate["android_boot_completed_verified"],
   "live_boot_property":k.get("live_boot_property",{}).get("boot_completed_property_live",False),
   "activity_missing_in_source_console":bool(activity_lines),
   "activity_evidence":activity_lines[-1] if activity_lines else None,
 },
 "SystemServer":{
   "pid":7354,"leader":ss_threads[0] if ss_threads else None,
   "thread_count":len(ss_threads),
   "runnable_threads":sum(1 for x in ss_threads if x.get("on_rq")==1),
   "running_threads":sum(1 for x in ss_threads if x.get("on_cpu")==1),
   "static_prio_distribution":dict(Counter(x.get("static_prio") for x in ss_threads)),
 },
 "cppreopts_family":{
   "process_count":len(targets),
   "thread_count":len(target_threads),
   "process_name_counts":dict(Counter(g["name"] for g in targets)),
   "static_prio_distribution":dict(static),
   "nice_minus20_static_prio_100_threads":nice_minus20,
   "nice0_static_prio_120_threads":nice0,
   "runnable_target_threads":len(runnable_targets),
   "runnable_target_names":dict(Counter(x["name"] for x in runnable_targets)),
 },
 "scheduler_probe":k.get("scheduler_probe"),
 "binder_live_queues":binder,
 "current_task":k.get("current_task_details"),
 "SELinux_enforcing":k["live_kernel_security"]["selinux_enforcing"],
 "diagnostic_read_only":True,
 "guest_executed":False,
 "guest_mutated":False,
}
if nice_minus20>0 and summary["SystemServer"]["runnable_threads"]>0 and summary["SystemServer"]["running_threads"]==0:
 summary["diagnosis"]="Verified nice -20 cppreopts-family work coexists with runnable/non-running SystemServer; priority normalization remains directly targeted."
else:
 summary["diagnosis"]="Deep scheduling evidence changed the prior hypothesis; inspect distributions before any further correction."
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(summary,indent=2)+"\n")
print(json.dumps(summary))
