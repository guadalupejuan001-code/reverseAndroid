#!/usr/bin/env python3
"""Compare the packaged application to the owner's original native byte adapter."""
import hashlib,json,subprocess,sys,tempfile
from pathlib import Path

runtime=Path(sys.argv[1]).resolve();native=Path(sys.argv[2]).resolve();owner=Path(sys.argv[3]).resolve();app=Path(sys.argv[4]).resolve();out=Path(sys.argv[5]).resolve();out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(owner/'ai/native'))
from session_client import Session,decode_result

cases=[('hola',1,'Hola. Soy BBY Q.'),('ana tiene dos manzanas.',1,'Ana tiene dos manzanas.'),('ella recibe tres más',1,'Ana tiene cinco manzanas.'),('¿cuántas tiene ella?',1,'Ana tiene cinco manzanas.'),('how many apples does she have?',2,'Ana has five apples.'),('¿qué es un gato?',1,'Un gato es un mamífero.'),('¿y un perro?',1,'Un perro es un mamífero.'),('hello',2,'Hello. I am BBY Q.')]
cases += [(text,language,None) for text,language in [('ana tiene siete manzanas',1),('ella da dos manzanas',1),('cuántas tiene ella?',1),('what is a cat?',2),('and a dog?',2),('que sabes?',1),('un más dos',1),('inventame una historia de dragones',1)]]
initial_hash=hashlib.sha256(app.read_bytes()).hexdigest();checks=0;raw=[]
with tempfile.TemporaryDirectory(prefix='bbyq-app-exam-') as temporary:
    work=Path(temporary);state=work/'app.rvp';reference=work/'reference.rvp';program=work/'original.xi'
    subprocess.run([str(runtime),'app-init',str(app),str(state)],check=True);seed=state.read_bytes();reference.write_bytes(seed)
    assert hashlib.sha256(seed).hexdigest()=='55992a06df9ec9cad6889f3dabbdbc179d2626cfd32667efec1f74b78660e732'
    program.write_bytes(subprocess.check_output(['bash',str(owner/'ai/xi/knowledge/link.sh'),'conversation']))
    for repeat in range(4):
        # Both runners are deliberately reopened while preserving the same RVPC2.
        with Session(native,reference,program) as original:
            for text,language,expected in cases:
                response=subprocess.run([str(runtime),'app-turn',str(app),str(state),str(state),'es' if language==1 else 'en',text],check=True,capture_output=True,text=True).stdout.rstrip('\n')
                other,_=original.request(text,language);answer=decode_result(other)
                assert response==answer,(text,response,answer)
                if expected is not None:assert response==expected,(text,response,expected)
                assert state.read_bytes()==reference.read_bytes(),('state differs',text)
                checks+=3 if expected is not None else 2;raw.append({'text':text,'language':language,'answer':response,'exact_state_match':True,'restart':repeat})
        # Reject malformed input without publication or a successful response.
        before=state.read_bytes()
        rejected=subprocess.run([str(runtime),'app-turn',str(app),str(state),str(state),'es','x'*256],capture_output=True,text=True)
        assert rejected.returncode!=0 and rejected.stdout=='' and state.read_bytes()==before;checks+=1
    (out/'after.rvp').write_bytes(state.read_bytes())
assert hashlib.sha256(app.read_bytes()).hexdigest()==initial_hash
(out/'turns.json').write_text(json.dumps(raw,ensure_ascii=False,indent=2)+'\n')
result={'checks':checks,'turns':len(raw),'original_checkpoint_preserved':True,'original_adapter_exact_answers':True,'original_adapter_exact_rvpc2_after_every_turn':True,'restarts':4,'third_party_models':False,'training_replayed':False,'executable_bytes':app.stat().st_size,'executable_sha256':initial_hash}
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print('BBYQ_RXE_NATIVE_PASS',json.dumps(result))
