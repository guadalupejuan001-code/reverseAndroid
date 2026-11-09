#!/usr/bin/env python3
"""Select the highest fully preserved C91 checkpoint from one authoritative run.

This is a host-only selector. It never executes or mutates the guest. A stage is
eligible only when its metadata artifact and every transport piece named by the
authenticated manifest are present, unexpired, and tied to the same workflow
run/head SHA. The generated lock is local workflow state, not promotion.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile
import zipfile
from urllib.request import Request, urlopen

ROOT=Path(__file__).resolve().parents[1]
REPO="guadalupejuan001-code/reverseAndroid"
RUN_ID=42596365209
PREFIX="reverseandroid-c91-"
SHA=re.compile(r"^[0-9a-f]{64}$")


def api(path):
    token=os.environ["GH_TOKEN"]
    req=Request(
        "https://api.github.com/repos/"+REPO+"/"+path,
        headers={"Authorization":"Bearer "+token,"Accept":"application/vnd.github+json",
                 "X-GitHub-Api-Version":"2022-11-28"})
    with urlopen(req,timeout=60) as response:
        payload=response.read(16*1024*1024+1)
    assert len(payload)<=16*1024*1024
    return json.loads(payload)


def zip_digest(item):
    value=(item.get("digest") or "")
    assert value.startswith("sha256:")
    value=value.split(":",1)[1]
    assert SHA.fullmatch(value)
    return value


def safe_manifest_from_metadata(item,stage):
    with tempfile.TemporaryDirectory() as directory:
        archive=Path(directory)/"metadata.zip"
        with archive.open("xb") as output:
            subprocess.run(
                ["gh","api",f"repos/{REPO}/actions/artifacts/{item['id']}/zip"],
                stdout=output,check=True,timeout=600)
        with archive.open("rb") as stream:
            assert hashlib.file_digest(stream,"sha256").hexdigest()==zip_digest(item)
        with zipfile.ZipFile(archive) as z:
            names=z.namelist()
            wanted=f"build/c91-transport-{stage}/manifest.json"
            assert names.count(wanted)==1, (stage,wanted,names)
            for info in z.infolist():
                path=Path(info.filename)
                assert not path.is_absolute() and ".." not in path.parts
                assert not stat.S_ISLNK(info.external_attr>>16)
            raw=z.read(wanted)
        manifest=json.loads(raw)
        return manifest,raw


def group_artifacts(artifacts):
    grouped={}
    rx=re.compile(r"^reverseandroid-c91-([1-9][0-9]*)-(metadata|[0-9]{3})$")
    for item in artifacts:
        m=rx.match(item.get("name",""))
        if not m or item.get("expired") is not False:
            continue
        stage=int(m.group(1)); kind=m.group(2)
        assert stage<=16
        grouped.setdefault(stage,{})[kind]=item
    return grouped


def validate_candidate(stage,group,manifest):
    assert manifest["schema"]=="reverseandroid-checkpoint-transport-v1"
    assert manifest["compression"]=="tar+gzip"
    contract=manifest["source_contract"]; progress=manifest["progress"]
    assert contract["checkpoint_sha256"]==progress["checkpoint_sha256"]
    assert contract["instructions"]==progress["instructions"]
    assert contract["source_commit"]=="a035f386b468c71ad11c32aee88252dbab0b91db"
    assert progress["source_boundary_verified"] is True
    assert progress["system_server_alive_verified"] is True
    assert isinstance(progress["android_boot_completed_verified"],bool)
    assert 1<=len(manifest["parts"])<=16
    expected=[]
    for index,part in enumerate(manifest["parts"]):
        assert part["file"]==f"checkpoint.part-{index:03d}"
        assert 0<part["bytes"]<=200*1024**2
        assert SHA.fullmatch(part["sha256"])
        kind=f"{index:03d}"
        assert kind in group
        item=group[kind]
        assert item["workflow_run"]["id"]==RUN_ID
        assert item["workflow_run"]["head_sha"]=="a035f386b468c71ad11c32aee88252dbab0b91db"
        expected.append(item)
    metadata=group["metadata"]
    assert metadata["workflow_run"]["id"]==RUN_ID
    assert metadata["workflow_run"]["head_sha"]=="a035f386b468c71ad11c32aee88252dbab0b91db"
    return contract,progress,expected,metadata


def select():
    response=api(f"actions/runs/{RUN_ID}/artifacts?per_page=100")
    artifacts=response.get("artifacts",[])
    grouped=group_artifacts(artifacts)
    rejected=[]
    for stage in sorted(grouped,reverse=True):
        group=grouped[stage]
        if "metadata" not in group:
            rejected.append({"stage":stage,"reason":"metadata_missing"})
            continue
        try:
            manifest,raw=safe_manifest_from_metadata(group["metadata"],stage)
            contract,progress,pieces,metadata=validate_candidate(stage,group,manifest)
        except Exception as exc:
            rejected.append({"stage":stage,"reason":str(exc)})
            continue
        selection={
            "schema":"reverseandroid-latest-fully-preserved-c91-selection-v1",
            "repository":REPO,"run_id":RUN_ID,"stage":stage,
            "checkpoint_sha256":contract["checkpoint_sha256"],
            "instructions":contract["instructions"],
            "source_commit":contract["source_commit"],
            "android_boot_completed_verified":progress["android_boot_completed_verified"],
            "metadata_artifact":{"id":metadata["id"],"sha256":zip_digest(metadata),"name":metadata["name"]},
            "piece_artifacts":[{"id":x["id"],"sha256":zip_digest(x),"name":x["name"]} for x in pieces],
            "manifest_SHA256":hashlib.sha256(raw).hexdigest(),
            "rejected_higher_or_partial_stages":rejected,
            "selection_is_promotion_authority":False,
            "guest_executed":False,
            "guest_mutated":False,
        }
        return selection,manifest
    raise SystemExit("No fully preserved C91 checkpoint found")


def write_runtime(selection,manifest,directory):
    directory=Path(directory)
    directory.mkdir(parents=True,exist_ok=True)
    transport=directory/"c91-selected-transport.json"
    lock=directory/"c91-selected-artifacts.json"
    transport.write_text(json.dumps(manifest,indent=2)+"\n")
    lock_value={
        "schema":"reverseandroid-runtime-selected-artifacts-v1",
        "repository":REPO,
        "artifacts":{
            "seed":{
                "transport_manifest":str(transport.relative_to(ROOT)),
                "pieces":[{"id":x["id"],"repository":REPO,"sha256":x["sha256"]} for x in selection["piece_artifacts"]],
                "producer_run_id":RUN_ID,
                "producer_execution_commit":selection["source_commit"],
            },
            "metadata":{
                "id":selection["metadata_artifact"]["id"],
                "repository":REPO,
                "sha256":selection["metadata_artifact"]["sha256"],
            },
        },
        "runtime_selection":selection,
    }
    lock.write_text(json.dumps(lock_value,indent=2)+"\n")
    return lock,transport


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--runtime-dir",default="state/experiments/runtime-c91-selection")
    args=parser.parse_args()
    os.chdir(ROOT)
    selection,manifest=select()
    lock,transport=write_runtime(selection,manifest,ROOT/args.runtime_dir)
    output={**selection,"runtime_lock":str(lock.relative_to(ROOT)),"runtime_transport":str(transport.relative_to(ROOT))}
    print(json.dumps(output,indent=2))
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"],"a") as out:
            out.write(f"stage={selection['stage']}\n")
            out.write(f"checkpoint_sha256={selection['checkpoint_sha256']}\n")
            out.write(f"instructions={selection['instructions']}\n")
            out.write(f"lock={lock.relative_to(ROOT)}\n")


if __name__=="__main__":
    main()
