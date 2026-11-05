#!/usr/bin/env python3
"""Operate the real packaged BBY Q app; no test-only inference command channel."""
import hashlib,json,runpy,struct,subprocess,time
from pathlib import Path

driver=runpy.run_path(str(Path(__file__).with_name('desktop-smoke.py')),init_globals={'REVERSE_DRIVER_ONLY':True})
for key in ['adb','tree','find','tap','click','enter','launch','close','screenshot','wait_log','log_check','file','OUT','PACKAGE','COMPONENT','events']:
    globals()[key]=driver[key]

def downloads():
    root=tree()
    if any(n.get('text','').casefold()=='downloads' for n in root.iter('node')):click(text='Downloads');return
    for n in root.iter('node'):
        if 'roots' in n.get('content-desc','').lower() or 'navigation drawer' in n.get('content-desc','').lower():tap(n);click(text='Downloads');return
    raise AssertionError('Downloads root unavailable')

def saved():
    raw=file(record);assert raw[:5]==b'RXOS1'
    text_len,state_len=struct.unpack_from('<II',raw,5);assert 13+text_len+state_len==len(raw)
    return raw,json.loads(raw[13:13+text_len]),raw[13+text_len:]

def send(text,expected):
    before=adb('logcat','-d','-s','ReverseDesktop:I','*:S').count('BBYQ_APP_TURN_OK')
    enter(find(desc='ENTRADA BBY Q'),text);click(desc='ENVIAR BBY Q')
    until=time.monotonic()+30
    while time.monotonic()<until:
        logs=adb('logcat','-d','-s','ReverseDesktop:I','*:S')
        if logs.count('BBYQ_APP_TURN_OK')>before:
            node=find(desc='RESPUESTA BBY Q')
            if node.get('text')=='BBY Q · '+expected:return
        time.sleep(.3)
    raise AssertionError(('BBY Q response',text,expected,find(desc='ESTADO BBY Q').get('text')))

baseline=file('session.rvp');image=file('workspace/Aplicaciones/BBY-Q.rxe')
assert image[:8]==b'RXE2\r\n\x1a\n' and b'RVPC2' in image and b'#!reverse-pc-machine-v1' not in image
identity=hashlib.sha256(image).hexdigest();record='rxe-apps/'+identity+'/session.rxas'
launch('BBY Q');wait_log('BBYQ_APP_READY');find(desc='ENTRADA BBY Q')
adb('shell','run-as',PACKAGE,'sh','-c','test ! -e files/'+record)
send('hola','Hola. Soy BBY Q.');raw,metadata,state=saved()
assert len(metadata['history'])==2 and metadata['executable_sha256']==identity and state[:5]==b'RVPC2'
assert file('session.rvp')==baseline
screenshot('bbyq-rxe-conversation.png')
log_check('REAL_BBYQ_RXE_CONTAINS_CODE_AND_TRAINED_MEMORY_AND_RESPONDS_PASS')

send('ana tiene dos manzanas','Ana tiene dos manzanas.')
send('ella recibe tres mas','Ana tiene cinco manzanas.')
assert len(saved()[1]['history'])==6 and file('session.rvp')==baseline
log_check('PACKAGED_XI_COMPOSES_SENTENCES_AND_UPDATES_ITS_OWN_ACCOUNT_PASS')

before=saved()[0];time.sleep(1);(OUT/'before-restart-logcat.txt').write_text(adb('logcat','-d'))
adb('shell','am','force-stop',PACKAGE);adb('logcat','-c');adb('shell','am','start','-W','-n',COMPONENT)
wait_log('BBYQ_APP_READY',timeout=35)
assert find(desc='RESPUESTA BBY Q',timeout=35).get('text')=='BBY Q · Ana tiene cinco manzanas.'
assert saved()[0]==before and 'BBYQ_APP_TURN_OK' not in adb('logcat','-d','-s','ReverseDesktop:I','*:S')
send('cuantas tiene ella?','Ana tiene cinco manzanas.')
log_check('PROCESS_RESTART_RESTORES_REAL_BRAIN_CONTEXT_AND_TRANSCRIPT_WITHOUT_AUTORUN_PASS')

