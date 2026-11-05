'use strict';

/* X MARKET — REVERSE Workbench first-party UI.
 * No feeds, future information, trades, or predictive runtime are connected.
 * Lab figures come from the isolated, reproducible XI Laplace experiment.
 */
const XM_ASSETS = Object.freeze([
  {id:'SPY',name:'S&P 500 ETF'},
  {id:'QQQ',name:'Nasdaq 100 ETF'},
  {id:'AAPL',name:'Apple'},
  {id:'NVDA',name:'NVIDIA'}
]);
const XM_HORIZONS = Object.freeze([
  {id:'1d',label:'1 día'},
  {id:'5d',label:'5 días'},
  {id:'20d',label:'20 días'}
]);
const XM_TABS = Object.freeze([
  {id:'market',label:'Mercado',ico:'market'},
  {id:'laplace',label:'Laplace',ico:'orbit'},
  {id:'research',label:'Experimentos',ico:'beaker'},
  {id:'about',label:'Configuración',ico:'sliders'}
]);
function xmValidTab(tab){return XM_TABS.some(x=>x.id===tab)?tab:'market';}
function xmValidAsset(asset){return XM_ASSETS.some(x=>x.id===asset)?asset:'SPY';}
function xmValidHorizon(h){return XM_HORIZONS.some(x=>x.id===h)?h:'1d';}
function xmDetails(w){
  w.marketTab=xmValidTab(w.marketTab);
  w.marketAsset=xmValidAsset(w.marketAsset);
  w.marketHorizon=xmValidHorizon(w.marketHorizon);
  w.marketShock=w.marketShock===true;
  return {asset:XM_ASSETS.find(a=>a.id===w.marketAsset),horizon:XM_HORIZONS.find(h=>h.id===w.marketHorizon)};
}
function xmPill(label,mod=''){return '<span class="xm-pill '+mod+'">'+label+'</span>';}
function xmHeading(kicker,title,description){
  return '<div class="xm-heading"><div><span class="xm-kicker">'+kicker+'</span><h2>'+title+'</h2><p>'+description+'</p></div>'+xmPill('SOLO INTERFAZ','neutral')+'</div>';
}
function xmMetric(label,value,note){
  return '<div class="xm-metric"><span>'+label+'</span><strong>'+value+'</strong><small>'+note+'</small></div>';
}
function xmTabButton(item,selected){
  return '<button type="button" class="xm-nav-button '+(selected?'selected':'')+'" data-xm-tab="'+item.id+'" aria-current="'+(selected?'page':'false')+'">'+icon(item.ico)+'<span>'+item.label+'</span></button>';
}
function xmMarket(w,selected){
  return xmHeading('TERMINAL / 01','Mercado','Panel de observación. Sin precios, señales ni ejecuciones inventadas.')+
    '<section class="xm-card xm-market-toolbar"><div class="xm-toolbar-left"><span class="xm-section-label">Instrumento</span><div class="xm-asset-list" role="group" aria-label="Instrumento">'+
    XM_ASSETS.map(a=>'<button type="button" data-xm-asset="'+a.id+'" aria-pressed="'+(w.marketAsset===a.id)+'" class="'+(w.marketAsset===a.id?'selected':'')+'">'+a.id+'</button>').join('')+
    '</div><span class="xm-asset-name">'+esc(selected.asset.name)+'</span></div><div class="xm-toolbar-right"><span class="xm-section-label">Horizonte</span><div class="xm-asset-list" role="group" aria-label="Horizonte">'+
    XM_HORIZONS.map(h=>'<button type="button" data-xm-horizon="'+h.id+'" aria-pressed="'+(w.marketHorizon===h.id)+'" class="'+(w.marketHorizon===h.id?'selected':'')+'">'+h.label+'</button>').join('')+
    '</div></div></section>'+
    '<section class="xm-metrics" aria-label="Indicadores no disponibles">'+
    xmMetric('Precio actual','—','Sin proveedor de datos')+
    xmMetric('Variación','—','Cotización no conectada')+
    xmMetric('Probabilidad','—','Modelo no calibrado')+
    xmMetric('Nivel de riesgo','—','Sin estimación verificada')+
    '</section>'+
    '<section class="xm-market-grid"><div class="xm-card xm-chart-card"><div class="xm-card-head"><div><strong>'+esc(selected.asset.id)+' / Histórico</strong><small>Serie de precios · '+esc(selected.horizon.label)+'</small></div>'+xmPill('SIN DATOS','muted')+'</div>'+
    '<div class="xm-empty-chart" aria-label="Sin cotizaciones reales; no se muestra una gráfica ficticia"><div class="xm-grid-h"></div><div class="xm-no-data">'+icon('market')+'<strong>Sin datos de mercado</strong><span>La gráfica aparecerá únicamente con cotizaciones verificables.</span></div></div>'+
    '<footer class="xm-card-foot">Fuente: no configurada <span>Última actualización: —</span></footer></div>'+
    '<aside class="xm-card xm-research-side"><div class="xm-card-head"><div><strong>Observación</strong><small>Estados del sistema</small></div></div>'+
    '<div class="xm-state-row"><span>Alimentación bursátil</span>'+xmPill('Desconectado','muted')+'</div>'+
    '<div class="xm-state-row"><span>Motor predictivo XI</span>'+xmPill('Pendiente','muted')+'</div>'+
    '<div class="xm-state-row"><span>Backtesting</span>'+xmPill('Sin ejecutar','muted')+'</div>'+
    '<div class="xm-state-row"><span>Operaciones</span>'+xmPill('Deshabilitadas','muted')+'</div>'+
    '<div class="xm-research-notice">'+icon('orbit')+'<p>El Laboratorio de Laplace está disponible para explorar la diferencia entre cálculo exacto e información desconocida.</p><button type="button" data-xm-tab="laplace">Abrir laboratorio '+icon('arrow-right')+'</button></div></aside></section>';
}
function xmPath(withShock){
  const result=[];
  for(let t=0;t<=40;t++){
    const cents=10000+7*t+3*t*(t-1)/2+(withShock&&t>=20?250:0);
    const x=24+t*9.3, y=158-(cents-10000)*.048;
    result.push(x.toFixed(1)+','+y.toFixed(1));
  }
  return result.join(' ');
}
function xmDemoGraph(shock){
  return '<svg viewBox="0 0 430 200" role="img" aria-label="Demostración sintética. Predicción cerrada y escenario con un evento de 250 céntimos no conocido al pronosticar.">'+
    '<g class="xm-graph-grid"><path d="M24 32H407M24 76H407M24 120H407M24 158H407M24 15V158M117 15V158M210 15V158M303 15V158M396 15V158"/></g>'+
    '<path class="xm-graph-base" d="M210 15V158" stroke-dasharray="3 6"/>'+
    '<polyline class="xm-graph-forecast" points="'+xmPath(false)+'"/>'+
    (shock?'<polyline class="xm-graph-shock" points="'+xmPath(true)+'"/>':'')+
    (shock?'<circle class="xm-graph-event" cx="210" cy="124" r="4"/>':'')+
    '<text x="24" y="179">0</text><text x="201" y="179">20</text><text x="390" y="179">40</text>'+
    '<text x="28" y="11">Precio sintético</text><text x="264" y="193">Paso temporal</text>'+
    '</svg>';
}
function xmLaplace(w){
  const shock=w.marketShock;
  return xmHeading('LABORATORIO / 02','Laplace','El cálculo puede ser exacto dentro de un sistema definido. El mundo real agrega información que no conocemos.')+
    '<section class="xm-laplace-lead xm-card"><span class="xm-section-label">EL EXPERIMENTO</span><h3>¿Qué ocurre cuando el futuro sigue reglas conocidas?</h3><p>Usamos un modelo artificial de 40 pasos ejecutado y verificado en XI. La trayectoria depende de una regla fija. Luego introducimos un evento exterior que no estaba disponible al hacer el pronóstico.</p>'+
    '<div class="xm-formula">P(t+1) = P(t) + 7 + 3t <span>·</span> P(0) = 10 000 ¢</div></section>'+
    '<section class="xm-lab-controls xm-card"><div><strong>Condiciones del experimento</strong><small>Alterna entre dos escenarios sintéticos.</small></div><div class="xm-choice" role="group" aria-label="Escenario Laplace"><button type="button" data-xm-shock="off" class="'+(!shock?'selected':'')+'" aria-pressed="'+(!shock)+'">Sistema cerrado</button><button type="button" data-xm-shock="on" class="'+(shock?'selected':'')+'" aria-pressed="'+shock+'">Evento desconocido</button></div></section>'+
    '<section class="xm-laplace-grid"><div class="xm-card xm-lab-graph"><div class="xm-card-head"><div><strong>Trayectorias</strong><small>Datos calculados de ejemplo, no precios bursátiles</small></div>'+xmPill('SINTÉTICO','neutral')+'</div>'+
    '<div class="xm-lab-visual">'+xmDemoGraph(shock)+'</div><div class="xm-legend"><span><i class="forecast"></i>Pronóstico inicial</span>'+(shock?'<span><i class="shock"></i>Resultado con evento no observado</span>':'')+'</div></div>'+
    '<aside class="xm-card xm-lab-results"><span class="xm-section-label">RESULTADO DEL MODELO</span>'+
    '<div class="xm-result"><small>Predicción previa</small><strong>12 620 ¢</strong><span>Regla y estado inicial conocidos</span></div>'+
    '<div class="xm-result"><small>Resultado realizado</small><strong>'+(shock?'12 870':'12 620')+' ¢</strong><span>'+(shock?'Incluye un evento externo de +250 ¢':'Sin eventos externos')+'</span></div>'+
    '<div class="xm-result final"><small>Error de la predicción</small><strong>'+ (shock?'+250':'0') +' ¢</strong><span>'+(shock?'No se conocía el evento al pronosticar.':'Coincidencia exacta dentro del modelo.')+'</span></div></aside></section>'+
    '<div class="xm-explain">'+icon('help')+'<p><strong>Conclusión:</strong> '+(shock?'Conocer todas las ecuaciones no permite anticipar datos externos que no se han observado. Si el evento se conociera antes, la predicción condicionada sería 12 870 ¢.':'Las dos soluciones XI coinciden exactamente porque el sistema y las condiciones del experimento están completamente definidos.')+' Esto no es una señal para operar en bolsa.</p></div>';
}
function xmResearch(){
  const records=[
    {number:'01',title:'Sistema cerrado',metric:'0 ¢',result:'Dos procedimientos XI coincidieron en 12 620 céntimos.',state:'Cálculo exacto del modelo'},
    {number:'02',title:'Evento no observado',metric:'+250 ¢',result:'Un choque externo dejó un error en el pronóstico original.',state:'Frontera de información'},
    {number:'03',title:'Sensibilidad',metric:'×32 768',result:'Una diferencia inicial de 1 crece a 32 768 tras 15 iteraciones de F(x)=2x.',state:'Demostración matemática'}
  ];
  return xmHeading('INVESTIGACIÓN / 03','Experimentos','Registro del laboratorio XI separado de cualquier historial de operaciones bursátiles.')+
    '<div class="xm-research-intro xm-card"><div><span class="xm-section-label">REVERSE-PC / XI</span><h3>Primera prueba de la hipótesis</h3><p>El proyecto X MARKET Laplace Lab v0.1 pasó una ejecución automatizada en GitHub Actions el 27 de octubre de 2026. Es evidencia de exactitud formal, no de predicción de mercados reales.</p></div>'+xmPill('CI XI APROBADA','positive')+'</div>'+
    '<div class="xm-research-list">'+records.map(r=>'<article class="xm-card xm-experiment"><div class="xm-experiment-index">'+r.number+'</div><div><h3>'+r.title+'</h3><p>'+r.result+'</p><small>'+r.state+'</small></div><strong>'+r.metric+'</strong></article>').join('')+'</div>'+
    '<div class="xm-explain">'+icon('lock')+'<p>Estos resultados se cargan como documentación de una ejecución separada. Abrir X MARKET no ejecuta XI, no modifica la memoria de BBY Q ni crea transacciones.</p></div>';
}
function xmAbout(){
  return xmHeading('SISTEMA / 04','Configuración','Estado de las conexiones y capacidades realmente presentes en esta primera interfaz.')+
  '<section class="xm-about-grid"><div class="xm-card xm-settings-card"><h3>Conectividad</h3>'+
  '<div class="xm-setting"><span>Proveedor bursátil</span><strong>No configurado</strong></div>'+
  '<div class="xm-setting"><span>Streaming de precios</span><strong>No disponible</strong></div>'+
  '<div class="xm-setting"><span>Noticias y fundamentales</span><strong>No conectado</strong></div>'+
  '<div class="xm-setting"><span>Operaciones reales</span><strong>Deshabilitadas</strong></div>'+
  '</div><div class="xm-card xm-settings-card"><h3>Arquitectura</h3>'+
  '<div class="xm-setting"><span>Interfaz</span><strong>REVERSE Workbench</strong></div>'+
  '<div class="xm-setting"><span>Experimento matemático</span><strong>XI, validado externamente</strong></div>'+
  '<div class="xm-setting"><span>Ejecución local XI desde esta ventana</span><strong>Pendiente</strong></div>'+
  '<div class="xm-setting"><span>Precisión bursátil demostrada</span><strong>Sin datos</strong></div>'+
  '</div></section>'+
  '<section class="xm-card xm-roadmap"><span class="xm-section-label">SIGUIENTE ETAPA</span><h3>Interfaz lista para conectar componentes reales</h3><p>La fuente de datos, el almacenamiento temporal, el motor estadístico, la auditoría de pronósticos y las pruebas financieras no forman parte de esta entrega visual. No se simulan como si estuvieran instalados.</p>'+
  '<div class="xm-roadmap-items"><span>01 / Datos históricos</span><span>02 / Motor XI</span><span>03 / Pronósticos fechados</span><span>04 / Auditoría temporal</span></div></section>';
}
function xMarketHTML(w){
  const d=xmDetails(w),tab=w.marketTab;
  let content='';
  if(tab==='laplace')content=xmLaplace(w);
  else if(tab==='research')content=xmResearch();
  else if(tab==='about')content=xmAbout();
  else content=xmMarket(w,d);
  return '<div class="xm-shell" data-xm-ui="v0.1"><aside class="xm-side"><div class="xm-logo">'+icon('market')+
    '<span><strong>X MARKET</strong><small>REVERSE / EXPERIMENTAL</small></span></div>'+
    '<div class="xm-nav-label">TERMINAL</div><nav class="xm-navigation" aria-label="X MARKET">'+XM_TABS.map(t=>xmTabButton(t,tab===t.id)).join('')+'</nav>'+
    '<div class="xm-side-bottom"><span class="xm-status-dot"></span><span>Datos reales<br><strong>No conectados</strong></span></div></aside>'+
    '<div class="xm-main"><header class="xm-topline"><span>REVERSE <b>/</b> X MARKET <b>/</b> '+esc(XM_TABS.find(t=>t.id===tab).label)+'</span><span>LAB v0.1 <i></i> SIN TRADING</span></header>'+
    '<div class="xm-scroll">'+content+'</div></div></div>';
}
document.addEventListener('click',event=>{
  const button=event.target.closest('button[data-xm-tab],button[data-xm-asset],button[data-xm-horizon],button[data-xm-shock]');
  if(!button)return;
  const owner=button.closest('.window');
  if(!owner)return;
  const w=getWindow(owner.dataset.id);
  if(!w||w.app!=='xmarket')return;
  if(button.dataset.xmTab)w.marketTab=xmValidTab(button.dataset.xmTab);
  else if(button.dataset.xmAsset)w.marketAsset=xmValidAsset(button.dataset.xmAsset);
  else if(button.dataset.xmHorizon)w.marketHorizon=xmValidHorizon(button.dataset.xmHorizon);
  else if(button.dataset.xmShock)w.marketShock=button.dataset.xmShock==='on';
  renderWindowContent(w);
  queueSave();
});
