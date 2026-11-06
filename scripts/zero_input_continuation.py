"""Bounded continuation configuration; every native stage supplies no new input."""
import re

def validate(p):
    assert p['schema']=='reverseandroid-original-zero-input-continuation-plan-v1'
    assert re.fullmatch(r'[a-z][a-z0-9-]{0,39}',p['cycle'])
    assert p['new_UART_bytes']==0 and type(p['new_UART_bytes']) is int
    assert 'input_file' not in p and 'input_plan_file' not in p
    assert p['host_wall_ms']==900000 and p['max_stages']==16
    for key in ['initial_instruction_budget','continuation_instruction_budget']:
        assert type(p[key]) is int and 0<p[key]<=1000000000
    events=p['inherited_source_events']
    assert 1<=len(events)<=32 and [e['phase'] for e in events]==list(range(len(events)))
    assert p['expected_native_SHA256']=='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
    assert re.fullmatch(r'[0-9a-f]{64}',p['source_checkpoint_sha256'])
    assert type(p['source_instructions']) is int and p['source_instructions']>0
