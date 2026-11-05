#!/usr/bin/env python3
"""Real Android UI workflows through adb; no test-only app command channel."""
import json
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

PACKAGE='com.reversepc.desktop.rxe'
COMPONENT=PACKAGE+'/com.reversepc.os.BootActivity'
OUT=Path(sys.argv[1]);OUT.mkdir(parents=True,exist_ok=True)
events=[]

def adb(*args, binary=False):
    values=list(map(str,args))
    command=['adb',values[0],' '.join(shlex.quote(x) for x in values[1:])] if values[0] in ('shell','exec-out') else ['adb',*values]
    return subprocess.check_output(command,text=not binary,timeout=30)

def tree():
    for _ in range(4):
        try:
            adb('shell','uiautomator','dump','/sdcard/reverse-ui.xml')
            return ET.fromstring(adb('exec-out','cat','/sdcard/reverse-ui.xml'))
        except (ET.ParseError,subprocess.SubprocessError):
            time.sleep(.5)
    raise AssertionError('UI hierarchy unavailable')

def box(node):
    x1,y1,x2,y2=map(int,re.findall(r'\d+',node.attrib['bounds']))
    assert x2>x1 and y2>y1,node.attrib
    return x1,y1,x2,y2

def find(text=None,desc=None,clickable=False,timeout=12):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        root=tree()
        for n in root.iter('node'):
            if text is not None and n.get('text','').casefold()!=text.casefold():continue
            if desc is not None and n.get('content-desc')!=desc:continue
            if clickable and text is None and n.get('clickable')!='true':continue
            try:box(n)
            except (AssertionError,ValueError):continue
            return n
        time.sleep(.25)
    raise AssertionError(('missing UI',text,desc))

