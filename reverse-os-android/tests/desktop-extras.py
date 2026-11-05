#!/usr/bin/env python3
"""Run the core UI journey, then real browser, host apps and SAF transfers."""
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys
import time
import zipfile
from PIL import Image

state=runpy.run_path(str(Path(__file__).with_name('desktop-smoke.py')))
for key in ['adb','tree','find','box','tap','click','enter','launch','close','screenshot','log_check','wait_log','file','OUT','PACKAGE','events']:
    globals()[key]=state[key]


def scroll_to(text,window):
    for _ in range(8):
        try:return find(text=text,timeout=2)
        except AssertionError:
            x1,y1,x2,y2=box(find(desc='VENTANA '+window))
            adb('shell','input','swipe',x2-70,y2-45,x2-70,y1+95,300)
    return find(text=text)


def downloads():
    root=tree()
    if any(n.get('text','').casefold()=='downloads' for n in root.iter('node')):
        click(text='Downloads');return
    for n in root.iter('node'):
        desc=n.get('content-desc','').lower()
        if 'roots' in desc or 'navigation drawer' in desc:
            tap(n);click(text='Downloads');return
    # Recent documents may already contain the requested file.


def wait_public(path,timeout=15):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        try:
            adb('shell','test','-f',path)
            data=adb('exec-out','cat',path,binary=True)
            if data:return data
        except subprocess.SubprocessError:pass
        time.sleep(.5)
    raise AssertionError(('public file not written',path))

close('Ventanas')
launch('Navegador');find(text='Tu ventana a la web.',timeout=25)
screenshot('browser.png');close('Navegador')
log_check('REAL_WEBVIEW_OFFLINE_HOME_PASS')
launch('Aplicaciones');click(text='Android');tap(scroll_to('Settings','Android'))
assert 'com.android.settings' in adb('shell','dumpsys','activity','activities')
adb('shell','input','keyevent','4');find(desc='VENTANA Android');close('Android');close('Aplicaciones')
log_check('INSTALLED_ANDROID_APP_LAUNCH_AND_RETURN_PASS')
# The optional Home entry must reuse the current desktop and authority.
before=adb('logcat','-d','-s','ReverseDesktop:I','*:S').count('DESKTOP_CORE_READY')
adb('shell','am','start','-W','-n',PACKAGE+'/com.reversepc.os.HomeActivity')
find(desc='INICIO')
after=adb('logcat','-d','-s','ReverseDesktop:I','*:S').count('DESKTOP_CORE_READY')
assert before==after,'Home started another desktop/core'
log_check('HOME_ENTRY_REUSES_DESKTOP_AUTHORITY_PASS')

fixture=OUT/'reverse-fixture.png'
Image.new('RGB',(80,60),(30,100,200)).save(fixture)
adb('push',fixture,'/sdcard/Download/reverse-fixture.png')
adb('shell','am','broadcast','-a','android.intent.action.MEDIA_SCANNER_SCAN_FILE','-d','file:///sdcard/Download/reverse-fixture.png')
launch('Archivos')
for _ in range(3):
    if any(n.get('content-desc')=='ARCHIVO Descargas' for n in tree().iter('node')):break
    click(text='↑')
click(desc='ARCHIVO Descargas');click(text='Importar')
downloads();click(text='reverse-fixture.png')
wait_log('FILE_IMPORT_OK Descargas/reverse-fixture.png')
assert file('workspace/Descargas/reverse-fixture.png')==fixture.read_bytes()
click(desc='ARCHIVO reverse-fixture.png')
image=find(desc='IMAGEN ABIERTA');x1,y1,x2,y2=box(image)
screenshot('image-viewer.png')
pixel=Image.open(OUT/'image-viewer.png').convert('RGB').getpixel(((x1+x2)//2,(y1+y2)//2))
assert max(abs(a-b) for a,b in zip(pixel,(30,100,200)))<12,pixel
close('Imagen')
log_check('SAF_IMPORT_AND_REAL_IMAGE_DECODE_PASS')

click(text='↑');click(desc='ARCHIVO Documentos');click(desc='ARCHIVO Prueba escritorio.txt')
enter(find(desc='CONTENIDO DEL DOCUMENTO'),'exportar texto actual')
click(text='EXPORTAR')
downloads();click(text='Save')
wait_log('FILE_EXPORT_OK Documentos/Prueba escritorio.txt')
exported=wait_public('/sdcard/Download/Prueba escritorio.txt')
assert exported==b'exportar texto actual',exported
assert file('workspace/Documentos/Prueba escritorio.txt')==b'borrador recuperable','export changed the saved document'
close('Editor')
log_check('SAF_EXPORT_CURRENT_DRAFT_WITHOUT_OVERWRITING_SAVED_FILE_PASS')

launch('Ajustes');tap(scroll_to('EXPORTAR RESPALDO DEL ESCRITORIO','Ajustes'))
downloads();click(text='Save')
wait_log('DESKTOP_BACKUP_OK')
# ZIP footer is written when the provider stream closes.
until=time.monotonic()+25
while time.monotonic()<until:
    try:
        data=wait_public('/sdcard/Download/REVERSE-Desktop-respaldo.zip',timeout=2)
        target=OUT/'backup.zip';target.write_bytes(data)
        with zipfile.ZipFile(target) as z:
            assert z.testzip() is None
            assert z.read('session.rvp')==file('session.rvp')
            assert z.read('archivos/Documentos/Prueba escritorio.txt')==b'borrador recuperable'
            assert z.read('archivos/Descargas/reverse-fixture.png')==fixture.read_bytes()
            prefs=json.loads(z.read('escritorio.json'))
            assert prefs['draft:Documentos/Prueba escritorio.txt']=='exportar texto actual'
        break
    except (zipfile.BadZipFile,subprocess.SubprocessError):time.sleep(.5)
else:raise AssertionError('backup did not complete')
close('Ajustes')
log_check('SAF_FULL_BACKUP_CORE_FILES_AND_DRAFTS_PASS')
launch('Archivos');click(desc='MENÚ Archivos');click(text='Ajustar izquierda')
launch('Calculadora');click(desc='MENÚ Calculadora');click(text='Ajustar derecha')
find(text='Pausa')
screenshot('final-desktop.png')
log_check('TWO_WINDOW_WORKSPACE_PAUSES_COVERED_WALLPAPER_PASS')
(OUT/'results.json').write_text(json.dumps({'package':PACKAGE,'passed':events,'real_android_ui':True,'desktop_inside_android':True,'saf_transfers_and_backup':True},indent=2)+'\n')
(OUT/'logcat.txt').write_text(adb('logcat','-d'))
print('REVERSE_DESKTOP_EXTENDED_UI_PASS',len(events),flush=True)
