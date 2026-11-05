import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

test('a saved ARM64 tab renders after its renderer is registered, without executing a guest',()=>{
  const renders=[];
  const context={
    state:{windows:[{id:'saved-xi',app:'xi',xiTab:'arm64'},{id:'files',app:'files'}]},
    xiHTML:()=>'<p>Legacy editor</p>',
    guestHTML:()=>'<p>Saved ARM64 guest</p>',
    document:{addEventListener(){}},
    nativeRequest(){throw Error('Restoration must not execute the guest');},
  };
  context.renderWindowContent=w=>renders.push({id:w.id,html:context.xiHTML(w)});
  vm.createContext(context);
  vm.runInContext(readFileSync(new URL('../reverse-os-android/app/src/main/assets/workbench/quantum-workbench.js',import.meta.url),'utf8'),context);
  assert.equal(renders.length,1);
  assert.equal(renders[0].id,'saved-xi');
  assert.match(renders[0].html,/Saved ARM64 guest/);
  assert.doesNotMatch(renders[0].html,/Legacy editor/);
  assert.match(renders[0].html,/data-quantum-tab="arm64"[^>]*aria-current="page"/);
});
