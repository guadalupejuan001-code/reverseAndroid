"""Validate both a console wrapper and the exact guest shell argument it passes."""
import shlex
import subprocess


def extract_su_shell_body(command):
    text = command.decode('ascii') if isinstance(command, bytes) else command
    # Plain shlex.split merges an adjacent, unquoted outer ';' into the
    # preceding argument. Shell punctuation must remain separate tokens.
    lexer = shlex.shlex(text, posix=True, punctuation_chars=';&|()<>')
    lexer.whitespace_split = True
    lexer.commenters = ''
    words = list(lexer)
    if len(words) < 6 or words[:4] != ['su', '0', 'sh', '-c'] or words[5] != ';':
        raise ValueError('Expected one su 0 sh -c argument followed by its status wrapper')
    return words[4]


def validate_nested_console_command(command, shells=('sh', 'bash')):
    text = command.decode('ascii') if isinstance(command, bytes) else command
    body = extract_su_shell_body(text)
    for shell in shells:
        for scope, source in [('outer console command', text), ('actual guest sh -c argument', body)]:
            result = subprocess.run([shell, '-n'], input=source.encode('ascii'), capture_output=True)
            if result.returncode:
                raise ValueError(f'{shell} rejects {scope}: {result.stderr.decode("utf8", errors="replace").strip()}')
    return body
