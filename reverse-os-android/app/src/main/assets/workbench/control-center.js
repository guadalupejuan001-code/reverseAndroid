'use strict';
/* Lecturas locales con procedencia. Sin métricas sintéticas ni sondeos remotos. */
const DEFAULT_WALL = 'assets/pliegue-default.webp';
const UI_BUILD = '2.4.1';
let systemSnapshot = null;
let savedAt = null;
let saveError = null;

function wallpaperURL(){return state.prefs.wall==='image'&&validImage(state.prefs.image)?state.prefs.image:DEFAULT_WALL;}
function applyPrefs(){
  const p=state.prefs, root=document.documentElement, preset=PRESETS.find(x=>x.id===p.preset)||PRESETS[0];
  document.body.dataset.wallMotion=String(p.motion);document.body.dataset.theme=p.theme;document.body.dataset.wall=p.wall;document.body.dataset.grid=String(p.grid);document.body.dataset.dock='full';
  for(const key of ['--desktop','--surface','--surface-2','--surface-3','--ink','--muted','--line','--window','--dock-bg','--wall-color'])root.style.removeProperty(key);
  if(p.theme==='dark'){
    const colors=preset.id==='noche'?['#111219','#1b1d28','#242735','#303444']:preset.id==='arena'?['#171513','#25221f','#302c27','#3b352f']:['#0d0f11','#1a1d20','#24282d','#30353b'];
    root.style.setProperty('--desktop',colors[0]);root.style.setProperty('--window',colors[1]);root.style.setProperty('--surface',colors[1]);root.style.setProperty('--surface-2',colors[2]);root.style.setProperty('--surface-3',colors[3]);root.style.setProperty('--dock-bg',colors[0]);
  }
  root.style.setProperty('--accent',p.accent);root.style.setProperty('--accent-ink',p.theme==='light'?'#233023':'#202428');root.style.setProperty('--radius',p.radius+'px');root.style.setProperty('--wall-dim',p.dim/100);
  const layer=$('#wall-image');layer.style.backgroundImage=['pliegue','image'].includes(p.wall)?'url("'+wallpaperURL()+'")':'';
  layer.style.backgroundSize=p.fit;layer.style.backgroundPosition=p.wallX+'% '+p.wallY+'%';layer.style.backgroundRepeat='no-repeat';layer.style.transform='scale('+(p.scale/100)+')';layer.style.setProperty('--wall-motion-scale',String(p.scale/100*1.07));layer.style.setProperty('--wall-motion-scale-end',String(p.scale/100*1.105));layer.style.filter='blur('+p.blur+'px) saturate('+p.saturation+'%)';
  $('#wallpaper').style.backgroundImage='';if($('#desktop-name'))$('#desktop-name').textContent=p.title||'REVERSE';$('#clock-widget').hidden=!p.clock;$('#shortcuts').hidden=!p.icons;
  $$('[data-wallpaper-preview]').forEach(el=>{el.style.backgroundImage='url("'+wallpaperURL()+'")';el.style.backgroundSize=p.fit;el.style.backgroundPosition=p.wallX+'% '+p.wallY+'%';el.style.filter='brightness('+(1-p.dim/100)+') blur('+Math.min(p.blur,5)+'px) saturate('+p.saturation+'%)';});
  document.querySelector('meta[name="theme-color"]').content=getComputedStyle(root).getPropertyValue('--desktop').trim();
}
function persistWeb(){
  try{const raw=JSON.stringify(state);if(raw.length>3600000)throw Error('Espacio de guardado lleno');localStorage.setItem(STORE_KEY,raw);if(localStorage.getItem(STORE_KEY)!==raw)throw Error('El navegador no confirmó el guardado');savedAt=now();saveError=null;}
  catch(error){saveError=error.message;if(now()-lastSaveError>6000){toast('No se pudo guardar. Exporta un respaldo desde Ajustes.','help');lastSaveError=now();}}
  $$('.save-label').forEach(el=>el.textContent=saveError?'Sin guardar: exporta una copia':'Guardado en este navegador');
}
function renderShortcuts(){
  const count=state.files.filter(f=>f.trashRoot===f.id).length;
  $('#shortcuts').innerHTML='';
}
function taskApp(w){return w.app==='rxe'&&w.rxePath==='Aplicaciones/BBY-Q.rxe'?'bbyq':w.app;}
function taskButton(app){
  const list=state.windows.filter(w=>taskApp(w)===app&&w.space===state.space);const focused=list.some(w=>w.id===activeId&&!w.min);
  return '<button class="dock-button task-app '+(list.length?'running ':'')+(focused?'focused':'')+'" data-task-app="'+app+'" aria-label="'+(list.length?'Ventanas de ':'Abrir ')+APPS[app].name+'" title="'+(list.length?list.length+' ventana'+(list.length===1?'':'s'):'Abrir aplicación')+'">'+icon(APPS[app].icon)+'<span class="task-app-label">'+APPS[app].name+'</span>'+(list.length>1?'<span class="task-count">'+list.length+'</span>':'')+'</button>';
}
function renderDock(){
  $('#pinned-apps').innerHTML=PINNED.map(taskButton).join('');
  const extra=[...new Set(state.windows.filter(w=>w.space===state.space&&w.app!=='wallpaper'&&!PINNED.includes(taskApp(w))).map(taskApp))];$('#running-apps').innerHTML=extra.map(taskButton).join('');
}
function selectTopWindow(){const top=state.windows.filter(w=>w.space===state.space&&!w.min).sort((a,b)=>b.z-a.z)[0];activeId=top?.id||null;$$('.window').forEach(el=>el.classList.toggle('focused',el.dataset.id===activeId));}
function updateClock(){
  const date=new Date();const time=new Intl.DateTimeFormat('es',{hour:'2-digit',minute:'2-digit',hour12:false}).format(date);const day=new Intl.DateTimeFormat('es',{weekday:'long',day:'numeric',month:'long'}).format(date);
  $('#desktop-time').textContent=time;$('#desktop-day').textContent=day;$('#top-date').textContent=new Intl.DateTimeFormat('es',{weekday:'short',day:'numeric',month:'short'}).format(date);$('#task-time').textContent=time;$('#task-time').title=day;
}
function refreshSettings(){
  for(const w of state.windows.filter(w=>w.app==='settings'||w.app==='wallpaper'||(w.app==='control'&&w.tab==='appearance'))){const body=$('.window-content',$('#'+w.id));if(!body)continue;const top=body.scrollTop;const pane=$('.control-main',body);const paneTop=pane?.scrollTop||0;renderWindowContent(w);body.scrollTop=top;const next=$('.control-main',body);if(next)next.scrollTop=paneTop;}
}
function wallpaperSlider(key,label,min,max,suffix){const p=state.prefs;return '<label class="wall-control"><span>'+label+'</span><input type="range" data-pref="'+key+'" min="'+min+'" max="'+max+'" value="'+p[key]+'" aria-label="'+label+'"><output data-pref-output="'+key+'">'+p[key]+suffix+'</output></label>';}
function wallpaperHTML(){
  const p=state.prefs;const url=wallpaperURL();
  return '<div class="wallpaper-editor"><div class="wall-preview" data-wallpaper-preview style="background-image:url(&quot;'+esc(url)+'&quot;);background-size:'+p.fit+';background-position:'+p.wallX+'% '+p.wallY+'%;filter:brightness('+(1-p.dim/100)+') saturate('+p.saturation+'%)"><span class="preview-tag">'+(p.wall==='pliegue'?'PREDETERMINADO':p.wall==='image'?'TU IMAGEN':'FONDO DE COLOR')+'</span></div><div class="wall-editor-heading"><h2>'+(p.wall==='image'?'Mi imagen':p.wall==='pliegue'?'Pliegue':'Fondo de color')+'</h2><p>Los cambios se ven directamente en el escritorio.</p></div><div class="wall-mode-row">'+[['pliegue','Pliegue'],['gradient','Atmósfera'],['plain','Liso'],...(validImage(p.image)?[['image','Mi imagen']]:[])].map(([id,label])=>'<button class="wallpaper-choice '+(p.wall===id?'selected':'')+'" data-wall="'+id+'">'+label+'</button>').join('')+'</div>'+wallpaperSlider('dim','Oscurecer',0,65,'%')+wallpaperSlider('wallX','Posición horizontal',0,100,'%')+wallpaperSlider('wallY','Posición vertical',0,100,'%')+wallpaperSlider('scale','Ampliación',100,180,'%')+wallpaperSlider('blur','Desenfoque',0,16,' px')+wallpaperSlider('saturation','Saturación',0,150,'%')+settingRow('Movimiento del fondo','Animación suave.',toggleHTML('motion','Movimiento del fondo'))+'<label class="wall-fit"><span>Ajuste</span><select data-pref="fit" aria-label="Ajuste del fondo"><option value="cover" '+(p.fit==='cover'?'selected':'')+'>Cubrir pantalla</option><option value="contain" '+(p.fit==='contain'?'selected':'')+'>Mostrar completa</option></select></label><div class="wall-editor-actions"><button class="secondary-button" data-action="upload-wallpaper">'+icon('image')+'Elegir imagen</button><button class="secondary-button" data-ui="restore-wallpaper">'+icon('restore')+'Restaurar fondo</button></div><p class="wall-editor-note">PNG, JPG o WebP hasta 8 MB. Las imágenes propias se adaptan para guardarse en la aplicación. El fondo original se conserva.</p></div>';
}
function diagnosticRow(label,value,source,status='read',note=''){return {label,value,source,status,note};}
function collectWebSystemInfo(probeGraphics=true){
  const rows=[diagnosticRow('Entorno','Navegador web','Entorno de ejecución de esta aplicación'),diagnosticRow('Procesador físico','Modelo no expuesto','Sin API de modelo físico en este escritorio web','unavailable'),diagnosticRow('Hilos lógicos disponibles',Number.isInteger(navigator.hardwareConcurrency)&&navigator.hardwareConcurrency>0?String(navigator.hardwareConcurrency):'No expuestos','navigator.hardwareConcurrency',navigator.hardwareConcurrency?'reported':'unavailable','Es lo que el navegador comunica. Puede ser inferior al total del equipo; no mide carga ni rendimiento.'),diagnosticRow('Carga total de CPU','No disponible','Sin acceso a los contadores del sistema operativo','unavailable'),diagnosticRow('RAM física / consumo del sistema','No disponible','Sin acceso a los contadores de memoria del anfitrión','unavailable')];
  let renderer='No disponible',rendererSource='WebGL no disponible o lectura restringida',rendererStatus='unavailable';
  if(probeGraphics)try{const canvas=document.createElement('canvas');const gl=canvas.getContext('webgl',{antialias:false,depth:false,stencil:false});if(gl){try{const ext=gl.getExtension('WEBGL_debug_renderer_info');renderer=String(ext?gl.getParameter(ext.UNMASKED_RENDERER_WEBGL):gl.getParameter(gl.RENDERER));rendererSource=ext?'WebGL · UNMASKED_RENDERER_WEBGL':'WebGL · RENDERER (cadena genérica)';rendererStatus='reported';}finally{gl.getExtension('WEBGL_lose_context')?.loseContext();}}}catch{}
  if(probeGraphics)rows.push(diagnosticRow('Renderizador WebGL',renderer,rendererSource,rendererStatus,'Cadena del controlador expuesta por el navegador; no es una medición de FPS ni de potencia.'));
  rows.push(diagnosticRow('XI / Ether','Sin conexión','Esta edición web no tiene un puente al runtime REVERSE-PC','disconnected'));
  let raw=null;try{raw=localStorage.getItem(STORE_KEY);}catch{}
  rows.push(diagnosticRow('Datos guardados del escritorio',raw===null?'No se pudo leer':new TextEncoder().encode(raw).length.toLocaleString('es')+' bytes','localStorage · JSON leído y contado en UTF-8',raw===null?'unavailable':'read','Tamaño del archivo lógico guardado; no equivale a RAM física ni al espacio libre del dispositivo.'));
  rows.push(diagnosticRow('Ventanas de la sesión',String(state.windows.length)+' ('+state.windows.filter(w=>w.min).length+' minimizadas)','Gestor de ventanas de este escritorio'));
  rows.push(diagnosticRow('Archivos / papelera',state.files.filter(f=>f.type!=='folder'&&!f.trashRoot).length+' archivos · '+state.files.filter(f=>f.trashRoot===f.id).length+' elementos en papelera','Índice de archivos de este escritorio'));
  rows.push(diagnosticRow('Interfaz','REVERSE Workbench '+UI_BUILD,'Versión declarada en el código'));
  return {schema:'reverse.workbench.diagnostics.v1',capturedAt:new Date().toISOString(),scope:'Datos de este navegador y esta sesión. No son lecturas de XI/Ether ni del teléfono de otro usuario.',saveError,rows};
}
function webSystemHTML(){
  systemSnapshot ||= collectSystemInfo();const d=systemSnapshot;const when=new Intl.DateTimeFormat('es',{hour:'2-digit',minute:'2-digit',second:'2-digit'}).format(new Date(d.capturedAt));
  return '<div class="control-heading"><div><span class="eyebrow">PROCEDENCIA VISIBLE</span><h2>Sistema</h2><p>Lecturas del dispositivo donde abriste este escritorio.</p></div><button class="secondary-button" data-ui="refresh-system">'+icon('restore')+'Actualizar</button></div><div class="system-boundary">'+icon('help')+'<span>Interfaz web autónoma. XI/Ether figura sin conexión hasta que exista un puente real al núcleo.</span></div><div class="diagnostic-table">'+d.rows.map(row=>'<div class="diagnostic-row"><div><span class="diagnostic-label">'+esc(row.label)+'</span><small>'+esc(row.source)+'</small></div><div><strong class="'+(['unavailable','disconnected'].includes(row.status)?'unavailable':'')+'">'+esc(row.value)+'</strong>'+(row.note?'<p>'+esc(row.note)+'</p>':'')+'</div></div>').join('')+'</div><div class="system-actions"><span>Leído a las '+when+'</span><button class="secondary-button" data-ui="export-system">'+icon('download')+'Exportar diagnóstico</button></div><details class="source-links"><summary>Documentación de las lecturas</summary><a href="https://developer.mozilla.org/en-US/docs/Web/API/Navigator/hardwareConcurrency" target="_blank" rel="noopener noreferrer">Hilos lógicos comunicados por el navegador ↗</a><a href="https://developer.mozilla.org/en-US/docs/Web/API/WEBGL_debug_renderer_info" target="_blank" rel="noopener noreferrer">Cadena del renderizador WebGL ↗</a></details>';
}
const APP_DESCRIPTIONS={files:'Carpetas, documentos y papelera.',notes:'Texto con guardado automático.',calculator:'Operaciones y precedencia aritmética.',control:'Aplicaciones, ventanas y lecturas del entorno.',settings:'Apariencia y preferencias del escritorio.',help:'Uso, atajos y límites del entorno.',xmarket:'Terminal bursátil y laboratorio Laplace. Solo interfaz.'};
function appsHTML(owner){
  const apps=SHELL_APPS;const visible=state.windows.filter(w=>w.space===(owner?.space??state.space));
  return '<div class="control-heading"><div><span class="eyebrow">HERRAMIENTAS INTEGRADAS</span><h2>Aplicaciones</h2><p>Herramientas y ejecutables de esta mesa.</p></div><span class="source-badge">'+apps.length+' integradas</span></div><div class="control-app-grid">'+apps.map(app=>'<button class="control-app-card" data-open="'+app+'">'+icon(APPS[app].icon)+'<span><strong>'+APPS[app].name+'</strong><small>'+APP_DESCRIPTIONS[app]+'</small></span>'+icon('arrow-right')+'</button>').join('')+'</div><div class="control-section-heading"><h3>Ventanas de '+(state.space?'Enfoque':'Principal')+'</h3><span>'+visible.length+' abiertas</span></div><div class="window-inventory">'+visible.map(w=>'<div class="inventory-row">'+icon(APPS[w.app].icon)+'<button data-ui-focus="'+w.id+'"><strong>'+esc(windowTitle(w))+'</strong><small>'+APPS[w.app].name+' · '+(w.min?'Minimizada':w.max?'Maximizada':'Visible')+'</small></button><button class="icon-button" data-ui-close="'+w.id+'" aria-label="Cerrar ventana '+esc(windowTitle(w))+'">'+icon('close')+'</button></div>').join('')+'</div><p class="control-note">Esta lista corresponde a las ventanas de la mesa. Las aplicaciones instaladas en Android y los procesos del sistema necesitan acceso nativo.</p>';
}
function backupHTML(){return '<div class="control-heading"><div><span class="eyebrow">TU ESPACIO</span><h2>Respaldo</h2><p>Conserva una copia de documentos, preferencias y ventanas.</p></div></div><div class="backup-card">'+icon('download')+'<h3>Exportar mi espacio</h3><p>Crea un archivo JSON que puedes guardar y restaurar en otra instalación de la mesa.</p><button class="primary-button" data-action="export-backup">Exportar respaldo</button></div><div class="backup-card">'+icon('upload')+'<h3>Restaurar una copia</h3><p>El archivo se valida antes de restaurar. Verás una confirmación antes de sustituir los datos actuales.</p><button class="secondary-button" data-action="import-backup">Importar respaldo</button></div><p class="control-note">Los ejecutables .rxe y la memoria de BBY Q se exportan desde sus ventanas. REVERSE Workbench guarda los datos en la aplicación; no hay sincronización automática.</p>';}
function controlHTML(w){
  const tab=['apps','appearance','system','backup'].includes(w.tab)?w.tab:'apps';w.tab=tab;
  const tabs=[['apps','Sesión','grid'],['appearance','Apariencia','palette'],['system','Sistema','desktop'],['backup','Respaldo','download']];
  return '<div class="control-layout"><nav class="control-sidebar" aria-label="Secciones del centro de control">'+tabs.map(([id,label,ico])=>'<button class="control-tab '+(tab===id?'selected':'')+'" data-ui-tab="'+id+'" aria-current="'+(tab===id?'page':'false')+'">'+icon(ico)+'<span>'+label+'</span></button>').join('')+'<div class="control-sidebar-footer">REVERSE<br><span>Workbench '+UI_BUILD+'</span></div></nav><div class="control-main">'+(tab==='apps'?appsHTML(w):tab==='system'?systemHTML():tab==='appearance'?'<div class="appearance-intro"><button class="secondary-button" data-open="wallpaper">'+icon('image')+'Abrir editor de fondo</button></div>'+settingsHTML():backupHTML())+'</div></div>';
}
function refreshControlLists(){for(const w of state.windows.filter(w=>w.app==='control'&&w.tab==='apps'))renderWindowContent(w);}
function resetWallpaper(){Object.assign(state.prefs,{wall:'pliegue',dim:16,wallX:50,wallY:50,scale:100,blur:0,saturation:100,fit:'cover'});applyPrefs();refreshSettings();queueSave();toast('Fondo predeterminado restaurado.');}
function handleTaskClick(app,button){
  hideLauncher();const list=state.windows.filter(w=>taskApp(w)===app&&w.space===state.space).sort((a,b)=>b.z-a.z);
  if(list.length>1){const r=button.getBoundingClientRect();showMenu(r.left,r.top-list.length*40,list.map(w=>({label:windowTitle(w)+(w.min?' · Minimizada':''),icon:APPS[w.app].icon,run:()=>focusWindow(w.id)})));return;}
  if(list.length){const w=list[0];if(activeId===w.id&&!w.min)minimizeWindow(w);else focusWindow(w.id);}else createWindow(app);
}
async function prepareWallpaperFile(file){
  if(file.size>8000000)throw Error('La imagen debe pesar menos de 8 MB.');
  if(!['image/png','image/jpeg','image/webp'].includes(file.type))throw Error('Elige una imagen PNG, JPG o WebP.');
  const url=URL.createObjectURL(file);
  try{const img=new Image();await new Promise((resolve,reject)=>{img.onload=resolve;img.onerror=()=>reject(Error('No se pudo abrir esta imagen.'));img.src=url;});const ratio=Math.min(1,1920/Math.max(img.naturalWidth,img.naturalHeight));const canvas=document.createElement('canvas');canvas.width=Math.max(1,Math.round(img.naturalWidth*ratio));canvas.height=Math.max(1,Math.round(img.naturalHeight*ratio));const context=canvas.getContext('2d');if(!context)throw Error('El navegador no permite adaptar esta imagen.');context.drawImage(img,0,0,canvas.width,canvas.height);let data=canvas.toDataURL('image/webp',.84);if(data.length>=1100000)data=canvas.toDataURL('image/jpeg',.65);if(!validImage(data))throw Error('La imagen adaptada sigue siendo demasiado grande.');return data;}finally{URL.revokeObjectURL(url);}
}
document.addEventListener('click',e=>{
  const button=e.target.closest('button');if(!button)return;const w=getWindow(button.closest('.window')?.dataset.id);
  if(button.dataset.taskApp){handleTaskClick(button.dataset.taskApp,button);return;}
  if(button.dataset.uiTab&&w){w.tab=button.dataset.uiTab;renderWindowContent(w);queueSave();return;}
  if(button.dataset.uiFocus){focusWindow(button.dataset.uiFocus);refreshControlLists();return;}
  if(button.dataset.uiClose){const target=getWindow(button.dataset.uiClose);if(target)closeWindow(target);refreshControlLists();return;}
  const action=button.dataset.ui;
  if(action==='bin')createWindow('files',{folder:'bin'});
  if(action==='control')createWindow('control',{tab:'apps'});
  if(action==='search')showLauncher(true);
  if(action==='restore-wallpaper')resetWallpaper();
  if(action==='refresh-system'){persist();systemSnapshot=collectSystemInfo();for(const item of state.windows.filter(x=>x.app==='control'&&x.tab==='system'))renderWindowContent(item);}
  if(action==='export-system'){persist();systemSnapshot=collectSystemInfo();downloadBlob('REVERSE-diagnostico-'+new Date().toISOString().slice(0,10)+'.json',JSON.stringify(systemSnapshot,null,2),'application/json');toast('Diagnóstico exportado con sus fuentes.');}
});
document.addEventListener('input',e=>{const key=e.target.dataset.pref;if(key){const out=$('[data-pref-output="'+key+'"]');if(out)out.textContent=e.target.value+(key==='blur'?' px':'%');}});

document.addEventListener('visibilitychange',()=>{document.body.dataset.pageHidden=String(document.hidden);});
