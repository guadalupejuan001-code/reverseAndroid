#!/usr/bin/env python3
"""C91F1: compact guarded cppreopts priority control from latest preserved C91.

The source is selected by scripts/select_latest_preserved_c91.py at workflow
runtime. The failed C91F0 tail is never a source. Stage 2 is permitted only if
stage 1 both changes the target scheduling state and gives SystemServer measured
runtime progress. Every guest boundary remains subject to ordinary admission.
"""
from __future__ import annotations
from collections import Counter
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PLAN=ROOT/"state/experiments/c91f1-plan.json"
REPORTS=ROOT/"evidence/c91f1-reports"
SELECTION=ROOT/"evidence/c91f1/source-selection.json"
SOURCE=ROOT/"build/source-c91f1"
SOURCE_SCHED=ROOT/"evidence/c91f1/source-scheduling.json"
RUNTIME=ROOT/"build/host-observer-runtime"
NATIVE=ROOT/"build/host-native/epoch-clz-ic-host"
FRAME=ROOT/"build/host-native/frame"
DISK=ROOT/"build/media/android-c27.raw"
TARGET={"cppreopts.sh","preopt2cachenam","mv","cp","fsync"}


def read(path): return json.loads(Path(path).read_text())
def save(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2)+"\n")
def digest(path):
    with Path(path).open("rb") as stream:return hashlib.file_digest(stream,"sha256").hexdigest()


def validate_plan(p):
    assert p["schema"]=="reverseandroid-c91f1-latest-cppreopts-priority-control-v1"
    assert p["cycle"]=="C91F1" and p["source_run_id"]==42596365209
    assert p["source_must_be_latest_fully_preserved"] is True
    assert p["failed_C91F0_tail_must_not_be_source"] is True
    assert p["failed_C91F0_checkpoint_sha256"]=="d2157027999e28930b7cacadacfbd61cd50305207af6bcb64f1012620cc0153b"
    assert p["input_bytes"]==255 and re.fullmatch(r"[0-9a-f]{64}",p["input_sha256"])
    assert p["inherited_event_count"]==p["new_event_phase"]==8
    assert p["stage1_instruction_budget"]==p["stage2_instruction_budget"]==1000000000
    assert p["host_wall_ms"]==900000 and p["host_snapshot_capacity_bytes"]==4026531840
    assert p["expected_native_SHA256"]=="a31e3fd923f63071ebadeb6a78ee5aadf77111c88186e428927220f199710318"
    for key in ["guest_property_injection","guest_snapshot_patching","RAM_layout_modified",
                "disk_image_modified_before_resume","ISA_modified","HAL_modified",
                "GPU_or_QSSD_modified","promotion_authority"]:
        assert p[key] is False,key


def parse_reply(raw):
    clean=re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]",b"",raw).replace(b"\r",b"")
    result=[int(x) for x in re.findall(rb"^REVERSE_C91F1_RESULT RC_([0-9]+)$",clean,re.M)]
    done=re.findall(rb"^REVERSE_C91F1_DONE N_([0-9]+) RC_([0-9]+)$",clean,re.M)
    assert len(result)<=1 and len(done)<=1
    n=inner=None
    if done:n,inner=map(int,done[0])
    return {"reply_seen":bool(result),"return_code":result[0] if result else None,
            "done_seen":bool(done),"changed":n,"inner_return_code":inner,
            "guard_succeeded":result==[0] and bool(done) and inner==0}


def validate_target_identities(data):
    selected=[g for g in data["groups"] if g.get("name") in TARGET]
    assert selected, "No read-only target identity groups were selected"
    rows=[]
    for group in selected:
        leader=next((t for t in group.get("threads",[]) if t.get("pid")==group.get("pid")),None)
        assert leader is not None, ("Missing target leader",group.get("pid"),group.get("name"))
        scheduling=leader.get("scheduling")
        assert isinstance(scheduling,dict) and scheduling.get("read_only") is True
        cred=scheduling.get("credentials")
        assert isinstance(cred,dict), ("Missing credentials",group.get("pid"),group.get("name"))
        row={"pid":group["pid"],"name":group["name"],"static_prio":leader.get("priority"),
             "uid":cred.get("uid"),"euid":cred.get("euid"),
             "scheduler_groups":[x.get("path") for x in scheduling.get("groups",[])]}
        rows.append(row)
    assert all(x["uid"]=="0" and x["euid"]=="0" for x in rows), "Non-root process matches correction names"
    assert all(x["static_prio"]==100 for x in rows), "A correction-name process is not nice -20 at source"
    return rows