click(desc='IDIOMA BBY Q EN');send('how many apples does she have?','Ana has five apples.')
assert saved()[1]['language']==2
click(desc='IDIOMA BBY Q ES');send('que es un gato?','Un gato es un mamífero.')
send('y un perro?','Un perro es un mamífero.')
screenshot('bbyq-rxe-factual-dialogue.png')
log_check('ONE_BRAIN_ANSWERS_ES_EN_AND_RETAINS_FACTUAL_FOLLOWUP_PASS')

exported=saved()[2];click(desc='EXPORTAR MEMORIA BBY Q');downloads();click(text='Save');wait_log('BBYQ_APP_MEMORY_EXPORTED')
until=time.monotonic()+20
while time.monotonic()<until:
    try:
        if adb('exec-out','cat','/sdcard/Download/BBY-Q-memoria.rvp',binary=True)==exported:break
    except subprocess.SubprocessError:pass
    time.sleep(.5)
else:raise AssertionError('Exported model differs')
close('BBY Q');copy=OUT/'BBY-Q.rxe';copy.write_bytes(image);adb('push',copy,'/sdcard/Download/BBY-Q.rxe')
adb('shell','am','broadcast','-a','android.intent.action.MEDIA_SCANNER_SCAN_FILE','-d','file:///sdcard/Download/BBY-Q.rxe')
launch('Aplicaciones');click(text='Agregar .rxe');downloads();click(text='BBY-Q.rxe');find(text='Agregar ejecutable .rxe');enter(find(desc='NOMBRE DE ARCHIVO'),'BBY-Q-copia.rxe');click(text='Aceptar');wait_log('RXE_IMPORT_OK Aplicaciones/BBY-Q-copia.rxe')
before=saved()[0];click(desc='ABRIR RXE Aplicaciones/BBY-Q-copia.rxe');find(desc='ENTRADA BBY Q')
assert find(desc='RESPUESTA BBY Q').get('text')=='BBY Q · Un perro es un mamífero.' and saved()[0]==before
assert file('workspace/Aplicaciones/BBY-Q-copia.rxe')==image
log_check('MEMORY_EXPORT_AND_BINARY_IMPORT_USE_THE_REAL_RETAINED_AGENT_PASS')

before=saved()[0];old_response=find(desc='RESPUESTA BBY Q').get('text');folder='files/rxe-apps/'+identity
adb('shell','run-as',PACKAGE,'chmod','500',folder)
try:
    enter(find(desc='ENTRADA BBY Q'),'ana tiene nueve manzanas');click(desc='ENVIAR BBY Q');wait_log('BBYQ_APP_TURN_FAILED')
    until=time.monotonic()+20
    while time.monotonic()<until:
        if 'Memoria anterior conservada' in find(desc='ESTADO BBY Q').get('text',''):break
    else:raise AssertionError('save failure not reported')
    assert saved()[0]==before and find(desc='RESPUESTA BBY Q').get('text')==old_response
finally:adb('shell','run-as',PACKAGE,'chmod','700',folder)
send('cuantas manzanas tiene ana?','Ana tiene cinco manzanas.')
assert file('session.rvp')==baseline and file('workspace/Aplicaciones/BBY-Q.rxe')==image
screenshot('bbyq-rxe-memory-retained.png')
log_check('REAL_DISK_FAILURE_DOES_NOT_PUBLISH_ANSWER_OR_LOSE_PREVIOUS_KNOWLEDGE_PASS')

(OUT/'logcat.txt').write_text(adb('logcat','-d'))
(OUT/'results.json').write_text(json.dumps({'package':PACKAGE,'passed':events,'executable_sha256':identity,'executable_bytes':len(image),'stage':'E016','desktop_state_identical':True,'immutable_executable':True,'third_party_models':False},indent=2)+'\n')
print('BBYQ_RXE_ANDROID_PASS',len(events),flush=True)
