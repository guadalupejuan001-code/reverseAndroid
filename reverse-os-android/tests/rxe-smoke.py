#!/usr/bin/env python3
"""Build and run REVERSE's own executable format through the desktop UI."""
import json
from pathlib import Path
import runpy
import subprocess
import sys
import time

driver=runpy.run_path(str(Path(__file__).with_name('desktop-smoke.py')),init_globals={'REVERSE_DRIVER_ONLY':True})
for key in ['adb','tree','find','box','tap','click','enter','launch','close','screenshot','wait_log','log_check','file','OUT','PACKAGE','COMPONENT','events']:
    globals()[key]=driver[key]

def downloads():
    until=time.monotonic()+25
    while time.monotonic()<until:
        try:root=tree()
        except AssertionError:
            time.sleep(.3);continue
        # The picker starts asynchronously. uiautomator can temporarily return
        # the old app tree while DocumentsUI is still opening.
        if not any('documentsui' in n.get('package','') for n in root.iter('node')):
            time.sleep(.3);continue
        if any(n.get('text','').casefold()=='downloads' for n in root.iter('node')):
            click(text='Downloads');return
        for n in root.iter('node'):
            if 'roots' in n.get('content-desc','').lower() or 'navigation drawer' in n.get('content-desc','').lower():
                tap(n);click(text='Downloads');return
        time.sleep(.3)
    raise AssertionError('Android document picker has no Downloads entry')

def public(path):
    until=time.monotonic()+20
    while time.monotonic()<until:
        try:
            adb('shell','test','-f',path);data=adb('exec-out','cat',path,binary=True)
            if data:return data
        except subprocess.SubprocessError:pass
        time.sleep(.5)
    raise AssertionError(('export missing',path))

baseline=file('session.rvp')
launch('Aplicaciones');click(desc='ABRIR RXE Aplicaciones/Hola-REVERSE.rxe')
wait_log('RXE_RUN_OK Aplicaciones/Hola-REVERSE.rxe')
assert '42' in find(desc='SALIDA RXE Aplicaciones/Hola-REVERSE.rxe').get('text','')
assert file('session.rvp')==baseline,'readonly RXE changed the shared machine state'
image=file('workspace/Aplicaciones/Hola-REVERSE.rxe')
assert image[:8]==b'RXE1\r\n\x1a\n' and b'#!reverse-pc-machine-v1' not in image
screenshot('native-rxe-running.png');close('Hola REVERSE');close('Aplicaciones')
log_check('OWN_RXE_BINARY_RUNS_INSIDE_REVERSE_AND_PRESERVES_STATE_PASS')

launch('XI Studio')
until=time.monotonic()+20
while time.monotonic()<until:
    if 'set r0 1071' in find(desc='CODIGO XI').get('text',''):break
else:raise AssertionError('source program unavailable')
before=file('session.rvp');click(text='CREAR .RXE')
enter(find(desc='NOMBRE DE ARCHIVO'),'Mi-calculo.rxe');click(text='Aceptar')
wait_log('RXE_BUILD_OK Aplicaciones/Mi-calculo.rxe')
compiled=file('workspace/Aplicaciones/Mi-calculo.rxe')
assert compiled[:8]==b'RXE1\r\n\x1a\n' and b'set r0 1071' not in compiled
assert file('session.rvp')==before,'compiler executed the source program'
close('XI Studio');launch('Aplicaciones');click(desc='ABRIR RXE Aplicaciones/Mi-calculo.rxe')
wait_log('RXE_RUN_OK Aplicaciones/Mi-calculo.rxe')
assert '21' in find(desc='SALIDA RXE Aplicaciones/Mi-calculo.rxe').get('text','')
assert b'O 42 21' in file('session.rvp')
screenshot('compiled-rxe-running.png')
log_check('XI_STUDIO_COMPILES_REAL_BYTECODE_WITHOUT_RUNNING_THEN_RXE_EXECUTES_GCD_PASS')

state=file('session.rvp');time.sleep(1)
(OUT/'before-restart-logcat.txt').write_text(adb('logcat','-d'))
adb('shell','am','force-stop',PACKAGE);adb('logcat','-c');adb('shell','am','start','-W','-n',COMPONENT)
assert '21' in find(desc='SALIDA RXE Aplicaciones/Mi-calculo.rxe',timeout=30).get('text','')
assert file('session.rvp')==state
assert 'RXE_RUN_OK' not in adb('logcat','-d','-s','ReverseDesktop:I','*:S'),'session restoration executed the RXE again'
close('Mi-calculo')
log_check('RXE_WINDOW_RESTORES_WITHOUT_REEXECUTION_OR_STATE_CHANGE_PASS')

click(desc='OPCIONES RXE Aplicaciones/Mi-calculo.rxe');click(text='Anclar al escritorio')
close('Aplicaciones');click(desc='MOSTRAR ESCRITORIO')
click(desc='ACCESO RXE Aplicaciones/Mi-calculo.rxe');find(desc='SALIDA RXE Aplicaciones/Mi-calculo.rxe');close('Mi-calculo')
click(desc='BUSCAR');enter(find(desc='BUSQUEDA GLOBAL'),'Mi-calculo');click(desc='RESULTADO RXE Aplicaciones/Mi-calculo.rxe')
find(desc='SALIDA RXE Aplicaciones/Mi-calculo.rxe');close('Mi-calculo')
log_check('OWN_EXECUTABLE_LAUNCHES_FROM_DESKTOP_SHORTCUT_AND_SEARCH_PASS')

launch('Aplicaciones');click(desc='OPCIONES RXE Aplicaciones/Mi-calculo.rxe');click(text='Exportar')
downloads();click(text='Save');wait_log('FILE_EXPORT_OK Aplicaciones/Mi-calculo.rxe')
assert public('/sdcard/Download/Mi-calculo.rxe')==compiled
before=file('session.rvp');click(text='Agregar .rxe');downloads();click(text='Mi-calculo.rxe')
find(text='Agregar ejecutable .rxe');enter(find(desc='NOMBRE DE ARCHIVO'),'Importado.rxe');click(text='Aceptar')
wait_log('RXE_IMPORT_OK Aplicaciones/Importado.rxe')
assert file('workspace/Aplicaciones/Importado.rxe')==compiled and file('session.rvp')==before
log_check('RXE_EXPORT_IMPORT_ROUNDTRIP_PRESERVES_BINARY_WITHOUT_EXECUTION_PASS')

fake=OUT/'falso.rxe';fake.write_text('#!reverse-pc-machine-v1\nset r0 99\nprint r0\nhalt\n')
adb('push',fake,'/sdcard/Download/falso.rxe')
adb('shell','am','broadcast','-a','android.intent.action.MEDIA_SCANNER_SCAN_FILE','-d','file:///sdcard/Download/falso.rxe')
click(text='Agregar .rxe');downloads();click(text='falso.rxe');wait_log('RXE_IMPORT_REJECTED')
assert file('session.rvp')==before
adb('shell','run-as',PACKAGE,'sh','-c','test ! -e files/workspace/Aplicaciones/falso.rxe')
screenshot('rxe-program-center.png')
log_check('RENAMED_TEXT_REJECTED_BY_BINARY_LOADER_WITHOUT_MUTATION_PASS')
(OUT/'logcat.txt').write_text(adb('logcat','-d'))
(OUT/'results.json').write_text(json.dumps({'package':PACKAGE,'passed':events,'format':'RXE1','own_runtime':True,'android_apk_installer_used':False},indent=2)+'\n')
print('REVERSE_RXE_UI_PASS',len(events),flush=True)