def scheduling_stats(data):
    groups=data["all_thread_scheduling"]
    targets=[g for g in groups if g["name"] in TARGET]
    threads=[t for g in targets for t in g.get("threads",[])]
    ss=next(g for g in groups if g["pid"]==7354)
    leader=next(t for t in ss["threads"] if t["pid"]==7354)
    return {
        "target_process_count":len(targets),
        "target_thread_count":len(threads),
        "target_process_names":dict(Counter(g["name"] for g in targets)),
        "target_static_prio":dict(Counter(str(t.get("static_prio")) for t in threads)),
        "nice_minus20":sum(t.get("static_prio")==100 for t in threads),
        "nice0":sum(t.get("static_prio")==120 for t in threads),
        "target_runnable":sum(t.get("on_rq")==1 for t in threads),
        "system_server":{
            "pid":7354,"static_prio":leader.get("static_prio"),
            "prio":leader.get("prio"),"normal_prio":leader.get("normal_prio"),
            "policy":leader.get("policy"),"on_rq":leader.get("on_rq"),"on_cpu":leader.get("on_cpu"),
            "sum_exec_runtime":int(leader["accounting"]["sum_exec_runtime"]),
            "vruntime":int(leader["accounting"]["vruntime"]),
            "thread_count":len(ss["threads"]),
            "runnable_threads":sum(t.get("on_rq")==1 for t in ss["threads"]),
            "running_threads":sum(t.get("on_cpu")==1 for t in ss["threads"]),
        }
    }


def run_thread_probe(checkpoint,sha,output):
    subprocess.run([
        "node","--max-old-space-size=5000",
        str(RUNTIME/"tools/reverseandroid-thread-probe-4g.mjs"),
        str(checkpoint),sha,str(output),"--all-scheduling","--diagnostic-processes","--copy-processes"
    ],check=True)
    data=read(output)
    assert data["checkpoint_sha256"]==sha and data["input_unchanged"] is True
    return data


def run_deep_probe(checkpoint,sha,output):
    with Path(output).open("x") as stream:
        subprocess.run([
            "node","--max-old-space-size=5000",
            str(RUNTIME/"tools/reverseandroid-host-kernel-observer.mjs"),
            str(checkpoint),sha,"4026531840",
            "--live-properties","--binder-probe","--scheduler-probe","--zygote-probe"
        ],stdout=stream,check=True)
    data=read(output)
    assert data["checkpoint_sha256"]==sha and data["input_unchanged"] is True
    return data


def validate_source(p):
    sel=read(SELECTION)
    assert sel["schema"]=="reverseandroid-latest-fully-preserved-c91-selection-v1"
    assert sel["run_id"]==p["source_run_id"]
    assert sel["checkpoint_sha256"]!=p["failed_C91F0_checkpoint_sha256"]
    assert sel["selection_is_promotion_authority"] is False
    contract=read(SOURCE/"output-contract.json");gate=read(SOURCE/"c46-progress.json")
    kernel=read(SOURCE/"kernel-observation.json");proof=read(SOURCE/"zero-input-proof.json")
    assert digest(SOURCE/"current.rvg")==sel["checkpoint_sha256"]==contract["checkpoint_sha256"]==gate["checkpoint_sha256"]
    assert sel["instructions"]==contract["instructions"]==gate["instructions"]==kernel["instructions"]
    assert gate["source_boundary_verified"] is True and gate["system_server_alive_verified"] is True
    assert kernel["live_kernel_security"]["selinux_enforcing"] is True
    assert proof["events"] and len(proof["events"])==p["inherited_event_count"]
    assert proof["phase"]==8 and proof["actual_campaign_new_events"]==0
    assert proof["system_server_alive"] is True and proof["SELinux_enforcing"] is True
    sched=read(SOURCE_SCHED);assert sched["checkpoint_sha256"]==sel["checkpoint_sha256"] and sched["input_unchanged"] is True
    identities=validate_target_identities(sched)
    stats=scheduling_stats(sched)
    assert len(identities)==stats["target_process_count"]
    return sel,gate,kernel,proof,stats,identities


