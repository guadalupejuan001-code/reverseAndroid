#!/usr/bin/env python3
"""Reversible, source-bound foreground CPU-group allocation for the actual SystemServer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from c85_peripheral_completion import verify_live_peripherals, boot_complete
from console_command_syntax import validate_nested_console_command

REPO=Path(__file__).resolve().parents[1]


def read(path):return json.loads(Path(path).read_text())


def save(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n')


def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def boost_command(pid):
    assert isinstance(pid,int) and pid>1
    return (f'\nsu 0 sh -c \'d=/proc/{pid}; IFS= read -r n < "$d/comm"; '
            '[ "$n" = system_server ] || exit 81; tgid=; uid=; '
            'while read -r k v rest; do case "$k" in Tgid:) tgid=$v;; Uid:) uid=$v;; esac; done < "$d/status"; '
            f'[ "$tgid" = {pid} ] && [ "$uid" = 1000 ] || exit 82; '
            'found=0; while IFS=: read -r id cs path; do case ",$cs," in *,cpu,*) '
            '[ "$path" = /foreground ] || exit 83; found=1;; esac; done < "$d/cgroup"; '
            '[ "$found" = 1 ] || exit 84; f=/dev/cpuctl/foreground/cpu.shares; '
            'IFS= read -r b < "$f"; [ "$b" = 1024 ] || exit 85; '
            'IFS= read -r t < /dev/cpuctl/top-app/cpu.shares; [ "$t" = 16384 ] || exit 86; '
            'printf "%s\\n" 16384 > "$f" || exit 87; IFS= read -r a < "$f"; '
            '[ "$a" = 16384 ] || exit 88; printf "\\nREVERSE_%s SHARES_%s\\n" CFS_FOREGROUND_BOOST_DONE "$a"\'; '
            'r=$?; printf \'\\nREVERSE_%s RC_%s\\n\' CFS_FOREGROUND_BOOST_RESULT "$r"\n')


def restore_command():
    return ('\nsu 0 sh -c \'f=/dev/cpuctl/foreground/cpu.shares; t=/dev/cpuctl/top-app/cpu.shares; '
            'IFS= read -r a < "$f"; IFS= read -r b < "$t"; '
            '[ "$a" = 16384 ] && [ "$b" = 16384 ] || exit 91; '
            'printf "%s\\n" 1024 > "$f" || exit 92; printf "%s\\n" 1024 > "$t" || exit 93; '
            'IFS= read -r a < "$f"; IFS= read -r b < "$t"; '
            '[ "$a" = 1024 ] && [ "$b" = 1024 ] || exit 94; '
            'printf "\\nREVERSE_%s SHARES_%s\\n" CFS_FOREGROUND_RESTORE_DONE "$a"\'; '
            'r=$?; printf \'\\nREVERSE_%s RC_%s\\n\' CFS_FOREGROUND_RESTORE_RESULT "$r"\n')


def reply(stdout,phase):
    lines=[x.strip() for x in stdout.splitlines()]
    codes=[int(m[1]) for x in lines if (m:=re.fullmatch(rf'REVERSE_CFS_FOREGROUND_{phase}_RESULT RC_([0-9]+)',x))]
    shares=16384 if phase=='BOOST' else 1024
    return {'seen':bool(codes),'failed':any(x!=0 for x in codes),'success':bool(codes) and all(x==0 for x in codes)
            and f'REVERSE_CFS_FOREGROUND_{phase}_DONE SHARES_{shares}' in lines}


def observe_groups(probe,plan):
    assert probe['input_unchanged'] and probe['original_CPU_and_codecs_unchanged']
    selected={}
    for pid,path,pointer,uid in [(plan['system_server_PID'],'/foreground',plan['foreground_task_group'],'1000'),
                                 (plan['zygote_PID'],'/top-app',plan['topapp_task_group'],'0')]:
        g=next(g for g in probe['groups'] if g['pid']==pid);assert g['thread_list_verified']
        t=next(t for t in g['threads'] if t['pid']==pid)
        assert t['exit_state']==0 and t['tgid']==pid
        if uid=='1000':assert t['name']=='system_server'
        s=t['scheduling'];assert s['task_and_runqueue_group_match'] and s['ancestry_verified']
        assert s['credentials']['uid']==s['credentials']['euid']==uid
        c=next(x for x in s['groups'] if x['path']==path)
        assert c['task_group']==pointer and c['quota_raw']=='18446744073709551615' and c['period_raw']=='100000000'
        assert s['groups'][-1]['path']=='/' and s['groups'][-1]['shares_raw']=='1048576'
        selected[path]=c
    return selected


def fresh_probe(runtime,source,sha,out):
    subprocess.run(['node','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-thread-probe.mjs'),
                    str(source/'current.rvg'),sha,str(out)],check=True)
    probe=read(out);assert probe['checkpoint_sha256']==sha
    return probe


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('stage',type=int)
    p.add_argument('--runtime-root',type=Path,required=True);p.add_argument('--source',type=Path,required=True)
    p.add_argument('--output-root',type=Path,required=True);p.add_argument('--commit',required=True)
    args=p.parse_args();assert re.fullmatch('[0-9a-f]{40}',args.commit)
    plan=read(REPO/'state/experiments/foreground-shares-plan.json');assert 1<=args.stage<=plan['max_stages']
    runtime,root=args.runtime_root.resolve(),args.output_root.resolve();reports=root/'reports';reports.mkdir(parents=True,exist_ok=True)
    state_file=reports/'control-state.json'
    if args.stage==1:
        assert not state_file.exists(),'Never repeat an authenticated input'
        source=args.source.resolve();contract=read(source/'output-contract.json');old=read(source/'topapp-control-proof.json')
        assert contract['checkpoint_sha256']==digest(source/'current.rvg')==plan['source_checkpoint_sha256']
        assert contract['instructions']==plan['source_instructions'] and contract['epoch_sha256']==plan['source_epoch_SHA256']
        assert digest(source/'topapp-control-proof.json')==plan['source_control_proof_SHA256']
        assert not old['failed'] and not old['complete'] and old['boost_reply']['success'] and old['actual_boosted_group_verified']
        assert old['original_system_server_alive'] and not old['restore_issued'] and len(old['new_events'])==1
        assert old['checkpoint_sha256']==plan['source_checkpoint_sha256']
        assert digest(source/'console-all.bin')==old['combined_console_SHA256']==plan['source_console_SHA256']
        gate=read(source/'c46-progress.json');assert gate['source_boundary_verified'] and gate['system_server_alive_verified']
        kernel=read(source/'kernel-observation.json');ss=next(t for t in kernel['tasks'] if t['pid']==plan['system_server_PID'])
        assert ss['exit_state']==0 and ss['zygote_startup']['argv_verified']
        assert ss['zygote_startup']['argv']==['system_server'] and ss['zygote_startup']['exe_path']=='/system/bin/app_process64'
        assert ss['zygote_startup']['parent_pid']==plan['zygote_PID']
        before=fresh_probe(runtime,source,plan['source_checkpoint_sha256'],reports/'source-threads.json')
        actual=observe_groups(before,plan)
        assert actual['/foreground']['shares_raw']=='1048576' and actual['/top-app']['shares_raw']=='16777216'
        shutil.copy2(source/'console-all.bin',reports/'console-all.bin')
        shutil.copy2(source/'topapp-control-proof.json',reports/'source-topapp-control-proof.json')
        state={'source':str(source),'last_stage':0,'new_events':[],'inherited_topapp_control_proof_SHA256':plan['source_control_proof_SHA256'],
               'inherited_C87_events':old['inherited_C87_events'],'inherited_JIT_events':old['inherited_JIT_events'],
               'inherited_topapp_events':old['new_events'],'original_boot_completed':gate['android_boot_completed_verified'],
               'live_peripherals_verified':old['live_peripherals_verified'],'restore_issued':False,'failed':False,'complete':False}
    else:
        state=read(state_file);source=Path(state['source'])
        assert state['last_stage']==args.stage-1 and not state['failed'] and not state['complete'] and 1<=len(state['new_events'])<=2
    native=runtime/'build/ic-native/epoch-clz-ic';assert digest(native)==plan['native_SHA256']
    uart='-';purpose=None
    if args.stage==1:command=boost_command(plan['system_server_PID']);purpose='boost'
    elif (state['original_boot_completed'] and state['live_peripherals_verified'] and not state['restore_issued']
          and state['boost_reply']['success'] and state['actual_both_trial_shares_verified']):
        command=restore_command();purpose='restore';state['restore_issued']=True
    if purpose:
        validate_nested_console_command(command);payload=command.encode('ascii');input_file=reports/f'stage-{args.stage}-{purpose}.txt';input_file.write_bytes(payload)
        assert hashlib.sha256(payload).hexdigest()==plan[purpose+'_input_SHA256']
        assert len(payload)==plan[purpose+'_input_bytes']
        contract=read(source/'output-contract.json');uart=str(reports/f'stage-{args.stage}-{purpose}-plan.json')
        save(uart,{'schema':'reverseandroid-authenticated-console-input-plan-v1','purpose':'diagnostic_console',
                   'source_checkpoint_sha256':contract['checkpoint_sha256'],'source_instructions':contract['instructions'],
                   'input_file':input_file.name,'input_sha256':hashlib.sha256(payload).hexdigest(),'input_bytes':len(payload)})
    output=root/f'stage-{args.stage}';budget=plan['first_budget'] if args.stage==1 else plan['continuation_budget']
    with (reports/f'stage-{args.stage}.json').open('x') as log:
        subprocess.run(['node','--expose-gc','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-epoch.mjs'),'run',str(source),
                        str(output),str(native),str(runtime/'build/media/android-c27.raw'),str(runtime/'build/reverseandroid/frame'),
                        args.commit,str(budget),uart],cwd=runtime,stdout=log,check=True)
    gate,host=read(output/'c46-progress.json'),read(output/'process-outcome.json')
    assert gate['source_boundary_verified'] and digest(output/'current.rvg')==gate['checkpoint_sha256']
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    provenance=read(output/'current.json')['provenance']
    if purpose:
        event=read(output/'result.json')['external_uart_input'];expected=read(uart)
        assert event['sha256']==expected['input_sha256'] and event['bytes']==expected['input_bytes']
        assert event['instructions']==read(source/'output-contract.json')['instructions']
        state['new_events'].append({'purpose':purpose,'actual_native_event':event})
    else:assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
    raw=reports/'console-all.bin'
    with raw.open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
    normalizer=('import {readFileSync,writeFileSync} from "node:fs"; '
                f'import {{normalizeMultiplexedConsole}} from {json.dumps((runtime/"tools/reverseandroid-console-proof.mjs").as_uri())}; '
                'writeFileSync(process.argv[2],normalizeMultiplexedConsole(readFileSync(process.argv[1])).stdout,"latin1");')
    normalized=reports/'console-normalized.txt'
    subprocess.run(['node','--input-type=module','-e',normalizer,str(raw),str(normalized)],check=True)
    stdout=normalized.read_text(encoding='latin1');state['boost_reply']=reply(stdout,'BOOST');state['restore_reply']=reply(stdout,'RESTORE')
    state['failed']=state['boost_reply']['failed'] or state['restore_reply']['failed']
    probe=fresh_probe(runtime,output,gate['checkpoint_sha256'],output/'threads-readonly.json');actual=observe_groups(probe,plan)
    state['actual_both_trial_shares_verified']=all(g['shares_raw']=='16777216' for g in actual.values())
    state['actual_original_shares_restored']=(state['restore_issued'] and state['restore_reply']['success']
                                             and all(g['shares_raw']=='1048576' for g in actual.values()))
    assert all(g['shares_raw'] in ['1048576','16777216'] for g in actual.values())
    if not state['restore_issued']:
        assert actual['/top-app']['shares_raw']=='16777216'
        if state['boost_reply']['success']:assert state['actual_both_trial_shares_verified']
    subprocess.run(['node',str(runtime/'tools/reverseandroid-console-proof.mjs'),str(raw),
                    '548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b',
                    '/data/local/tmp/reverseandroid-peer-abi3.elf',str(output/'console-observation.json')],check=True)
    subprocess.run(['node','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-peripheral-probe.mjs'),str(output/'current.rvg'),
                    gate['checkpoint_sha256'],str(output/'peripherals-live-required.json'),'/data/local/tmp/reverseandroid-peer-abi3.elf'],check=True)
    live=verify_live_peripherals(read(output/'peripherals-live-required.json'),read(output/'console-observation.json'),gate['checkpoint_sha256'])
    state.update(source=str(output),last_stage=args.stage,checkpoint_sha256=gate['checkpoint_sha256'],instructions=gate['instructions'],
                 original_boot_completed=gate['android_boot_completed_verified'],original_system_server_alive=gate['system_server_alive_verified'],
                 live_peripherals_verified=live,native_SHA256=plan['native_SHA256'],combined_console_SHA256=digest(raw),
                 original_engine_and_codecs_preserved=True,usable_UI_response_verified=False,fresh_hardware_power_cycle=False)
    state['complete']=not state['failed'] and state['actual_original_shares_restored'] and boot_complete(True,live,gate)
    save(output/'foreground-control-proof.json',state);save(state_file,state);shutil.copy2(raw,output/'console-all.bin')
    shutil.rmtree(output/'native/journal')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('stop='+str(state['failed'] or state['complete']).lower()+'\n')
    print(json.dumps({k:state[k] for k in ['last_stage','checkpoint_sha256','instructions','boost_reply','restore_reply',
          'actual_both_trial_shares_verified','original_system_server_alive','original_boot_completed','complete','failed']}))


if __name__=='__main__':main()
