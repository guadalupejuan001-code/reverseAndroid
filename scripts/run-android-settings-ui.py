#!/usr/bin/env python3
"""Source-bound Android Settings validation after original completed boot and shares restoration."""
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

REPO = Path(__file__).resolve().parents[1]


def read(path):
    return json.loads(Path(path).read_text())


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def ui_command():
    return ('\nsu 0 sh -c \'cmd input keyevent KEYCODE_WAKEUP || exit 71; '
            'cmd window dismiss-keyguard || exit 72; '
            'cmd activity start-activity -W --user 0 -a android.settings.SETTINGS || exit 73; '
            'printf "\\nREVERSE_%s\\n" UI_SETTINGS_DUMP_BEGIN; '
            'dumpsys activity activities; r=$?; '
            'printf "\\nREVERSE_%s\\n" UI_SETTINGS_DUMP_END; exit "$r"\'; '
            'r=$?; printf \'\\nREVERSE_%s RC_%s\\n\' UI_SETTINGS_LAUNCH_RESULT "$r"\n')


def ui_reply(stdout):
    lines = [x.strip() for x in stdout.splitlines()]
    codes = [int(m[1]) for x in lines if (m := re.fullmatch(r'REVERSE_UI_SETTINGS_LAUNCH_RESULT RC_([0-9]+)', x))]
    begin = [i for i,x in enumerate(lines) if x == 'REVERSE_UI_SETTINGS_DUMP_BEGIN']
    end = [i for i,x in enumerate(lines) if x == 'REVERSE_UI_SETTINGS_DUMP_END']
    block = lines[begin[0]+1:end[0]] if len(begin)==len(end)==1 and begin[0]<end[0] else []
    resumed = [x for x in block if re.search(r'(?:mResumedActivity|topResumedActivity|ResumedActivity):?\s*[=:]?\s*.*\bu0\s+com\.android\.settings/', x)]
    activity = [x for x in lines if re.fullmatch(r'Activity:\s+com\.android\.settings/[^\s]+', x)]
    status_ok = 'Status: ok' in lines and 'Status: timeout' not in lines
    return {'seen':bool(codes),'failed':any(x!=0 for x in codes),
            'success':bool(codes) and all(x==0 for x in codes) and status_ok and bool(activity) and bool(resumed),
            'status_ok':status_ok,'activity':activity,'resumed':resumed}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('stage', type=int); p.add_argument('--runtime-root', type=Path, required=True)
    p.add_argument('--source', type=Path, required=True); p.add_argument('--output-root', type=Path, required=True)
    p.add_argument('--commit', required=True)
    args = p.parse_args(); assert 1 <= args.stage <= 12 and re.fullmatch('[0-9a-f]{40}',args.commit)
    runtime, root = args.runtime_root.resolve(),args.output_root.resolve()
    reports=root/'reports'; reports.mkdir(parents=True,exist_ok=True); state_file=reports/'control-state.json'
    pin=read(REPO/'state/experiments/settings-ui-source.json')
    if args.stage==1:
        assert not state_file.exists(), 'UI launch may be issued only once'
        source=args.source.resolve(); before=read(source/'checkpoint-frame.json'); gate=read(source/'c46-progress.json')
        control=read(source/'topapp-control-proof.json')
        assert control['complete'] and control['actual_original_shares_restored'] and control['restore_reply']['success']
        assert boot_complete(True,control['live_peripherals_verified'],gate)
        assert digest(source/'current.rvg') == pin['checkpoint_sha256'] == gate['checkpoint_sha256']
        assert gate['instructions'] == pin['instructions']
        assert digest(source/'topapp-control-proof.json') == pin['control_proof_SHA256']
        assert digest(source/'console-all.bin') == pin['console_SHA256'] == control['combined_console_SHA256']
        assert before['ppm_sha256'] == pin['before_frame_ppm_SHA256'] == digest(source/'checkpoint-frame.ppm')
        assert before['source_checkpoint_sha256'] == pin['checkpoint_sha256']
        shutil.copy2(source/'console-all.bin',reports/'console-all.bin')
        shutil.copy2(source/'checkpoint-frame.ppm',reports/'before-frame.ppm')
        shutil.copy2(source/'checkpoint-frame.json',reports/'before-frame.json')
        state={'source':str(source),'last_stage':0,'new_events':[],'failed':False,'complete':False,
               'candidate_PID':None,'candidate_system_server_PID':None,'before_frame_ppm_SHA256':pin['before_frame_ppm_SHA256'],
               'source_group_control_proof_SHA256':pin['control_proof_SHA256']}
    else:
        state=read(state_file);source=Path(state['source'])
        assert state['last_stage']==args.stage-1 and not state['complete'] and not state['failed'] and len(state['new_events'])==1
    uart='-'
    if args.stage==1:
        command=ui_command();validate_nested_console_command(command)
        payload=command.encode('ascii');input_file=reports/'settings-ui-input.txt';input_file.write_bytes(payload)
        uart=str(reports/'settings-ui-input-plan.json')
        save(uart,{'schema':'reverseandroid-authenticated-console-input-plan-v1','purpose':'ui_response_test',
                   'source_checkpoint_sha256':pin['checkpoint_sha256'],'source_instructions':pin['instructions'],
                   'input_file':input_file.name,'input_sha256':hashlib.sha256(payload).hexdigest(),'input_bytes':len(payload),
                   'before_frame_ppm_sha256':pin['before_frame_ppm_SHA256']})
    native=runtime/'build/ic-native/epoch-clz-ic';assert digest(native)==pin['native_SHA256']
    output=root/f'stage-{args.stage}'
    budget=250_000_000 if state['candidate_PID'] else (500_000_000 if args.stage==1 else 1_000_000_000)
    with (reports/f'stage-{args.stage}.json').open('x') as log:
        subprocess.run(['node','--expose-gc','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-epoch.mjs'),
                        'run',str(source),str(output),str(native),str(runtime/'build/media/android-c27.raw'),
                        str(runtime/'build/reverseandroid/frame'),args.commit,str(budget),uart],cwd=runtime,stdout=log,check=True)
    gate,host=read(output/'c46-progress.json'),read(output/'process-outcome.json')
    assert gate['source_boundary_verified'] and digest(output/'current.rvg')==gate['checkpoint_sha256']
    assert host['status']=='process_completed' and host['exit_code']==0 and not host['forced_kill']
    provenance=read(output/'current.json')['provenance']
    if args.stage==1:
        event=read(output/'result.json')['external_uart_input'];expected=read(uart)
        assert event['sha256']==expected['input_sha256'] and event['bytes']==expected['input_bytes']
        assert event['instructions']==pin['instructions'];state['new_events'].append(event)
    else:
        assert provenance['new_uart_input_bytes']==0 and 'external_uart_input' not in provenance
    with (reports/'console-all.bin').open('ab') as stream:stream.write((output/'console-new.txt').read_bytes())
    raw=reports/'console-all.bin';normalized=reports/'console-normalized.txt'
    normalizer=('import {readFileSync,writeFileSync} from "node:fs"; '
                f'import {{normalizeMultiplexedConsole}} from {json.dumps((runtime/"tools/reverseandroid-console-proof.mjs").as_uri())}; '
                'writeFileSync(process.argv[2],normalizeMultiplexedConsole(readFileSync(process.argv[1])).stdout,"latin1");')
    subprocess.run(['node','--input-type=module','-e',normalizer,str(raw),str(normalized)],check=True)
    reply=ui_reply(normalized.read_text(encoding='latin1'))
    subprocess.run(['node','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-app-process-probe.mjs'),
                    str(output/'current.rvg'),gate['checkpoint_sha256'],str(output/'apps-readonly.json')],check=True)
    apps=read(output/'apps-readonly.json');assert apps['input_unchanged'] and apps['checkpoint_sha256']==gate['checkpoint_sha256']
    subprocess.run(['node',str(runtime/'tools/reverseandroid-console-proof.mjs'),str(raw),
                    '548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b',
                    '/data/local/tmp/reverseandroid-peer-abi3.elf',str(output/'console-observation.json')],check=True)
    subprocess.run(['node','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-peripheral-probe.mjs'),
                    str(output/'current.rvg'),gate['checkpoint_sha256'],str(output/'peripherals-live-required.json'),
                    '/data/local/tmp/reverseandroid-peer-abi3.elf'],check=True)
    live=verify_live_peripherals(read(output/'peripherals-live-required.json'),read(output/'console-observation.json'),gate['checkpoint_sha256'])
    kernel=read(output/'kernel-observation.json');security=kernel['live_kernel_security']
    ss=[x for x in apps['processes'] if x['argv']==['system_server'] and x['credentials']['uid']=='1000']
    settings=[x for x in apps['settings_processes'] if any(x['parent_pid']==y['pid'] for y in apps['processes']
              if y['credentials']['uid']=='0' and ('--zygote' in y['argv'] or y['argv']==['zygote64']))]
    frame=read(output/'checkpoint-frame.json')
    assert frame['source_checkpoint_sha256']==gate['checkpoint_sha256'] and digest(output/'checkpoint-frame.ppm')==frame['ppm_sha256']
    candidate=(reply['success'] and len(settings)==len(ss)==1 and boot_complete(True,live,gate)
               and security['selinux_enforcing'] and security['policy_present']
               and frame['ppm_sha256']!=state['before_frame_ppm_SHA256'])
    stable=bool(candidate and state['candidate_PID']==settings[0]['pid'] and state['candidate_system_server_PID']==ss[0]['pid'])
    state.update(source=str(output),last_stage=args.stage,checkpoint_sha256=gate['checkpoint_sha256'],instructions=gate['instructions'],
                 reply=reply,failed=reply['failed'],complete=stable,
                 candidate_PID=settings[0]['pid'] if candidate else None,candidate_system_server_PID=ss[0]['pid'] if candidate else None,
                 original_boot_completed=gate['android_boot_completed_verified'],original_system_server_alive=gate['system_server_alive_verified'],
                 live_peripherals_verified=live,frame_changed=frame['ppm_sha256']!=state['before_frame_ppm_SHA256'],
                 settings_process_identity_verified=bool(settings),stable_over_second_zero_input_boundary=stable,
                 authentic_UI_response_evidence_verified=stable,visual_review_pending=True,usable_UI_response_verified=False,
                 combined_console_SHA256=digest(raw),original_engine_and_codecs_preserved=True)
    save(output/'settings-ui-proof.json',state);save(state_file,state)
    shutil.copy2(raw,output/'console-all.bin');shutil.rmtree(output/'native/journal')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'],'a') as stream:stream.write('stop='+str(state['failed'] or stable).lower()+'\n')
    print(json.dumps(state))


if __name__=='__main__':
    main()
