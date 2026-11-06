import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';

const runtime=process.env.REVERSE_ANDROID_RUNTIME_ROOT;
let module=new URL('../tools/reverseandroid-app-process-probe.mjs',import.meta.url).href;
if(runtime) {
  const source=readFileSync(new URL(module),'utf8').replace(/from (['"])(\.\/[^'"]+)\1/g,
    (_,q,spec)=>'from '+q+pathToFileURL(resolve(runtime,'tools',spec)).href+q);
  module='data:text/javascript;base64,'+Buffer.from(source).toString('base64');
}
const {isSettingsProcess}=await import(module);
const row={identity_verified:true,exit_state:0,leader_verified:true,exe_path:'/system/bin/app_process64',
  argv:['com.android.settings'],pid:7000,parent_pid:5629,credentials:{uid:'1000',euid:'1000'}};

test('accepts an actual live Settings identity independent of truncated comm',()=>{
  assert.equal(isSettingsProcess({...row,comm:'ndroid.settings'}),true);
});
test('process name alone cannot authenticate a real Android app',()=>{
  for(const change of [{identity_verified:false},{leader_verified:false},{exit_state:16},
    {argv:['zygote64']},{argv:['com.android.settings','fake']},{exe_path:'/system/bin/sh'},
    {credentials:{uid:'0',euid:'0'}},{credentials:{uid:'1000',euid:'0'}},{parent_pid:1},{pid:1}]) {
    assert.equal(isSettingsProcess({...row,comm:'com.android.settings',...change}),false);
  }
});
