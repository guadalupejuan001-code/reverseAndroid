#!/usr/bin/env python3
"""Produce a UI replay recipe only from an authenticated, restored original CI boot boundary."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import zipfile

REPO=Path(__file__).resolve().parents[1]
REPOSITORY='guadalupejuan001-code/reverseAndroid'
NATIVE='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'


def read_member(package,basename):
    found=[x for x in package.infolist() if Path(x.filename).name==basename and not x.is_dir()]
    assert len(found)==1 and found[0].file_size<=16*1024*1024, f'Ambiguous or oversized {basename}'
    return package.read(found[0])


def recipe(archive,artifacts,bootstrap,expected_run,execution_commit):
    assert isinstance(expected_run,int) and expected_run>0 and re.fullmatch('[0-9a-f]{40}',execution_commit)
    with zipfile.ZipFile(archive) as package:
        assert package.testzip() is None
        raw=read_member(package,'foreground-control-proof.json');control=json.loads(raw)
        assert control['complete'] is True and control['failed'] is False
        assert control['actual_original_shares_restored'] is True and control['restore_reply']['success'] is True
        assert control['original_boot_completed'] is True and control['original_system_server_alive'] is True
        assert control['live_peripherals_verified'] is True and control['native_SHA256']==NATIVE
        stage=control['last_stage'];assert isinstance(stage,int) and 1<=stage<=16
        metadata=next(a for a in artifacts['artifacts'] if a['name']==f'reverseandroid-foreground-control-{stage}-metadata')
        assert not metadata['expired'] and metadata['workflow_run']['id']==expected_run
        assert metadata['workflow_run']['head_sha']==execution_commit
        with Path(archive).open('rb') as stream:assert 'sha256:'+hashlib.file_digest(stream,'sha256').hexdigest()==metadata['digest']
        manifest=json.loads(read_member(package,'manifest.json'));gate=json.loads(read_member(package,'c46-progress.json'))
        contract=json.loads(read_member(package,'output-contract.json'));host=json.loads(read_member(package,'process-outcome.json'))
        kernel=json.loads(read_member(package,'kernel-observation.json'));threads=json.loads(read_member(package,'threads-readonly.json'))
        frame=json.loads(read_member(package,'checkpoint-frame.json'));ppm=read_member(package,'checkpoint-frame.ppm')
        assert manifest['source_contract']==contract and manifest['progress']==gate
        sha=control['checkpoint_sha256'];counter=control['instructions']
        for observed in [gate,contract,kernel,threads]:
            assert observed['checkpoint_sha256']==sha and observed['instructions']==counter
        assert gate['source_boundary_verified'] and gate['system_server_alive_verified'] and gate['android_boot_completed_verified']
        assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
        assert contract['source_commit']==execution_commit
        assert kernel['input_unchanged'] and threads['input_unchanged']
        security=kernel['live_kernel_security']
        assert all(security[x] is True for x in ['btf_layout_verified','selinux_initialized','selinux_enforcing','policy_present'])
        assert frame['source_checkpoint_sha256']==sha and frame['status']=='scanout_exported'
        assert hashlib.sha256(ppm).hexdigest()==frame['ppm_sha256']
        system=[x for x in kernel['tasks'] if x['exit_state']==0 and x.get('zygote_startup',{}).get('argv_verified')
                and x['zygote_startup']['argv']==['system_server'] and x['zygote_startup']['exe_path']=='/system/bin/app_process64']
        assert len(system)==1
        pid=system[0]['pid'];ss=next(g for g in threads['groups'] if g['pid']==pid);assert ss['thread_list_verified']
        leader=next(t for t in ss['threads'] if t['pid']==pid)
        assert leader['exit_state']==0 and leader['scheduling']['credentials']['uid']==leader['scheduling']['credentials']['euid']=='1000'
        top=next(g for g in threads['groups'] if g['pid']==system[0]['zygote_startup']['parent_pid'])
        for group,path in [(ss,'/foreground'),(top,'/top-app')]:
            task=next(t for t in group['threads'] if t['pid']==group['pid']);scheduling=task['scheduling']
            assert scheduling['task_and_runqueue_group_match'] and scheduling['ancestry_verified']
            assert next(g for g in scheduling['groups'] if g['path']==path)['shares_raw']=='1048576'
        pieces=[]
        assert 1<=len(manifest['parts'])<=16
        for i,part in enumerate(manifest['parts']):
            assert part['file']==f'checkpoint.part-{i:03d}' and 0<part['bytes']<=200*1024*1024
            artifact=next(a for a in artifacts['artifacts'] if a['name']==f'reverseandroid-foreground-control-{stage}-{i:03d}')
            assert not artifact['expired'] and artifact['workflow_run']['id']==expected_run
            assert artifact['workflow_run']['head_sha']==execution_commit
            assert re.fullmatch('sha256:[0-9a-f]{64}',artifact['digest'])
            pieces.append({'id':artifact['id'],'repository':REPOSITORY,'sha256':artifact['digest'][7:]})
        source={'schema':'reverseandroid-settings-ui-source-v1','checkpoint_sha256':sha,'instructions':counter,
                'control_proof_file':'foreground-control-proof.json','control_proof_SHA256':hashlib.sha256(raw).hexdigest(),
                'console_SHA256':control['combined_console_SHA256'],'before_frame_ppm_SHA256':frame['ppm_sha256'],
                'native_SHA256':NATIVE,'system_server_PID':pid,'source_run_id':expected_run,'source_execution_commit':execution_commit,
                'source_metadata_artifact_id':metadata['id'],'source_metadata_ZIP_SHA256':metadata['digest'][7:],
                'original_CI_admission_verified':True,'current_full_RVG_independent_local_reproduction_claimed':False,
                'actual_UI_input_has_executed':False}
        lock=copy.deepcopy(bootstrap)
        assert lock['repository']==REPOSITORY
        lock['artifacts']['seed']={'transport_manifest':'state/experiments/settings-ui-source-transport.json','pieces':pieces}
        lock['source_complete_original_RVG_independently_reproduced_locally']=False
        lock['original_CI_admission_and_all_transport_verified']=True
        lock['explicit_source_foreground_control_run']=expected_run;lock['explicit_source_foreground_control_stage']=stage
        return {'settings-ui-source.json':source,'settings-ui-source-transport.json':manifest,'settings-ui-artifacts.json':lock}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('metadata_zip',type=Path);p.add_argument('artifacts_json',type=Path)
    p.add_argument('--expected-run',type=int,required=True);p.add_argument('--execution-commit',required=True)
    p.add_argument('--bootstrap-lock',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();assert a.metadata_zip.stat().st_size<=16*1024*1024
    result=recipe(a.metadata_zip,json.loads(a.artifacts_json.read_text()),json.loads(a.bootstrap_lock.read_text()),a.expected_run,a.execution_commit)
    a.output.mkdir(parents=True,exist_ok=True)
    for name,value in result.items():
        with (a.output/name).open('x') as stream:stream.write(json.dumps(value,indent=2)+'\n')
    print(json.dumps({'UI_source_recipe_prepared':True,'input_has_not_executed':True,'source':result['settings-ui-source.json']}))


if __name__=='__main__':main()
