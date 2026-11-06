"""Fail closed before an explicit ordinary-OS cleanup priority input.

The complete original source admission remains mandatory in the controller.
This supplemental read-only proof authorizes only the exact dying worker.
"""


def verify(contract, gate, host, kernel, state, threads, plan):
    sha = plan['source_checkpoint_sha256']
    count = plan['source_instructions']
    assert contract['checkpoint_sha256'] == gate['checkpoint_sha256'] == state['last_admitted_sha256'] == threads['checkpoint_sha256'] == sha
    assert contract['instructions'] == gate['instructions'] == state['last_admitted_instructions'] == threads['instructions'] == count
    assert gate['source_boundary_verified'] and not gate['system_server_alive_verified'] and not gate['android_boot_completed_verified']
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and not host['forced_kill']
    assert kernel['live_kernel_security']['selinux_enforcing']
    assert not state['failed'] and not state['complete']
    assert state['phase'] == plan['inherited_source_phase'] == 5 and state['waiting'] is False
    assert state['events'] == plan['inherited_source_events'] and len(state['events']) == 5
    assert [e['phase'] for e in state['events']] == list(range(5))
    assert state['explicit_IC_IVAU_correction'] and state['IC_IVAU_validation_run'] == 42221035217
    assert state['native_binary_SHA256'] == plan['expected_extended_native_SHA256']
    assert threads['read_only'] and threads['input_unchanged'] and threads['original_observer_file_unchanged']
    assert threads['original_CPU_and_codecs_unchanged'] and threads['kernel_layout_pinned']
    group, = [g for g in threads['groups'] if g['pid'] == 562]
    assert group['thread_list_verified'] and group['thread_count'] == len(group['threads']) == 2
    assert group['leader_has_no_user_mm'] and not group.get('error')
    leader, = [t for t in group['threads'] if t['pid'] == 562]
    worker, = [t for t in group['threads'] if t['pid'] == 3387]
    assert leader['tgid'] == 562 and leader['exit_state'] == 32 and leader['signals']['exit_code_raw'] == '9'
    assert leader['signals']['exit_signal_raw'] == '17' and leader['signals']['parent']['pid'] == 1
    assert worker['tgid'] == 562 and worker['name'] == 'Jit thread pool' and worker['exit_state'] == 0
    assert worker['state'] == 0 and worker['on_rq'] == 1 and worker['on_cpu'] == 0
    assert worker['priority'] == worker['prio'] == worker['normal_prio'] == 139
    assert worker['policy'] == 0 and worker['rt_priority'] == 0
    scheduling = worker['scheduling']
    assert scheduling['task_and_runqueue_group_match'] and scheduling['ancestry_verified']
    assert scheduling['credentials'] == dict(uid='0',euid='0',gid='0',egid='0')
    assert scheduling['groups'][0]['path'] == '/top-app'
    assert worker['signals']['ptrace_raw'] == '0' and worker['signals']['group_exit_code_raw'] == '9'
    assert int(worker['signals']['flags_raw']) & 4
    symbols = {s['symbol'].split('+')[0] for s in worker['kernel_stack'] if s['symbol']}
    assert {'exit_mmap','do_exit','do_group_exit','get_signal'} <= symbols
    return dict(source_checkpoint_sha256=sha,instructions=count,thread_list_verified=True,
                dying_leader_PID=562,last_cleanup_TID=3387,current_nice=19,
                guarded_relative_renice=-19,waiting_input_finished=True,
                source_events_preserved=5,snapshot_or_clock_change=False)