def prepare(p):
    validate_plan(p)
    body=ROOT/p["input_file"]
    assert body.stat().st_size==p["input_bytes"] and digest(body)==p["input_sha256"]
    subprocess.run(["sh","-n",str(body)],check=True)
    assert digest(NATIVE)==p["expected_native_SHA256"]
    sel,gate,kernel,proof,stats,identities=validate_source(p)
    need=(not gate["android_boot_completed_verified"]
          and stats["nice_minus20"]>=p["source_gates"]["minimum_nice_minus20_target_threads"]
          and stats["system_server"]["on_rq"]==1 and stats["system_server"]["on_cpu"]==0)
    state={
      "schema":"reverseandroid-c91f1-control-state-v1",
      "source_selection":sel,
      "source":str(SOURCE.relative_to(ROOT)),
      "source_checkpoint_sha256":sel["checkpoint_sha256"],
      "source_instructions":sel["instructions"],
      "source_stats":stats,
      "source_target_identities":identities,
      "source_SystemServer_runtime":stats["system_server"]["sum_exec_runtime"],
      "events":proof["events"],
      "actual_new_events":0,
      "new_event_sent":False,
      "need_correction":need,
      "current_stage":0,
      "failed":False,
      "causal_progress":False,
      "boot_completed":gate["android_boot_completed_verified"],
      "promotion_authority":False,
    }
    REPORTS.mkdir(parents=True,exist_ok=True);save(REPORTS/"control-state.json",state)
    save(REPORTS/"source-summary.json",{"selection":sel,"stats":stats,"target_identities":identities,
      "boot_completed":gate["android_boot_completed_verified"],
      "SELinux_enforcing":kernel["live_kernel_security"]["selinux_enforcing"],
      "correction_required":need})
    if os.environ.get("GITHUB_OUTPUT"):
      with open(os.environ["GITHUB_OUTPUT"],"a") as out:out.write("need_correction="+str(need).lower()+"\n")
    print(json.dumps({"selection":sel,"source_stats":stats,"need_correction":need}))


def require_source_lease_before_stage1(st):
    runtime_dir="state/experiments/runtime-c91-stage1-lease"
    checked=subprocess.run(
        ["python3","scripts/select_latest_preserved_c91.py","--runtime-dir",runtime_dir],
        check=True,capture_output=True,text=True)
    latest=json.loads(checked.stdout)
    expected=st["source_selection"]
    assert latest["checkpoint_sha256"]==expected["checkpoint_sha256"], (
        "Selected source became stale before stage1: selected "
        +expected["checkpoint_sha256"]+" but latest fully preserved C91 is "
        +latest["checkpoint_sha256"])
    assert latest["stage"]==expected["stage"] and latest["instructions"]==expected["instructions"]
    save(REPORTS/"stage1-source-lease.json",{
        "schema":"reverseandroid-c91f1-stage1-source-lease-v1",
        "selected_stage":expected["stage"],
        "selected_checkpoint_sha256":expected["checkpoint_sha256"],
        "latest_stage":latest["stage"],
        "latest_checkpoint_sha256":latest["checkpoint_sha256"],
        "latest_instructions":latest["instructions"],
        "lease_passed":True,
        "guest_executed_during_lease":False,
        "guest_mutated_during_lease":False})
    return latest


