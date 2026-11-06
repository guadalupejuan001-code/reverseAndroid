#!/usr/bin/env python3
"""Exercise UI recipe gates with synthetic archive fixtures, never guest snapshots."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

REPO=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ready_recipe',REPO/'scripts/prepare-settings-ui-source.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)


class ReadySourceGates(unittest.TestCase):
    def evaluate(self,change=None,artifact_change=None):
        # Deliberately small synthetic metadata. No RVG or native engine is
        # present, and these test artifact IDs are never sent to GitHub.
        sha='a'*64;commit='b'*40;counter=123;pid=6512;parent=5629;run=987
        control={'complete':True,'failed':False,'actual_original_shares_restored':True,
                 'restore_reply':{'success':True},'original_boot_completed':True,
                 'original_system_server_alive':True,'live_peripherals_verified':True,
                 'native_SHA256':module.NATIVE,'last_stage':2,'checkpoint_sha256':sha,
                 'instructions':counter,'combined_console_SHA256':'c'*64}
        gate={'checkpoint_sha256':sha,'instructions':counter,'source_boundary_verified':True,
              'system_server_alive_verified':True,'android_boot_completed_verified':True}
        contract={'checkpoint_sha256':sha,'instructions':counter,'source_commit':commit}
        groups=[]
        for p,path,uid in [(pid,'/foreground','1000'),(parent,'/top-app','0')]:
            groups.append({'pid':p,'thread_list_verified':True,'threads':[{'pid':p,'exit_state':0,
                'scheduling':{'credentials':{'uid':uid,'euid':uid},'task_and_runqueue_group_match':True,
                'ancestry_verified':True,'groups':[{'path':path,'shares_raw':'1048576'}]}}]})
        ppm=b'P6\n1 1\n255\n\x00\x00\x00'
        files={'foreground-control-proof.json':control,'c46-progress.json':gate,'output-contract.json':contract,
            'process-outcome.json':{'status':'process_completed','exit_code':0,'forced_kill':False},
            'kernel-observation.json':{'checkpoint_sha256':sha,'instructions':counter,'input_unchanged':True,
               'live_kernel_security':dict.fromkeys(['btf_layout_verified','selinux_initialized','selinux_enforcing','policy_present'],True),
               'tasks':[{'pid':pid,'exit_state':0,'zygote_startup':{'argv_verified':True,'argv':['system_server'],
                        'exe_path':'/system/bin/app_process64','parent_pid':parent}}]},
            'threads-readonly.json':{'checkpoint_sha256':sha,'instructions':counter,'input_unchanged':True,'groups':groups},
            'checkpoint-frame.json':{'source_checkpoint_sha256':sha,'status':'scanout_exported','ppm_sha256':hashlib.sha256(ppm).hexdigest()},
            'manifest.json':{'source_contract':dict(contract),'progress':dict(gate),
               'parts':[{'file':'checkpoint.part-000','bytes':40,'sha256':'d'*64}]}}
        if change:change(files)
        with tempfile.TemporaryDirectory(prefix='ui-metadata-fixture-') as temporary:
            archive=Path(temporary)/'synthetic-metadata.zip'
            with zipfile.ZipFile(archive,'w') as package:
                for name,value in files.items():package.writestr('synthetic/'+name,json.dumps(value))
                package.writestr('synthetic/checkpoint-frame.ppm',ppm)
            entries=[]
            for id,name,digest in [(11,'reverseandroid-foreground-control-2-metadata',hashlib.sha256(archive.read_bytes()).hexdigest()),
                                   (12,'reverseandroid-foreground-control-2-000','e'*64)]:
                entries.append({'id':id,'name':name,'digest':'sha256:'+digest,'expired':False,
                                'workflow_run':{'id':run,'head_sha':commit}})
            artifacts={'artifacts':entries}
            if artifact_change:artifact_change(artifacts)
            return module.recipe(archive,artifacts,{'repository':module.REPOSITORY,'artifacts':{}},run,commit)

    def test_valid_metadata_produces_recipe_without_claiming_executed_UI(self):
        result=self.evaluate();self.assertFalse(result['settings-ui-source.json']['actual_UI_input_has_executed'])
        self.assertFalse(result['settings-ui-source.json']['current_full_RVG_independent_local_reproduction_claimed'])
        self.assertEqual(result['settings-ui-artifacts.json']['artifacts']['seed']['pieces'][0]['id'],12)

    def test_completion_boolean_cannot_override_original_failed_boot_gate(self):
        for name,key in [('c46-progress.json','android_boot_completed_verified'),
                         ('c46-progress.json','system_server_alive_verified'),
                         ('foreground-control-proof.json','actual_original_shares_restored')]:
            with self.subTest(key=key):
                with self.assertRaises(AssertionError):self.evaluate(lambda f:f[name].update({key:False}))

    def test_requires_enforcing_security_real_UID_and_both_original_weights(self):
        cases=[lambda f:f['kernel-observation.json']['live_kernel_security'].update(selinux_enforcing=False),
               lambda f:f['threads-readonly.json']['groups'][0]['threads'][0]['scheduling']['credentials'].update(uid='0'),
               lambda f:f['threads-readonly.json']['groups'][1]['threads'][0]['scheduling']['groups'][0].update(shares_raw='16777216'),
               lambda f:f['process-outcome.json'].update(forced_kill=True)]
        for change in cases:
            with self.assertRaises(AssertionError):self.evaluate(change)

    def test_rejects_foreign_or_changed_metadata_producer(self):
        for change in [lambda a:a['artifacts'][0]['workflow_run'].update(id=1),
                       lambda a:a['artifacts'][0].update(digest='sha256:'+'0'*64),
                       lambda a:a['artifacts'][1]['workflow_run'].update(head_sha='f'*40)]:
            with self.assertRaises(AssertionError):self.evaluate(artifact_change=change)
        with self.assertRaises(StopIteration):self.evaluate(artifact_change=lambda a:a['artifacts'].pop())


if __name__=='__main__':unittest.main()
