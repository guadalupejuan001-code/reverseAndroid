#!/usr/bin/env python3
import json,subprocess,sys,tempfile
from pathlib import Path
exe=str(Path(sys.argv[1]).resolve())
def run(*args,ok=True):
    p=subprocess.run([exe,*map(str,args)],capture_output=True,text=True)
    assert (p.returncode==0)==ok,(args,p.stdout,p.stderr)
    return p
with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary);source=root/'echo.xi';source.write_text('#!reverse-pc-machine-v1\nload8 r14 7400000000000000128\nprint r14\nstore8 42 21\nhalt\n')
    code=root/'code.rxe';seed=root/'seed.rvp';image=root/'app.rxe';run('compile',source,code);run('init',seed)
    info=json.loads(run('pack-app',code,seed,image,'org.reverse.test','Prueba','1.0','a'*40).stdout);assert info['version']==2 and info['ui']=='chat'
    assert json.loads(run('inspect-rxe',image).stdout)==info
    state=root/'state.rvp';run('app-init',image,state);assert state.read_bytes()==seed.read_bytes()
    assert run('app-turn',image,state,state,'es','Hola').stdout.strip()=='H'
    previous=state.read_bytes();run('app-init',image,state,ok=False);assert state.read_bytes()==previous
    image_before=image.read_bytes();run('app-turn',image,state,image,'es','Hola',ok=False);assert image.read_bytes()==image_before
    bad=root/'bad.rxe';bad.write_bytes(image_before[:-1]);destination=root/'out.rvp';destination.write_bytes(b'previous state')
    p=run('app-turn',bad,state,destination,'es','Hola',ok=False);assert p.stdout=='' and destination.read_bytes()==b'previous state'
    run('run',image,seed,destination,ok=False);assert destination.read_bytes()==b'previous state'
    run('app-turn',image,state,destination,'other','Hola',ok=False);assert destination.read_bytes()==b'previous state'
    run('pack-app',code,seed,code,'org.reverse.test','Prueba','1.0','a'*40,ok=False);assert code.read_bytes()[:8]==b'RXE1\r\n\x1a\n'
    print(json.dumps({'rxe_app_cli':True,'binary_capsule':True,'real_mailbox_echo':'H','seed_preserved':True,'existing_memory_protected':True,'failed_output_preserved':True}))
