"""Read-only phase decisions from authenticated, exact console observations."""
import re


def evaluate_console_phase(observed, phase):
    markers = phase.get('response_markers', [phase['response_marker']])
    failures = phase.get('failure_markers', [])
    assert markers and len(set(markers)) == len(markers)
    assert all(re.fullmatch(r'REVERSE_PTS_[A-Z0-9_]+', name) for name in markers + failures)
    responses = observed['actual_response_codes']
    codes = {name: responses.get(name.removeprefix('REVERSE_'), []) for name in set(markers + failures)}
    assert all(isinstance(items, list) for items in codes.values())
    assert all(type(code) is int and 0 <= code <= 255 for items in codes.values() for code in items)
    failed = any(code != 0 for items in codes.values() for code in items)
    missing = [name for name in markers if not codes[name]]
    # A data acknowledgment alone must not permit the next line to be sent
    # before the root receiver has acknowledged its next blocking read.
    return {'failed': failed, 'succeeded': not failed and not missing, 'missing_markers': missing}
