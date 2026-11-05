#!/usr/bin/env python3
"""The original backend is a byte-for-byte oracle for XI's public contract."""
import hashlib, json, subprocess, sys
old=subprocess.check_output([sys.argv[1]],timeout=90)
new=subprocess.check_output([sys.argv[2]],timeout=90)
assert old==new, 'Original and optimized runtime differ in outputs, errors, snapshots or RNG'
print(json.dumps({'suite':'xi_runtime_contract','byte_identical':True,'transcript_bytes':len(new),'sha256':hashlib.sha256(new).hexdigest(),'cases':int(new.split(b'CONTRACT_CASES ')[-1])}))