def execute_epoch(p,source,output,budget,input_plan):
    parent=read(source/"output-contract.json")
    command=["node","--expose-gc","--max-old-space-size=5000",
      str(RUNTIME/"tools/reverseandroid-epoch.mjs"),"run",str(source),str(output),
      str(NATIVE),str(DISK),str(FRAME),os.environ["GITHUB_SHA"],str(budget),
      str(input_plan) if input_plan else "-",str(p["host_wall_ms"])]
    with (REPORTS/f"{output.name}-run.json").open("x") as log:
      subprocess.run(command,stdout=log,check=True)
    gate=read(output/"c46-progress.json");kernel=read(output/"kernel-observation.json")
    current=read(output/"current.json");host=read(output/"process-outcome.json")
    contract=read(output/"output-contract.json");policy=read(output/"host-policy.json")
    sha=digest(output/"current.rvg")
    assert sha==gate["checkpoint_sha256"]==kernel["checkpoint_sha256"]==contract["checkpoint_sha256"]
    assert read(output/"input-contract.json")==parent
    assert gate["source_boundary_verified"] is True and gate["system_server_alive_verified"] is True
    assert kernel["live_kernel_security"]["selinux_enforcing"] is True
    assert host["status"]=="process_completed" and host["exit_code"]==0 and host["forced_kill"] is False
    assert policy["native_binary_sha256"]==p["expected_native_SHA256"]
    return sha,gate,kernel,current


def run_stage(p,stage):
    assert stage in (1,2)
    st=read(REPORTS/"control-state.json")
    assert st["need_correction"] is True and st["failed"] is False and st["current_stage"]==stage-1
    if stage==1:
      require_source_lease_before_stage1(st)
      source=SOURCE;budget=p["stage1_instruction_budget"]
      body=ROOT/p["input_file"];input_plan=REPORTS/"stage1-input-plan.json"
      save(input_plan,{"schema":"reverseandroid-authenticated-console-input-plan-v1",
        "purpose":"diagnostic_console","input_file":os.path.relpath(body,input_plan.parent),
        "input_bytes":p["input_bytes"],"input_sha256":p["input_sha256"],
        "source_checkpoint_sha256":st["source_checkpoint_sha256"],
        "source_instructions":st["source_instructions"],
        "scope":"One multi-PID Toybox renice over names whose complete source set was read-only verified UID/EUID 0 and static_prio 100; postflight re-measures every scheduling gate. No property, service, RAM, disk, ISA, HAL, GPU or QSSD edit."})
    else:
      assert st["new_event_sent"] and st["actual_new_events"]==1 and st["causal_progress"]
      source=ROOT/"evidence/c91f1-1";budget=p["stage2_instruction_budget"];input_plan=None
    output=ROOT/f"evidence/c91f1-{stage}"
    sha,gate,kernel,current=execute_epoch(p,source,output,budget,input_plan)
    provenance=current["provenance"]
    if stage==1:
      assert provenance["new_uart_input_bytes"]==p["input_bytes"]
      event=provenance["external_uart_input"];assert event["sha256"]==p["input_sha256"]
      st["events"]=st["events"]+[{"phase":p["new_event_phase"],"input_plan":str(input_plan.relative_to(ROOT)),"actual_native_event":event}]
      st["new_event_sent"]=True;st["actual_new_events"]=1
    else:
      assert provenance["new_uart_input_bytes"]==0 and "external_uart_input" not in provenance

    transcript=REPORTS/"console-c91f1.bin"
    with transcript.open("ab") as out:out.write((output/"console-new.txt").read_bytes())
    reply=parse_reply(transcript.read_bytes())
    sched=run_thread_probe(output/"current.rvg",sha,output/"thread-scheduling.json")
    stats=scheduling_stats(sched)
    deep=run_deep_probe(output/"current.rvg",sha,output/"kernel-deep.json")
    ss_delta=stats["system_server"]["sum_exec_runtime"]-st["source_SystemServer_runtime"]

    if stage==1:
      source_n20=st["source_stats"]["nice_minus20"]
      drop=source_n20-stats["nice_minus20"]
      drop_fraction=(drop/source_n20) if source_n20 else 0.0
      gates=p["stage1_success_gates"]
      causal=(reply["guard_succeeded"]
        and (reply["changed"] or 0)>=gates["minimum_acknowledged_changed_processes"]
        and drop_fraction>=gates["minimum_nice_minus20_drop_fraction"]
        and stats["nice_minus20"]<=gates["maximum_nice_minus20_target_threads_after"]
        and stats["nice0"]>=gates["minimum_nice0_target_threads_after"]
        and ss_delta>=gates["minimum_SystemServer_runtime_delta"]
        and gate["system_server_alive_verified"] is True
        and kernel["live_kernel_security"]["selinux_enforcing"] is True)
      st["causal_progress"]=causal
      st["priority_drop_nice_minus20"]=drop
      st["priority_drop_fraction"]=drop_fraction
      if not causal:
        st["failed"]=True
        st["failure_reason"]="C91F1 stage1 did not satisfy every guarded scheduling/SystemServer causal gate."
    else:
      causal=st["causal_progress"]

    st.update({
      "source":str(output.relative_to(ROOT)),"current_stage":stage,
      "last_checkpoint_sha256":sha,"last_instructions":gate["instructions"],
      "last_stats":stats,"SystemServer_runtime_delta_from_source":ss_delta,
      "last_reply":reply,"boot_completed":gate["android_boot_completed_verified"],
      "live_boot_property":deep.get("live_boot_property",{}).get("boot_completed_property_live",False),
      "binder_live_queues":deep.get("binder_live_queues"),
      "SELinux_enforcing":True,"SystemServer_alive":True,
    })
    save(REPORTS/"control-state.json",st);save(output/"c91f1-proof.json",st)
    journal=output/"native/journal"
    if journal.exists():shutil.rmtree(journal)
    summary={"stage":stage,"checkpoint_sha256":sha,"instructions":gate["instructions"],
      "reply":reply,"source_nice_minus20":st["source_stats"]["nice_minus20"],
      "source_target_identity_count":len(st["source_target_identities"]),
      "current_nice_minus20":stats["nice_minus20"],"current_nice0":stats["nice0"],
      "SystemServer_runtime_delta_from_source":ss_delta,"causal_progress":causal,
      "boot_completed":gate["android_boot_completed_verified"],"failed":st["failed"]}
    save(REPORTS/f"stage{stage}-summary.json",summary);print(json.dumps(summary))
    if os.environ.get("GITHUB_OUTPUT"):
      with open(os.environ["GITHUB_OUTPUT"],"a") as out:
        out.write("continue="+str(causal and not gate["android_boot_completed_verified"] and not st["failed"]).lower()+"\n")
        out.write("complete="+str(gate["android_boot_completed_verified"]).lower()+"\n")
    if st["failed"]:raise SystemExit(st["failure_reason"])


