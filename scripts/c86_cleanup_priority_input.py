"""Prepare a bounded ordinary OS priority repair, never a snapshot edit.

The caller must bind this to a fresh admitted source and matching thread proof.
The current C85 input phase must have finished before any such input is sent.
"""
import shlex


def cleanup_body(tid=3387,tgid=562,proc_root="/proc"):
    assert isinstance(tid,int) and isinstance(tgid,int) and tid>1 and tgid>1
    directory=shlex.quote(proc_root.rstrip('/')+'/'+str(tid))
    # Toybox renice -n is RELATIVE. Adding zero repeats the old C71 mistake.
    return (f'd={directory}; '
        'if [ ! -e "$d/status" ]; then printf "\\nREVERSE_%s RC_0\\n" PTS_C86_CLEANUP_ALREADY_ABSENT; exit 0; fi; '
        '[ "$(cat "$d/comm")" = "Jit thread pool" ] || exit 41; '
        'tgid=; uid=; while read -r k v rest; do case "$k" in Tgid:) tgid=$v;; Uid:) uid=$v;; esac; done < "$d/status"; '
        f'[ "$tgid" = {tgid} ] && [ "$uid" = 0 ] || exit 42; '
        'IFS= read -r line < "$d/stat" || exit 43; rest=${line#*) }; set -- $rest; [ "$#" -ge 17 ] || exit 43; '
        'flags=${7}; [ "$((flags & 4))" = 4 ] || exit 44; '
        'if [ "${17}" = 0 ]; then printf "\\nREVERSE_%s RC_0\\n" PTS_C86_CLEANUP_ALREADY_NICE0; exit 0; fi; '
        '[ "${17}" = 19 ] || exit 45; '
        f'renice -n -19 -p {tid} || exit 46; '
        'if [ ! -e "$d/stat" ]; then printf "\\nREVERSE_%s RC_0\\n" PTS_C86_CLEANUP_RENICED_THEN_ABSENT; exit 0; fi; '
        'IFS= read -r line < "$d/stat" || exit 47; rest=${line#*) }; set -- $rest; [ "${17}" = 0 ] || exit 48; '
        'printf "\\nREVERSE_%s RC_0 NICE_%s\\n" PTS_C86_CLEANUP_PRIORITY_APPLIED "${17}"')


def command(tid=3387,tgid=562):
    body=cleanup_body(tid,tgid)
    return ("\nsu 0 sh -c "+shlex.quote(body)+"; r=$?; printf '\\nREVERSE_%s RC_%s\\n' PTS_C86_CLEANUP \"$r\"\n").encode()
