'use strict';
const guestUi={snapshot:null,busy:false,error:'',output:'',draft:'mem_read 1073778696\n',steps:1000,input:'',running:false,pausePending:false,resumePending:false,frame:null,frameNumber:-1};
function refreshGuestViews(){for(const w of state.windows)if(w.app==='xi'&&w.xiTab==='arm64')renderWindowContent(w);}
function guestHTML(){
  if(nativeAvailable&&!guestUi.snapshot&&!guestUi.busy&&!guestUi.error)queueMicrotask(()=>guestOperation('guestInspect'));
  const g=guestUi.snapshot?.guest,disabled=(guestUi.busy||guestUi.running)?' disabled':'';
  const row=(name,value)=>'<div class="diagnostic-row"><span>'+esc(name)+'</span><strong>'+esc(value)+'</strong></div>';
  const details=g?'<div class="diagnostic-table">'+row('Estado',g.halted?'Terminado · código '+g.exit_code:g.waiting?'Esperando interrupción o entrada':'Listo para ejecutar')+
    row('Modo',g.system_mode?'Sistema · EL'+g.exception_level+' · MMU '+(g.mmu_enabled?'activa':'inactiva'):'Aplicación · EL0')+
    row('RAM lógica',Number(g.logical_ram_bytes).toLocaleString('es')+' bytes')+row('Programa conservado',g.immutable_image_bytes+' bytes')+
    row('Bytes diferentes',g.delta_overrides)+row('Último checkpoint',(guestUi.snapshot.snapshot_bytes??guestUi.snapshot.last_checkpoint_bytes)+' bytes'+(guestUi.snapshot.checkpoint_dirty?' · cambios pendientes':''))+
    (guestUi.snapshot.journal_rolled_back?row('Recuperación','Último guardado válido; tramo incompleto descartado'):'')+
    row('Instrucciones ejecutadas',g.instructions)+row('Excepciones atendidas',g.exceptions||0)+row('Posición',g.pc)+'</div>':'<p>Prueba la compatibilidad de sistema o importa un programa ARM64.</p>';
  return '<div class="programs-layout guest-layout"><span class="eyebrow">XI / ETHER · COMPATIBILIDAD</span><h2>ARM64 en REVERSE</h2>'+
    '<p class="guest-boundary">Extensión del ejecutor de REVERSE con memoria XI/Ether. Android y Wild Rift todavía no arrancan.</p>'+
    '<div class="program-actions"><button class="primary-button" data-guest="system"'+disabled+'>Comprobar modo de sistema</button><button class="secondary-button" data-guest="float"'+disabled+'>Comprobar FP/SIMD</button><button class="secondary-button" data-guest="demo"'+disabled+'>Prueba 6×7</button><button class="secondary-button" data-guest="import"'+disabled+'>Importar ELF</button><button class="secondary-button" data-guest="system-elf"'+disabled+'>ELF de sistema</button><button class="secondary-button" data-guest="linux"'+disabled+'>Importar Linux Image</button><button class="secondary-button" data-guest="checkpoint"'+disabled+'>Importar checkpoint</button><button class="secondary-button" data-guest="inspect"'+disabled+'>Consultar</button></div>'+
    '<div class="program-actions"><button class="secondary-button" data-guest="graphics"'+disabled+'>Comprobar gráfica PCI</button><button class="secondary-button" data-guest="disk"'+disabled+'>Adjuntar disco GPT</button><button class="secondary-button" data-guest="display"'+(!g||guestUi.busy?' disabled':'')+'>Ver pantalla</button></div>'+
    '<p class="rxe-status" role="status">'+esc(guestUi.error||(guestUi.busy?(guestUi.running?'Ejecutando el invitado…':'Procesando y guardando el invitado…'):guestUi.running?'Continuando el invitado…':nativeAvailable?'La sesión se conserva al pausar.':'Abre REVERSE en Android para ejecutar el invitado.'))+'</p>'+details+
    (guestUi.frame?'<figure><img src="'+esc(guestUi.frame)+'" alt="Salida gráfica del invitado" style="display:block;width:100%;max-width:640px;height:auto;image-rendering:pixelated"><figcaption>Salida gráfica del invitado. La comprobación PCI no inicia Android.</figcaption></figure>':'')+
    '<p class="guest-note">La RAM vacía se describe con una regla. El programa y sus cambios sí consumen memoria física; la RAM lógica no mide la RAM disponible del teléfono.</p>'+
    '<div class="program-actions"><label>Pasos <input class="guest-steps" type="number" min="1" max="10000" value="'+guestUi.steps+'" aria-label="Pasos del invitado"'+disabled+'></label><button class="primary-button" data-guest="run"'+(guestUi.busy||guestUi.running||!g||g.halted?' disabled':'')+'>Ejecutar bloque</button><button class="primary-button" data-guest="continuous"'+(!guestUi.running&&(guestUi.busy||!g||g.halted)?' disabled':'')+'>'+(guestUi.running?'Pausar al guardar':'Continuar arranque')+'</button><button class="secondary-button" data-guest="export"'+(guestUi.busy||guestUi.running||!g?' disabled':'')+'>Exportar checkpoint</button></div>'+
    '<pre class="rxe-output" aria-label="Salida del invitado">'+esc(guestUi.output||'La salida aparecerá cuando el programa la produzca.')+'</pre>'+
    (g?.system_mode?'<div class="program-actions"><label>Consola <input class="guest-input" type="text" maxlength="1024" value="'+esc(guestUi.input)+'" aria-label="Entrada de consola"'+disabled+'></label><button class="secondary-button" data-guest="input"'+disabled+'>Enviar</button></div>':'')+
    '<details><summary>XI sobre el delta de memoria</summary><p>XI y RXE conservan las diferencias. En direcciones sin imagen, el delta coincide con el byte; sobre el programa, se combina por XOR.</p><textarea class="guest-source" aria-label="Programa XI del delta" maxlength="65536" spellcheck="false"'+disabled+'>'+esc(guestUi.draft)+'</textarea><button class="secondary-button" data-guest="xi"'+(guestUi.busy||guestUi.running||!g?' disabled':'')+'>Aplicar XI</button></details></div>';
}
async function guestOperation(operation,payload=''){
  if(guestUi.busy)return;
  if(!nativeAvailable){guestUi.error='Esta función necesita la aplicación Android.';refreshGuestViews();return;}
  guestUi.busy=true;guestUi.error='';refreshGuestViews();
  try{
    const response=await nativeRequest(operation,payload);
    if(operation==='guestDisplay'){guestUi.frame=response.display_ready?response.frame_png:null;guestUi.frameNumber=response.guest?.display_frame??-1;}
    else guestUi.snapshot=response;
    if(['guestDemo','guestSystem','guestFloat','guestGraphics'].includes(operation)){guestUi.output='';guestUi.frame=null;guestUi.frameNumber=-1;}
    if(response.stdout!==undefined||response.stderr!==undefined)guestUi.output=(guestUi.output+(response.stdout||'')+(response.stderr||'')).slice(-65536);
    if(response.xi_output!==undefined)guestUi.output=response.xi_output;
  }catch(error){guestUi.error=error.message;}
  finally{guestUi.busy=false;refreshGuestViews();}
  if(guestUi.pausePending)await flushGuestPause();
  if(guestUi.resumePending)await flushGuestResume();
  if(operation!=='guestDisplay'&&!guestUi.error&&!guestUi.snapshot?.paused&&!document.hidden&&document.body?.dataset.appPaused!=='true'&&guestUi.snapshot?.guest?.display_width>0&&guestUi.snapshot.guest.display_frame!==guestUi.frameNumber)await guestOperation('guestDisplay');
}
async function continueGuest(){
  if(guestUi.running){await pauseGuest();return;}
  if(guestUi.busy||guestUi.pausePending||!guestUi.snapshot?.loaded||guestUi.snapshot.guest?.halted)return;
  guestUi.running=true;const continuation=Date.now().toString(36)+'-'+Math.random().toString(36).slice(2);refreshGuestViews();
  while(guestUi.running){
    await guestOperation('guestRunContinuous',JSON.stringify({continuation}));
    const g=guestUi.snapshot?.guest;
    if(guestUi.snapshot?.continuation_paused||guestUi.error||!g||g.halted||g.waiting||document.hidden||
       !state.windows.some(w=>w.app==='xi'&&w.xiTab==='arm64'&&!w.min&&w.space===state.space))guestUi.running=false;
    if(guestUi.running)await new Promise(resolve=>setTimeout(resolve,30));
  }
  await pauseGuest();
}
async function flushGuestPause(){
  if(!nativeAvailable){guestUi.pausePending=false;return;}
  if(!guestUi.pausePending||guestUi.busy)return;
  guestUi.pausePending=false;guestUi.busy=true;refreshGuestViews();
  try{const result=await nativeRequest('guestPause','');if(result.guest)guestUi.snapshot=result;}
  catch(error){guestUi.error=guestUi.error?guestUi.error+' · '+error.message:error.message;}
  finally{guestUi.busy=false;refreshGuestViews();}
  if(guestUi.pausePending)await flushGuestPause();
  if(guestUi.resumePending)await flushGuestResume();
}
async function flushGuestResume(){
  if(!guestUi.resumePending||guestUi.busy||guestUi.pausePending)return;
  guestUi.resumePending=false;const previous=guestUi.error;
  await guestOperation('guestInspect');
  if(previous){guestUi.error=guestUi.error?previous+' · '+guestUi.error:previous;refreshGuestViews();}
}
async function resumeGuest(){
  if(!nativeAvailable||!guestUi.snapshot?.loaded||!state.windows.some(w=>w.app==='xi'&&w.xiTab==='arm64'&&!w.min&&w.space===state.space))return;
  guestUi.resumePending=true;await flushGuestResume();
}
async function pauseGuest(){guestUi.running=false;guestUi.resumePending=false;guestUi.pausePending=true;refreshGuestViews();await flushGuestPause();}
window.ReverseGuest={cancelImport(){guestUi.busy=false;refreshGuestViews();},pause:pauseGuest,resume:resumeGuest,imported(response){
  guestUi.running=false;guestUi.busy=false;
  if(response.ok){guestUi.snapshot=response;guestUi.error='';guestUi.output='';guestUi.frame=null;guestUi.frameNumber=-1;}
  else guestUi.error=response.error||'Archivo rechazado.';
  refreshGuestViews();
}};
document.addEventListener('input',event=>{
  if(event.target.matches('.guest-source'))guestUi.draft=event.target.value;
  if(event.target.matches('.guest-steps'))guestUi.steps=Math.max(1,Math.min(10000,Number(event.target.value)||1));
  if(event.target.matches('.guest-input'))guestUi.input=event.target.value;
});
document.addEventListener('click',event=>{
  const button=event.target.closest('[data-guest]');if(!button||button.disabled)return;
  const action=button.dataset.guest;
  if(action==='demo'||action==='system'||action==='float'||action==='graphics'||action==='import'||action==='system-elf'||action==='linux'||action==='checkpoint'){
    if(guestUi.snapshot?.loaded&&!confirm('Esto reemplazará el invitado guardado. Exporta su checkpoint antes si deseas conservarlo. ¿Continuar?'))return;
    if(['import','system-elf','linux','checkpoint'].includes(action)){
      if(!nativeAvailable){guestUi.error='Esta función necesita la aplicación Android.';refreshGuestViews();return;}
      guestUi.busy=true;guestUi.error='';refreshGuestViews();
    }
    if(action==='checkpoint'&&typeof window.ReverseNative?.importGuestCheckpoint==='function')window.ReverseNative.importGuestCheckpoint();
    else if(action==='demo')guestOperation('guestDemo');
    else if(action==='system')guestOperation('guestSystem');
    else if(action==='float')guestOperation('guestFloat');
    else if(action==='graphics')guestOperation('guestGraphics');
    else if(action==='linux'&&typeof window.ReverseNative?.importLinuxImage==='function')window.ReverseNative.importLinuxImage();
    else if(action==='system-elf'&&typeof window.ReverseNative?.importSystemElf==='function')window.ReverseNative.importSystemElf();
    else if(typeof window.ReverseNative?.importGuestElf==='function')window.ReverseNative.importGuestElf();
  }
  if(action==='disk'&&typeof window.ReverseNative?.importGuestDisk==='function'){guestUi.busy=true;guestUi.error='';refreshGuestViews();window.ReverseNative.importGuestDisk();}
  if(action==='display')guestOperation('guestDisplay');
  if(action==='continuous')continueGuest();
  if(action==='inspect')guestOperation('guestInspect');
  if(action==='run')guestOperation('guestRun',JSON.stringify({steps:guestUi.steps}));
  if(action==='xi')guestOperation('guestXi',guestUi.draft);
  if(action==='input')guestOperation('guestInput',guestUi.input+'\n');
  if(action==='export'&&typeof window.ReverseNative?.exportGuest==='function')window.ReverseNative.exportGuest();
});

document.addEventListener('visibilitychange',()=>{if(document.hidden)window.ReverseGuest.pause();});


