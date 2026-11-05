#!/usr/bin/env python3
import json,subprocess,sys,tempfile
from pathlib import Path
exe=str(Path(sys.argv[1]).resolve())
def run(*args,ok=True):
 p=subprocess.run([exe,*map(str,args)],capture_output=True,text=True)
 assert (p.returncode==0)==ok,(args,p.stdout,p.stderr)
 return p
with tempfile.TemporaryDirectory() as temporary:
 root=Path(temporary);source=root/'suma.xi';source.write_text('#!reverse-pc-machine-v1\nset r0 6\nmul r0 7\nprint r0\nhalt\n')
 image=root/'suma.rxe';info=json.loads(run('compile',source,image,'Mi programa').stdout);assert info['format']=='RXE' and info['instructions']==4
 binary=image.read_bytes();assert binary[:8]==b'RXE1\r\n\x1a\n' and b'#!reverse-pc-machine-v1' not in binary
 assert json.loads(run('inspect-rxe',image).stdout)==info
 initial=root/'initial.rvp';after=root/'after.rvp';run('init',initial);response=run('run',image,initial,after)
 assert json.loads(response.stdout)['value']==42 and initial.read_bytes()==after.read_bytes()
 recovered=root/'recuperado.xi';run('disassemble',image,recovered);again=root/'again.rvp';assert run('run',recovered,initial,again).stdout==response.stdout and again.read_bytes()==after.read_bytes()
 bad=root/'bad.rxe';bad.write_bytes(binary[:-1]);sentinel=b'preservar salida anterior';after.write_bytes(sentinel);p=run('run',bad,initial,after,ok=False);assert p.stdout=='' and after.read_bytes()==sentinel
 fake=root/'renombrado.rxe';fake.write_bytes(source.read_bytes());run('run',fake,initial,after,ok=False);assert after.read_bytes()==sentinel
 fake_upper=root/'renombrado.RXE';fake_upper.write_bytes(source.read_bytes());run('run',fake_upper,initial,after,ok=False);assert after.read_bytes()==sentinel
 old=source.read_bytes();run('compile',source,source,ok=False);assert source.read_bytes()==old
 print(json.dumps({'rxe_cli':True,'result':42,'binary_instructions':True,'readonly_state_identical':True,'disassembly_equivalent':True,'failed_output_preserved':True,'renamed_script_rejected':True}))
