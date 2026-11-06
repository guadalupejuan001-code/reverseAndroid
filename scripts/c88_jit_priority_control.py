"""Strict real-console acknowledgement for one guarded ordinary Android repair."""
import hashlib,json,pathlib,re

def read(path):return json.loads(pathlib.Path(path).read_text())
def save(path,value):pathlib.Path(path).write_text(json.dumps(value,indent=2)+'\n')
def digest(path):
    with pathlib.Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def validate_plan(p):
    for key in ['initial_instruction_budget','continuation_instruction_budget']:
        assert type(p[key]) is int and 0<p[key]<=1000000000
    assert p['host_wall_ms']==900000 and p['max_stages']==16
    assert p['source_main_PID']==5629 and p['target_JIT_TID']==5922
    assert p['source_main_state']==8193 and p['new_input_phase']==7 and p['new_phase_after_real_ACK']==8
    assert p['inherited_source_phase']==7 and p['inherited_source_waiting'] is False
    assert len(p['inherited_source_events'])==7 and [e['phase'] for e in p['inherited_source_events']]==list(range(7))
    assert p['selected_native_unchanged_from_C87'] and p['old_inputs_must_not_be_replayed']
    assert not p['fresh_hardware_power_cycle'] and not p['guest_property_injection'] and not p['guest_snapshot_patching']

def parse_reply(raw):
    # Authenticate raw bytes separately; normalize terminal CR/CSI only for exact reply lines.
    clean=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',raw).replace(b'\r',b'')
    codes=[int(x) for x in re.findall(rb'^REVERSE_JIT5922_CONTROL_RESULT RC_([0-9]+)$',clean,re.M)]
    done=len(re.findall(rb'^REVERSE_JIT5922_CONTROL_DONE NICE_0$',clean,re.M))
    assert len(codes)<=1 and done<=1,'Repeated repair reply; input must be applied exactly once'
    return dict(reply_seen=bool(codes),reply_code=codes[0] if codes else None,
                actual_nice0_reply_seen=bool(done),guard_failed=bool(codes and codes[0]!=0),
                guard_succeeded=codes==[0] and done==1)
