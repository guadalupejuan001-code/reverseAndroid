'use strict';
/* Runs against the shipped first-party interface module, not a rewritten mock app. */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const path=require('node:path');
const root=path.join(__dirname,'..','app','src','main','assets','workbench');
const ui=fs.readFileSync(path.join(root,'x-market.js'),'utf8');
const desktop=fs.readFileSync(path.join(root,'desktop.js'),'utf8');
const html=fs.readFileSync(path.join(root,'index.html'),'utf8');
const controls=fs.readFileSync(path.join(root,'control-center.js'),'utf8');
const listener={};
const w={id:'test-window',app:'xmarket'};
let redraws=0,saves=0;
const ctx=vm.createContext({
  document:{addEventListener(type,cb){listener[type]=cb;}},
  icon(name){return '<i data-icon="'+name+'"></i>';},
  esc(v){return String(v).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/"/g,'&quot;');},
  getWindow(id){return id===w.id?w:undefined;},
  renderWindowContent(owner){assert.equal(owner,w);redraws++;},
  queueSave(){saves++;}
});
vm.runInContext(ui,ctx,{filename:'x-market.js'});
function render(){return vm.runInContext('xMarketHTML(w)',Object.assign(ctx,{w}));}
function press(key,value){
  const windowEl={dataset:{id:'test-window'}};
  const button={dataset:{[key]:value},closest(selector){return selector==='.window'?windowEl:null;}};
  listener.click({target:{closest(selector){return selector.startsWith('button[')?button:null;}}});
}
const landing=render();
assert.match(landing,/X MARKET/);
assert.match(landing,/Sin datos de mercado/);
assert.match(landing,/Modelo no calibrado/);
assert.doesNotMatch(landing,/\$[1-9]\d+\.\d\d/);
assert.equal(w.marketTab,'market');
assert.equal(w.marketAsset,'SPY');
assert.equal(w.marketHorizon,'1d');
assert.match(landing,/data-xm-asset="NVDA"/);
assert.match(landing,/data-xm-horizon="20d"/);
assert.match(desktop,/xmarket:\{name:'X MARKET'/);
assert.match(desktop,/if\(w.app==='xmarket'\)body.innerHTML=xMarketHTML\(w\)/);
assert.match(desktop,/marketShock:w.marketShock===true/);
assert.match(controls,/xmarket:'Terminal bursátil y laboratorio Laplace/);
assert.match(html,/href="x-market.css"/);
assert.match(html,/src="x-market.js"/);
press('xmAsset','NVDA');
press('xmHorizon','5d');
assert.equal(w.marketAsset,'NVDA');
assert.equal(w.marketHorizon,'5d');
assert.match(render(),/NVIDIA/);
assert.match(render(),/5 días/);
press('xmTab','laplace');
assert.match(render(),/12 620/);
assert.match(render(),/Sistema cerrado/);
assert.equal(w.marketShock,false);
press('xmShock','on');
const unexpected=render();
assert.equal(w.marketShock,true);
assert.match(unexpected,/12 870/);
assert.match(unexpected,/\+250/);
assert.match(unexpected,/SINTÉTICO/);
assert.match(unexpected,/Esto no es una señal para operar en bolsa/);
assert.match(unexpected,/xm-graph-shock/);
press('xmShock','off');
assert.doesNotMatch(render(),/class="xm-graph-shock"/);
press('xmTab','research');
assert.match(render(),/Primera prueba de la hipótesis/);
assert.match(render(),/no ejecuta XI/);
press('xmTab','about');
assert.match(render(),/Operaciones reales/);
assert.match(render(),/Deshabilitadas/);
assert.match(render(),/Ejecución local XI desde esta ventana/);
press('xmTab','untrusted');
assert.equal(w.marketTab,'market');
press('xmAsset','../../');
assert.equal(w.marketAsset,'SPY');
press('xmHorizon','all');
assert.equal(w.marketHorizon,'1d');
assert.equal(redraws,10);
assert.equal(saves,10);
console.log(JSON.stringify({test:'x_market_workbench_ui',passed:true,sections:4,actions:redraws,synthetic_only:true,legacy_module_retained:true}));
