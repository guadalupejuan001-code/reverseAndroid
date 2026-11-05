#!/usr/bin/env python3
"""Package the owner's unchanged linked XI and released RVPC2. No AI here."""
import argparse
import base64
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

SOURCE='af1f9bff0e95cf78f0e03b5c289f7bd4dcdb9aa3'
SEED_SHA='55992a06df9ec9cad6889f3dabbdbc179d2626cfd32667efec1f74b78660e732'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('runtime');ap.add_argument('bbyq');ap.add_argument('output');args=ap.parse_args()
    runtime=Path(args.runtime).resolve();owner=Path(args.bbyq).resolve();output=Path(args.output).resolve()
    revision=subprocess.check_output(['git','-C',str(owner),'rev-parse','HEAD'],text=True).strip()
    if revision!=SOURCE:raise ValueError('BBY Q source differs from the reviewed E016 revision')
    subprocess.run(['git','-C',str(owner),'diff','--quiet','HEAD','--','ai/xi','ai/checkpoints'],check=True)
    manifest=json.loads((owner/'ai/checkpoints/e016-manifest.json').read_text())
    if manifest['decoded_sha256']!=SEED_SHA or manifest['decoded_bytes']!=209659:raise ValueError('Unreviewed E016 checkpoint')
    packed=base64.b64decode((owner/'ai/checkpoints'/manifest['file']).read_bytes(),validate=False)
    with tempfile.TemporaryDirectory(prefix='reverse-bbyq-app-') as temporary:
        work=Path(temporary);archive=work/'seed.gz';archive.write_bytes(packed)
        with gzip.open(archive,'rb') as source:seed=source.read(209660)
        if len(seed)!=209659 or hashlib.sha256(seed).hexdigest()!=SEED_SHA:raise ValueError('Checkpoint integrity failed')
        (work/'seed.rvp').write_bytes(seed)
        linked=subprocess.check_output(['bash',str(owner/'ai/xi/knowledge/link.sh'),'conversation'])
        (work/'conversation.xi').write_bytes(linked)
        subprocess.run([str(runtime),'compile',str(work/'conversation.xi'),str(work/'code.rxe'),'BBY Q E016'],check=True,capture_output=True)
        output.parent.mkdir(parents=True,exist_ok=True)
        result=subprocess.run([str(runtime),'pack-app',str(work/'code.rxe'),str(work/'seed.rvp'),str(output),'org.reverse.bbyq','BBY Q','E016',SOURCE],check=True,capture_output=True,text=True)
        info=json.loads(result.stdout);info.update({'checkpoint_sha256':SEED_SHA,'executable_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'linked_source_sha256':hashlib.sha256(linked).hexdigest(),'old_training_replayed':False})
        print(json.dumps(info,ensure_ascii=False))

if __name__=='__main__':main()
