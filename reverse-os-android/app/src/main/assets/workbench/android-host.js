'use strict';
const nativeAvailable=typeof window.ReverseNative!=='undefined';
let androidSnapshot=null,androidError=null,androidReading=false,androidReadAt=0,nativeSerial=0;
const nativePending=new Map();
let nativeProfileSequence=0,nativeProfileAcknowledged=0;
window.ReverseAndroid={complete(id,response){const pending=nativePending.get(id);if(!pending)return;nativePending.delete(id);clearTimeout(pending.timeout);if(response.ok)pending.resolve(response);else pending.reject(Error(response.error||'El núcleo rechazó la operación.'));}};
function nativeRequest(operation,payload=''){
  if(!nativeAvailable)return Promise.reject(Error('Este programa requiere la APK nativa.'));
  return new Promise((resolve,reject)=>{const id='n'+(++nativeSerial);const timeout=setTimeout(()=>{nativePending.delete(id);reject(Error('La operación sigue sin resultado confirmado. Actualiza el estado antes de repetirla.'));},60000);nativePending.set(id,{resolve,reject,timeout});window.ReverseNative.request(id,operation,payload);});
}
function persist(){
  if(!nativeAvailable)return persistWeb();
  const sequence=++nativeProfileSequence;
  try{
    const raw=JSON.stringify(state);if(raw.length>3600000)throw Error('Perfil demasiado grande');
    try{localStorage.setItem(STORE_KEY,raw);}catch{}
    $$('.save-label').forEach(el=>el.textContent='Guardando en Android…');
    return nativeRequest('saveProfile',raw).then(()=>{
      nativeProfileAcknowledged=Math.max(sequence,nativeProfileAcknowledged);savedAt=now();saveError=null;
      if(nativeProfileAcknowledged===nativeProfileSequence)$$('.save-label').forEach(el=>el.textContent='Guardado en Android');
      return true;
    }).catch(error=>{saveError=error.message;$$('.save-label').forEach(el=>el.textContent='Sin guardar: exporta una copia');toast('No se pudo guardar el perfil: '+error.message,'help');return false;});
  }catch(error){saveError=error.message;toast(error.message,'help');return Promise.resolve(false);}
}
async function refreshAndroid(force=false){
  if(!nativeAvailable||androidReading||(!force&&Date.now()-androidReadAt<5000))return;
  androidReading=true;
  try{androidSnapshot=await nativeRequest('inspect');androidError=null;androidReadAt=Date.now();}
  catch(error){androidError=error.message;androidSnapshot=null;androidReadAt=Date.now();}
  finally{androidReading=false;systemSnapshot=null;for(const w of state.windows.filter(w=>w.app==='control'&&w.tab==='system'))renderWindowContent(w);}
}
function collectSystemInfo(){
  const report=collectWebSystemInfo(!nativeAvailable);if(!nativeAvailable)return report;
  refreshAndroid();
  const nativeLabels=['Entorno','Procesador físico','Hilos lógicos disponibles','RAM física / consumo del sistema','XI / Ether','Interfaz','Datos guardados del escritorio'];
  report.rows=report.rows.filter(row=>!nativeLabels.includes(row.label));
  report.rows.unshift(diagnosticRow('Entorno','APK Android local','WorkbenchActivity · contenido empaquetado, sin servidor'));
  if(!androidSnapshot){report.rows.unshift(diagnosticRow('XI / Ether',androidError?'Error: '+androidError:'Leyendo el núcleo…','NativeRuntime.inspect sobre session.rvp',androidError?'unavailable':'pending'));return report;}
  const d=androidSnapshot,a=d.android,q=d.core;report.nativeCapturedAt=new Date(d.captured_at_ms).toISOString();report.scope='Lecturas nativas del dispositivo actual y del checkpoint RVPC2 de esta aplicación.';
  report.rows.unshift(diagnosticRow('XI / Ether','Conectado · '+q.state_format,'JNI · NativeRuntime.inspect(session.rvp)'));
  const mib=bytes=>(bytes/1048576).toLocaleString('es',{maximumFractionDigits:1})+' MiB';
  report.rows.push(diagnosticRow('Dispositivo',a.model+' · Android '+a.android,'android.os.Build · MODEL / VERSION'));
  report.rows.push(diagnosticRow('SoC / plataforma',a.soc||a.hardware,'android.os.Build · SOC_MODEL / HARDWARE','reported'));
  report.rows.push(diagnosticRow('Procesadores lógicos disponibles',String(a.logical_processors),'Runtime.availableProcessors()','reported'));
  report.rows.push(diagnosticRow('RAM accesible al kernel',mib(a.ram_total_bytes),'ActivityManager.MemoryInfo.totalMem','read','Cantidad accesible al kernel anfitrión; no equivale a memoria lógica de Ether.'));
  report.rows.push(diagnosticRow('RAM disponible del sistema',mib(a.ram_available_bytes),'ActivityManager.MemoryInfo.availMem','read','Instantánea de Android; varía con otras aplicaciones.'));
  report.rows.push(diagnosticRow('PSS del proceso',mib(a.process_pss_kib*1024),'android.os.Debug.getPss()','read','Huella PSS del proceso anfitrión, no solo del núcleo.'));
  report.rows.push(diagnosticRow('CPU consumida por este proceso',a.process_cpu_ms+' ms acumulados','android.os.Process.getElapsedCpuTime()','read','Tiempo acumulado; no es un porcentaje de carga total de CPU.'));
  report.rows.push(diagnosticRow('Qubits activos',String(q.active_qubits),'Runtime.core().stats().active_qubits'));
  if(window.REVERSE_CAPABILITIES){const limits=window.REVERSE_CAPABILITIES;report.rows.push(diagnosticRow('Límite de qubits activos',String(limits.max_active_qubits),'Perfil verificado de la compilación'));report.rows.push(diagnosticRow('Qubits por componente',String(limits.max_component_qubits),'Perfil verificado de la compilación'));}
  report.rows.push(diagnosticRow('Componentes / mayor componente',q.components+' / '+q.largest_component,'Runtime.core().stats()'));
  report.rows.push(diagnosticRow('Bytes de tableaux',q.tableau_bytes+' bytes','Runtime.core().stats().tableau_bytes'));
  report.rows.push(diagnosticRow('Reglas Ether',String(q.ether_rules),'Runtime.core().stats().ether_rules'));
  report.rows.push(diagnosticRow('Excepciones / enlaces de memoria',q.memory_overrides+' / '+q.quantum_bindings,'Runtime.memory().stats()'));
  report.rows.push(diagnosticRow('Revisión núcleo / memoria',q.core_revision+' / '+q.memory_revision,'NativeRuntime.inspect · revisiones del estado'));
  report.rows.push(diagnosticRow('Checkpoint guardado',d.state_bytes+' bytes · '+q.state_format,'SessionStore.loadState()'));
  report.rows.push(diagnosticRow('Perfil del escritorio',d.profile_bytes+' bytes','AtomicFile · workbench-profile.json'));
  report.rows.push(diagnosticRow('Versión de la aplicación',d.package+' · '+(d.build?.source_commit||'Sin metadatos de compilación'),'Paquete Android / build-info.json'));
  return report;
}
function systemHTML(){
  if(!nativeAvailable)return webSystemHTML();
  systemSnapshot ||= collectSystemInfo();const d=systemSnapshot;
  return '<div class="control-heading"><div><span class="eyebrow">LECTURAS NATIVAS</span><h2>Sistema</h2><p>Android y núcleo REVERSE de este dispositivo.</p></div><button class="secondary-button" data-ui="refresh-system">'+icon('restore')+'Actualizar</button></div><div class="system-boundary">'+icon('desktop')+'<span>'+(androidSnapshot?'XI/Ether cargado. Calculadora y programas XI usan el runtime original. La interfaz se dibuja con WebView.':androidError?esc(androidError):'Consultando la biblioteca nativa y el checkpoint RVPC2…')+'</span></div><div class="diagnostic-table">'+d.rows.map(row=>'<div class="diagnostic-row"><div><span class="diagnostic-label">'+esc(row.label)+'</span><small>'+esc(row.source)+'</small></div><div><strong class="'+(['unavailable','disconnected'].includes(row.status)?'unavailable':'')+'">'+esc(row.value)+'</strong>'+(row.note?'<p>'+esc(row.note)+'</p>':'')+'</div></div>').join('')+'</div><div class="system-actions"><span>Fuente: JNI / Android</span><button class="secondary-button" data-native="export-diagnostic">'+icon('download')+'Exportar diagnóstico</button></div><div class="wall-editor-actions"><button class="secondary-button" data-open="xi">'+icon('note')+'Pruebas XI</button><button class="secondary-button" data-open="programs">'+icon('desktop')+'Aplicaciones .rxe</button></div>';
}
async function calcKey(w,key){
  if(w.calc?.busy)return;
  if(!nativeAvailable||!['=','%'].includes(key))return calcKeyWeb(w,key);
  const c=w.calc||={expression:'',history:''};if(c.busy)return;c.busy=true;
  const expression=c.expression;
  try{const result=await nativeRequest('calculate',key==='%'?'('+expression+')/100':expression);c.history=expression+(key==='%'?' %':' =')+' · XI';c.expression=result.value;const el=$('#'+w.id);if(el){$('.calc-display',el).value=c.expression;$('.calc-history',el).textContent=c.history;}queueSave();refreshAndroid(true);}
  catch(error){toast(error.message,'help');}
  finally{c.busy=false;}
}
const XI_PROBE='#!reverse-pc-machine-v1\n# Aritmética y entrelazamiento reales; se conserva el checkpoint.\nset r0 6\nmul r0 7\nprint r0\nh 200\nset r1 200\nlabel unir\nmov r2 r1\nadd r2 1\ncx $r1 $r2\nmov r1 r2\ncmp r1 327\njlt unir\nt 327\nexpect Z:200 Z:327\nqstats\nhalt\n';
function xiHTML(w){
  w.xiSource ||= XI_PROBE;
  return '<div class="xi-layout"><div class="xi-toolbar"><span>XI / Ether · checkpoint RVPC2 compartido</span><button class="secondary-button" data-program-action="compile">Crear .rxe</button><button class="primary-button" data-native="run-xi">'+icon('arrow-right')+'Ejecutar</button></div><label class="xi-label">Programa XI<textarea class="xi-source" data-xi-source maxlength="262144" spellcheck="false">'+esc(w.xiSource)+'</textarea></label><div class="xi-status">'+esc(w.xiStatus||'Ejecutar aplica el programa a tu sesión de trabajo. Operadores y Hamiltonianos usan un estado aislado.')+'</div><pre class="xi-output" aria-label="Salida real XI">'+esc(w.xiOutput||'Esperando ejecución.')+'</pre></div>';
}
Object.assign(APP_DESCRIPTIONS,{xi:'Ejecutar programas en el núcleo local XI/Ether.',programs:'Ejecutables .rxe en esta mesa.',bbyq:'Conversación y memoria privada de BBY Q.'});
document.addEventListener('input',e=>{if(e.target.matches('[data-xi-source]')){const w=getWindow(e.target.closest('.window')?.dataset.id);if(w){w.xiSource=e.target.value;queueSave();}}});
document.addEventListener('click',async e=>{
  const button=e.target.closest('button');if(!button)return;
  if(button.dataset.ui==='refresh-system')refreshAndroid(true);
  const action=button.dataset.native,w=getWindow(button.closest('.window')?.dataset.id);
  if(action==='run-xi'&&w){if(w.xiBusy)return;w.xiBusy=true;button.disabled=true;w.xiStatus='Ejecutando en el núcleo nativo…';$('.xi-status',$('#'+w.id)).textContent=w.xiStatus;
    try{const result=await nativeRequest('execute',w.xiSource);w.xiOutput=result.output;w.xiStatus='Estado guardado · '+(result.elapsed_ns/1e6).toFixed(2)+' ms · '+result.source;refreshAndroid(true);}
    catch(error){w.xiOutput=error.message;w.xiStatus='No se confirmó la ejecución.';}
    finally{w.xiBusy=false;renderWindowContent(w);queueSave();}
  }
  if(action==='export-diagnostic'){try{await refreshAndroid(true);systemSnapshot=collectSystemInfo();downloadBlob('REVERSE-diagnostico.json',JSON.stringify(systemSnapshot,null,2),'application/json');}catch(error){toast(error.message,'help');}}
});
window.addEventListener('DOMContentLoaded',()=>refreshAndroid(true));