def finish(p):
    st=read(REPORTS/"control-state.json")
    level=4 if st.get("boot_completed") else (3 if st.get("causal_progress") else 2)
    summary={
      "schema":"reverseandroid-c91f1-final-summary-v1",
      "source_selection":st["source_selection"],
      "need_correction":st["need_correction"],
      "actual_new_events":st["actual_new_events"],
      "current_stage":st["current_stage"],
      "checkpoint_sha256":st.get("last_checkpoint_sha256",st["source_checkpoint_sha256"]),
      "instructions":st.get("last_instructions",st["source_instructions"]),
      "reply":st.get("last_reply"),
      "source_stats":st["source_stats"],
      "last_stats":st.get("last_stats"),
      "SystemServer_runtime_delta_from_source":st.get("SystemServer_runtime_delta_from_source",0),
      "causal_progress":st.get("causal_progress",False),
      "android_boot_completed":st.get("boot_completed",False),
      "live_boot_property":st.get("live_boot_property",False),
      "SELinux_enforcing":st.get("SELinux_enforcing",True),
      "SystemServer_alive":st.get("SystemServer_alive",True),
      "failed":st.get("failed",False),
      "failure_reason":st.get("failure_reason"),
      "achieved_level":level,
      "promotion_authority":False,
      "failed_C91F0_tail_used_as_source":False,
      "GPU_QSSD_RAM_disk_ISA_HAL_changed":False,
      "next":"If level 3+, choose the highest fully preserved corrected boundary before canonical continuation; otherwise investigate the measured failed gate."
    }
    save(REPORTS/"final-summary.json",summary);print(json.dumps(summary))


def main():
    parser=argparse.ArgumentParser();parser.add_argument("command",choices=["prepare","run","finish"]);parser.add_argument("--stage",type=int)
    args=parser.parse_args();os.chdir(ROOT);p=read(PLAN);validate_plan(p)
    if args.command=="prepare":prepare(p)
    elif args.command=="run":assert args.stage in (1,2);run_stage(p,args.stage)
    else:finish(p)
if __name__=="__main__":main()