def tap(node):
    x1,y1,x2,y2=box(node);adb('shell','input','tap',(x1+x2)//2,(y1+y2)//2)

def click(text=None,desc=None,clickable=False):
    try:tap(find(text,desc,clickable,timeout=4));return
    except AssertionError:
        if text is None:
            if desc is None or not desc.startswith(('ARCHIVO ','OPCIONES ','ABRIR RXE ')):raise
            panel_name='Aplicaciones' if desc.startswith(('ABRIR RXE ','OPCIONES RXE ')) else 'Archivos'
            # Catalogs can retain their scroll position when an imported app
            # sorts above the visible rows. Search both directions and stop
            # when a real gesture leaves the visible row positions unchanged.
            for direction in ((-1,1) if panel_name=='Aplicaciones' else (1,)):
                previous=None
                for _ in range(7):
                    panel=find(desc='VENTANA '+panel_name)
                    lists=[n for n in panel.iter('node') if n.get('class')=='android.widget.ScrollView']
                    if not lists:break
                    if panel_name=='Aplicaciones':
                        positions=tuple((n.get('content-desc'),n.get('bounds')) for n in panel.iter('node') if n.get('content-desc','').startswith(('ABRIR RXE ','OPCIONES RXE ')))
                        if positions==previous:break
                        previous=positions
                    x1,y1,x2,y2=box(lists[-1]);start,end=(y1+15,y2-12) if direction<0 else (y2-12,y1+15)
                    adb('shell','input','swipe',(x1+x2)//2,start,(x1+x2)//2,end,250)
                    try:tap(find(desc=desc,timeout=2));return
                    except AssertionError:pass
            tap(find(desc=desc));return
    # Native popup menus can scroll on a short landscape display.
    for _ in range(4):
        lists=[n for n in tree().iter('node') if n.get('class')=='android.widget.ListView' and n.get('scrollable')=='true']
        if not lists:break
        x1,y1,x2,y2=box(lists[-1]);adb('shell','input','swipe',(x1+x2)//2,y2-18,(x1+x2)//2,y1+20,250)
        try:tap(find(text,desc,clickable,timeout=2));return
        except AssertionError:pass
    tap(find(text,desc,clickable))

def enter(node,value,clear=True):
    tap(node)
    if clear:
        adb('shell','input','keyevent','123')
        current=node.get('text','')
        if current:adb('shell','input','keyevent',*(['67']*(len(current)+2)))
    adb('shell','input','text',value.replace(' ','%s'))
    # Back hides the IME; the desktop should remain present.
    adb('shell','input','keyevent','4');time.sleep(.5)

def launch(name):
    click(desc='INICIO')
    for _ in range(7):
        root=tree()
        panel=next((n for n in root.iter('node') if n.get('content-desc')=='VENTANA Inicio'),root)
        nodes=[n for n in panel.iter('node') if n.get('text')==name and n.get('clickable')=='true']
        if nodes:
            try:tap(nodes[-1]);time.sleep(.5);return
            except AssertionError:pass
        panel=find(desc='VENTANA Inicio');x1,y1,x2,y2=box(panel)
        adb('shell','input','swipe',(x1+x2)//2,y2-85,(x1+x2)//2,y1+65,250)
    raise AssertionError(('cannot launch',name))

def close(name):click(desc='CERRAR '+name)

def screenshot(name):
    (OUT/name).write_bytes(adb('exec-out','screencap','-p',binary=True))

def log_check(message):
    events.append(message);print(message,flush=True)

def wait_log(token,tag='ReverseDesktop',timeout=25):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        content=adb('logcat','-d','-s',tag+':I','*:S')
        if token in content:return
        time.sleep(.5)
    raise AssertionError(('missing log',token,content[-1500:]))

def file(path):return adb('exec-out','run-as',PACKAGE,'cat','files/'+path,binary=True)

def fail_with_evidence(kind,value,traceback):
    try:
        (OUT/'failure-ui.xml').write_bytes(ET.tostring(tree()))
        screenshot('failure.png')
        (OUT/'failure-logcat.txt').write_text(adb('logcat','-d'))
    finally:sys.__excepthook__(kind,value,traceback)
sys.excepthook=fail_with_evidence

def run_smoke():
    adb('shell','am','start','-W','-n',COMPONENT)
    wait_log('WELCOME_SCREEN_SHOWN','ReverseBoot')
    time.sleep(.6);screenshot('boot.png')
    wait_log('DESKTOP_CORE_READY')
    find(desc='INICIO');screenshot('desktop.png')
    time.sleep(1.5);screenshot('motion.png')
    from PIL import Image,ImageChops
    im=Image.open(OUT/'desktop.png').convert('RGB');motion=Image.open(OUT/'motion.png').convert('RGB')
    assert im.width>im.height
    bounds=(int(im.width*.46),int(im.height*.16),int(im.width*.78),int(im.height*.74))
    assert ImageChops.difference(im.crop(bounds),motion.crop(bounds)).getbbox(),'wallpaper is not moving'
    log_check('BOOT_LANDSCAPE_AND_REAL_ORBITAL_MOTION_PASS')

    launch('Archivos');find(desc='VENTANA Archivos')
    click(desc='MINIMIZAR Archivos');launch('Calculadora')
    enter(find(desc='EXPRESIÓN CALCULADORA'),'2.5*(3+1)')
    click(text='=',clickable=True);wait_log('CALCULATOR_XI_OK 2.5*(3+1)=10')
    assert find(desc='RESULTADO CALCULADORA').get('text')=='10'
    launch('Terminal');find(desc='VENTANA Calculadora');find(desc='VENTANA Terminal')
    # Nonmodal windows can be dragged/resized and snapped independently.
    before=box(find(desc='VENTANA Terminal'))
    caption=box(find(desc='MOVER Terminal'));x=(caption[0]+caption[2])//2;y=(caption[1]+caption[3])//2
    adb('shell','input','swipe',x,y,max(80,x-90),y+12,500)
    after=box(find(desc='VENTANA Terminal'));assert after[0]!=before[0] or after[1]!=before[1]
    grip=box(find(desc='REDIMENSIONAR Terminal'));gx=(grip[0]+grip[2])//2;gy=(grip[1]+grip[3])//2
    adb('shell','input','swipe',gx,gy,gx-160,gy-65,450)
    resized=box(find(desc='VENTANA Terminal'));assert resized[2]-resized[0]<after[2]-after[0]
    click(desc='MENÚ Terminal');click(text='Ajustar izquierda',clickable=True)
    left=box(find(desc='VENTANA Terminal'));assert left[2]-left[0]<im.width*.59
    click(desc='MINIMIZAR Terminal');click(desc='TAREA Archivos');find(desc='VENTANA Archivos')
    log_check('MULTIWINDOW_MINIMIZE_RESTORE_DRAG_RESIZE_SNAP_PASS')

    # Create and save a real document through the file manager and editor.
    click(desc='ARCHIVO Documentos');click(text='+ Texto',clickable=True)
    root=tree();dialog_field=next(n for n in root.iter('node') if n.get('class')=='android.widget.EditText')
    enter(dialog_field,'Prueba escritorio.txt');click(text='Aceptar',clickable=True)
    editor=find(desc='CONTENIDO DEL DOCUMENTO');enter(editor,'persistencia real')
    click(desc='GUARDAR DOCUMENTO');time.sleep(1)
    assert file('workspace/Documentos/Prueba escritorio.txt')==b'persistencia real'
    # A changed draft survives closing its window and reopening the file.
    enter(find(desc='CONTENIDO DEL DOCUMENTO'),'borrador recuperable')
    time.sleep(.8);close('Editor');click(text='Actualizar',clickable=True)
    click(desc='ARCHIVO Prueba escritorio.txt');assert find(desc='CONTENIDO DEL DOCUMENTO').get('text')=='borrador recuperable'
    click(desc='GUARDAR DOCUMENTO');time.sleep(.6);close('Editor')
    log_check('DOCUMENT_SAVE_AND_DRAFT_RECOVERY_PASS')

    click(desc='OPCIONES Prueba escritorio.txt');click(text='Copiar',clickable=True)
    click(text='↑',clickable=True);click(desc='ARCHIVO Descargas');click(text='Pegar',clickable=True)
    find(desc='ARCHIVO Prueba escritorio.txt');assert file('workspace/Descargas/Prueba escritorio.txt')==b'borrador recuperable'
    click(desc='OPCIONES Prueba escritorio.txt');click(text='Renombrar',clickable=True)
    root=tree();edit=next(n for n in root.iter('node') if n.get('class')=='android.widget.EditText')
    enter(edit,'Copia.txt');click(text='Aceptar',clickable=True);find(desc='ARCHIVO Copia.txt')
    click(desc='OPCIONES Copia.txt');click(text='A la papelera',clickable=True);click(text='Mover',clickable=True)
    time.sleep(.8);click(text='↑',clickable=True);click(desc='ARCHIVO Papelera')
    click(desc='OPCIONES Copia.txt');click(text='Restaurar',clickable=True);time.sleep(.7)
    assert file('workspace/Descargas/Copia.txt')==b'borrador recuperable'
    log_check('FILE_COPY_RENAME_TRASH_RESTORE_PASS')
    close('Archivos');launch('XI Studio')
    until=time.monotonic()+12
    while time.monotonic()<until:
        if any('set r0 1071' in n.get('text','') for n in tree().iter('node')):break
        time.sleep(.3)
    else:raise AssertionError('XI editor did not load Euclides')
    click(text='▶ EJECUTAR',clickable=True)
    wait_log('RUN_OK 01_euclides.xi','ReverseOS');time.sleep(.8)
    assert b'O 42 21' in file('session.rvp')
    screenshot('workspace.png');log_check('ORIGINAL_XI_EUCLIDES_AND_RVPC2_PASS')

    # Keep a dirty document open, then kill the Android process and restore it.
    close('XI Studio');launch('Archivos');click(text='↑',clickable=True)
    # The remembered file manager folder can be root or one level below it.
    root=tree()
    if not any(n.get('content-desc')=='ARCHIVO Documentos' for n in root.iter('node')):
        click(text='↑',clickable=True)
    click(desc='ARCHIVO Documentos');click(desc='ARCHIVO Prueba escritorio.txt')
    enter(find(desc='CONTENIDO DEL DOCUMENTO'),'sobrevive al reinicio');time.sleep(1)
    adb('shell','am','force-stop',PACKAGE)
    adb('shell','am','start','-W','-n',COMPONENT)
    time.sleep(4)
    assert find(desc='CONTENIDO DEL DOCUMENTO',timeout=25).get('text')=='sobrevive al reinicio'
    assert b'O 42 21' in file('session.rvp')
    log_check('PROCESS_RESTART_WINDOW_SESSION_DRAFT_AND_ETHER_PERSISTENCE_PASS')
    screenshot('restored-workspace.png')

    # Test viewport shrink similar to a phone, including IME open/close recovery.
    close('Editor');launch('Ajustes')
    for _ in range(5):
        root=tree()
        if any(n.get('text')=='EXPORTAR RESPALDO DEL ESCRITORIO' and n.get('clickable')=='true' and int(re.findall(r'\d+',n.get('bounds','[0,0][0,0]'))[3])>0 for n in root.iter('node')):break
        x1,y1,x2,y2=box(find(desc='VENTANA Ajustes'));adb('shell','input','swipe',x2-60,y2-50,x2-60,y1+100,250)
    find(text='EXPORTAR RESPALDO DEL ESCRITORIO',clickable=True)
    close('Ajustes');launch('Ventanas');find(desc='VENTANA Ventanas');screenshot('windows.png')
    pss=adb('shell','dumpsys','meminfo',PACKAGE)
    (OUT/'meminfo.txt').write_text(pss)
    (OUT/'ui.xml').write_bytes(ET.tostring(tree()))
    logs=adb('logcat','-d');(OUT/'logcat.txt').write_text(logs)
    assert 'FATAL EXCEPTION' not in logs,'Android process crashed'
    (OUT/'results.json').write_text(json.dumps({'package':PACKAGE,'passed':events,'real_android_ui':True,'desktop_inside_android':True},indent=2)+'\n')
    print('REVERSE_DESKTOP_UI_PASS',len(events),flush=True)

if not globals().get('REVERSE_DRIVER_ONLY',False):
    run_smoke()
