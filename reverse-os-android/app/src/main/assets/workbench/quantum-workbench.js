'use strict';
// Programs generated for the user's XI VM. There is no browser quantum engine.
function quantumProgram(kind,p={}){
  const integer=(v,min,max)=>{if(!Number.isInteger(Number(v))||Number(v)<min||Number(v)>max)throw Error('Revisa los parámetros del programa.');return Number(v);};
  if(!['phase','register','ising'].includes(kind))throw Error('Operación desconocida.');
  let source='#!reverse-pc-machine-v1\n';
  if(kind==='phase')return source+'h 0\nt 0\nexpect X:0\nexpect Y:0\nexpect Z:0\nqstats\nhalt\n';
  if(kind==='register'){
    const n=integer(p.qubits??4096,1,4096);
    return source+'set r0 0\nlabel sites\nh $r0\nt $r0\nadd r0 1\ncmp r0 '+n+'\njlt sites\nexpect X:0\nexpect X:'+(n-1)+'\nqstats\nhalt\n';
  }
  const j=integer(p.coupling??1,-3,3),field=integer(p.field??1,-3,3),steps=integer(p.steps??1,1,16);
  source+='h 0\nh 1\n';
  const phase=(q,n)=>(n<0?'tdg ':'t ')+q+'\n';
  for(let s=0;s<steps;s++){
    for(let k=0;k<Math.abs(j);k++)source+='cx 0 1\n'+phase(1,j)+'cx 0 1\n';
    for(let q=0;q<2;q++)if(field){source+='h '+q+'\n';for(let k=0;k<Math.abs(field);k++)source+=phase(q,field);source+='h '+q+'\n';}
  }
  return source+'expect Z:0 Z:1\nexpect X:0\nexpect X:1\nqstats\nhalt\n';
}
function quantumResultHTML(w){
  const result=w.quantumResult;if(!result)return '<div class="quantum-empty">'+icon('orbit')+'<p>El resultado aparecerá aquí después de ejecutar.</p></div>';
  let records=[];try{records=result.output.split('\n').filter(s=>s.startsWith('{')).map(s=>JSON.parse(s));}catch{}
  const values=records.filter(r=>r.type==='expectation').map(r=>r.value),fmt=n=>Number(n).toLocaleString('es',{maximumFractionDigits:8});
  let measurements='';
  if(result.kind==='ising'&&values.length===3){const p=result.parameters;const energy=p.coupling*values[0]+p.field*(values[1]+values[2]);measurements='<div class="energy-reading"><span>⟨H⟩ · energía esperada</span><strong>'+fmt(energy)+'</strong></div>'+['⟨Z₀Z₁⟩','⟨X₀⟩','⟨X₁⟩'].map((label,i)=>'<div class="observable-reading"><span>'+label+'</span><strong>'+fmt(values[i])+'</strong></div>').join('');}
  else measurements=values.map((n,i)=>'<div class="observable-reading"><span>'+(result.kind==='phase'?['⟨X₀⟩','⟨Y₀⟩','⟨Z₀⟩'][i]:i?'⟨Xúltimo⟩':'⟨X₀⟩')+'</span><strong>'+fmt(n)+'</strong></div>').join('');
  return '<section class="quantum-result"><div class="quantum-result-heading"><strong>Último resultado</strong><span>NÚCLEO NATIVO</span></div><div class="quantum-result-meta"><span>'+Number(result.core.active_qubits).toLocaleString('es')+' qubits activos</span><span>'+result.core.components+' componentes</span></div>'+measurements+'<details class="quantum-raw"><summary>Salida XI</summary><pre>'+esc(result.output)+'</pre></details></section>';
}
const xiEditorHTML=xiHTML;
xiHTML=function(w){
  const tab=w.xiTab||'editor';
  const tabs='<nav class="xi-tabs" aria-label="Secciones de XI">'+[['editor','Editor'],['quantum','Operadores'],['hamiltonian','Hamiltonianos'],['arm64','ARM64']].map(([key,label])=>'<button data-quantum-tab="'+key+'" class="'+(tab===key?'selected':'')+'"'+(tab===key?' aria-current="page"':'')+'>'+label+'</button>').join('')+'</nav>';
  if(tab==='editor')return '<div class="xi-workbench">'+tabs+xiEditorHTML(w)+'</div>';
  if(tab==='arm64')return '<div class="xi-workbench">'+tabs+guestHTML()+'</div>';
  const p=w.hamiltonian||{coupling:1,field:1,steps:1};
  let content;
  if(tab==='quantum')content='<div class="quantum-heading"><span class="home-date">OPERADORES XI / ETHER</span><h2>Control del registro</h2><p>H, CX, T y T† ejecutados por el motor local.</p></div><section class="quantum-operation"><div><h3>Fase T</h3><p>Un qubit · H → T · observables X, Y, Z.</p></div><button class="primary-button" data-quantum-run="phase"'+(w.xiBusy?' disabled':'')+'>Ejecutar</button></section><section class="quantum-operation register-operation"><div><h3>Registro independiente</h3><p>Una puerta H y una T en cada qubit.</p></div><label class="quantum-field"><span>Qubits</span><input type="number" inputmode="numeric" min="1" max="4096" value="4096" data-qubit-count aria-label="Qubits del experimento"></label><button class="primary-button" data-quantum-run="register"'+(w.xiBusy?' disabled':'')+'>Ejecutar</button></section><p class="quantum-scope">Hasta 4.096 qubits independientes en el perfil actual. Un circuito entrelazado tiene otros límites de memoria y componentes.</p>';
  else content='<div class="quantum-heading"><span class="home-date">EVOLUCIÓN Y OBSERVABLES</span><h2>Hamiltoniano de Ising</h2><p>Dos qubits · estado inicial |++⟩</p></div><div class="hamiltonian-equation">H = J Z₀Z₁ + h (X₀ + X₁)</div><div class="hamiltonian-fields">'+[['coupling','Acoplamiento J',-3,3],['field','Campo h',-3,3],['steps','Pasos',1,16]].map(([key,label,min,max])=>'<label class="quantum-field"><span>'+label+'</span><input data-hamiltonian="'+key+'" type="number" inputmode="numeric" min="'+min+'" max="'+max+'" value="'+p[key]+'"></label>').join('')+'</div><div class="quantum-run-row"><span>Δt = π/8 · producto de primer orden</span><button class="primary-button" data-quantum-run="ising"'+(w.xiBusy?' disabled':'')+'>Evolucionar</button></div><details class="quantum-method"><summary>Método de evolución</summary><p>Descomposición H/CX/T/T†. Si J y h son simultáneos, cada paso aproxima la evolución mediante un producto de Trotter de primer orden.</p></details>';
  return '<div class="xi-workbench">'+tabs+'<div class="quantum-pane">'+content+'<div class="quantum-state-note"><span class="tiny-status"></span><span>Experimento aislado · conserva tu sesión de trabajo</span></div><p class="quantum-status" role="status">'+esc(w.xiBusy?'Ejecutando en el núcleo…':w.quantumError||'')+'</p>'+quantumResultHTML(w)+(w.quantumResult?'<button class="secondary-button quantum-source-button" data-quantum-source>Ver programa en XI</button>':'')+'</div></div>';
};
document.addEventListener('click',async e=>{
  const b=e.target.closest('button');if(!b)return;const w=getWindow(b.closest('.window')?.dataset.id);if(!w||w.app!=='xi')return;
  if(b.dataset.quantumTab){w.xiTab=b.dataset.quantumTab;renderWindowContent(w);queueSave();return;}
  if(b.hasAttribute('data-quantum-source')){w.xiTab='editor';renderWindowContent(w);queueSave();return;}
  if(!b.dataset.quantumRun||w.xiBusy)return;
  const kind=b.dataset.quantumRun,parameters={};
  try{
    if(kind==='register')parameters.qubits=Number($('[data-qubit-count]',$('#'+w.id)).value);
    if(kind==='ising')for(const input of $$('[data-hamiltonian]',$('#'+w.id)))parameters[input.dataset.hamiltonian]=Number(input.value);
    const source=quantumProgram(kind,parameters);w.xiBusy=true;w.quantumError='';w.xiSource=source;if(kind==='ising')w.hamiltonian=parameters;renderWindowContent(w);
    const result=await nativeRequest('experiment',source);
    w.xiOutput=result.output;w.quantumResult={kind,parameters,output:result.output,core:result.core,capturedAt:Date.now()};
  }catch(error){w.quantumError=error.message;}
  finally{w.xiBusy=false;renderWindowContent(w);queueSave();}
});

// desktop.js mounts saved windows before this tab renderer is installed.
// Re-render the restored XI views so the visible ARM64 pane inspects its
// saved guest without needing another click after process restart.
for(const w of state.windows)if(w.app==='xi')renderWindowContent(w);
