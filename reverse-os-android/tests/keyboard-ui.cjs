'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require('playwright'),nativeHost=require('./native-host-fixture.cjs');
const root=path.resolve(__dirname,'../..'),[runtime,buildArg,probeArg,outArg]=process.argv.slice(2),out=path.resolve(outArg);
fs.mkdirSync(out,{recursive:true});
let browser,checks=0,holdReply=false,releaseReply,replyHeld=false;const errors=[],cases=[];
function check(value,message){assert.ok(value,message);checks++;}
(async()=>{
 const host=nativeHost({root,runtime,build:path.resolve(buildArg),probe:path.resolve(probeArg),directory:path.join(out,'native')});
 browser=await chromium.launch({headless:true,executablePath:process.env.REVERSE_BROWSER,args:['--no-sandbox','--no-zygote','--single-process','--disable-gpu','--disable-software-rasterizer']});
 const page=await browser.newPage({viewport:{width:844,height:390},locale:'es-ES'});page.setDefaultTimeout(15000);page.on('pageerror',e=>errors.push(e.message));
 await page.exposeFunction('__reverseNative',async(operation,payload)=>{
   const result=await host.request(operation,payload);
   if(operation==='askApp'&&holdReply){holdReply=false;replyHeld=true;await new Promise(resolve=>releaseReply=resolve);}
   return result;
 });
 await page.addInitScript(()=>{window.dismissCount=0;window.ReverseNative={loadProfile:()=>JSON.stringify({ok:true,profile:null}),request(id,operation,payload){window.__reverseNative(operation,payload).then(r=>window.ReverseAndroid.complete(id,r));},hideKeyboard(){window.dismissCount++;window.ReverseShell.setInsets({imeVisible:false,imeHeight:0});},exportText(){},exportBase64(){}};});
 await page.goto('file://'+path.join(root,'reverse-os-android/app/src/main/assets/workbench/index.html'));
 await page.waitForFunction(()=>window.ReverseShell?.mode==='desktop'&&!!androidSnapshot);
 const paint=()=>page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
 async function ime(visible,height,viewport=390){await page.evaluate(({visible,height})=>window.ReverseShell.setInsets({imeVisible:visible,imeHeight:height}),{visible,height});await page.setViewportSize({width:844,height:viewport});await paint();}
 async function inside(selector){return page.locator(selector).evaluate(e=>{const r=e.getBoundingClientRect(),hit=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);return r.y>=0&&r.bottom<=innerHeight+1&&!!hit&&(hit===e||e.contains(hit));});}
 await page.locator('[data-task-app="bbyq"]').click();await page.waitForSelector('[data-program-input]');
 const field=page.locator('[data-program-input]');await field.fill('hola');
 await page.evaluate(()=>{window.originalInput=document.querySelector('[data-program-input]');});
 holdReply=true;await page.locator('[data-program-form] [type="submit"]').click();
 for(let i=0;i<100&&!replyHeld;i++)await new Promise(r=>setTimeout(r,20));
 check(replyHeld,'Native reply was held for concurrent typing');
 check(await field.evaluate(e=>e===window.originalInput&&e.isConnected&&!e.disabled),'Input remains connected and editable while the engine works');
 await field.focus();await page.keyboard.insertText('Siguiente línea con á y ñ');
 await field.evaluate(e=>e.setSelectionRange(10,15));
 releaseReply();await page.waitForFunction(()=>[...programSessions.values()].some(p=>!p.busy&&p.snapshot?.history?.length===2));
 check(await field.inputValue()==='Siguiente línea con á y ñ','Typing during the native reply is retained');
 check(await field.evaluate(e=>e===window.originalInput&&document.activeElement===e),'Native reply preserves input identity and focus');
 check(await field.evaluate(e=>e.selectionStart===10&&e.selectionEnd===15),'Native reply preserves the selection');
 check(host.stats().turns===1&&host.stats().runs===0,'Typing never sends a second turn or runs a program');
 const saved=await page.evaluate(()=>({id:activeId,w:getWindow(activeId).w,h:getWindow(activeId).h,x:getWindow(activeId).x,y:getWindow(activeId).y}));
 for(const height of [260,200,160,132]){
   await ime(true,390-height,height);
   check(await inside('[data-program-input]'),'Chat input remains accessible at '+height+'px');
   check(await inside('[data-program-form] [type="submit"]'),'Send remains accessible at '+height+'px');
   check(!(await page.locator('.taskbar').isVisible()),'Typing space is not occupied by the taskbar');
   check(await page.locator('.window.focused [data-keyboard-dismiss]').isVisible(),'Dismiss keyboard is available');
   check(await page.evaluate(()=>document.activeElement===window.originalInput),'Inset changes preserve focus');
   cases.push({width:844,available_height:height,input_accessible:true,send_accessible:true});
 }
 await page.screenshot({path:path.join(out,'chat-keyboard-132.png')});
 await page.locator('.window.focused [data-keyboard-dismiss]').click();await paint();
 check(await page.evaluate(()=>window.dismissCount)===1,'Dismiss button reaches the native host');
 check(await field.inputValue()==='Siguiente línea con á y ñ','Dismiss preserves the draft');
 await ime(false,0);
 check(await page.locator('.taskbar').isVisible(),'Taskbar returns when the keyboard hides');
 check(await page.evaluate(saved=>{const w=getWindow(saved.id);return ['w','h','x','y'].every(k=>w[k]===saved[k]);},saved),'Original window frame is preserved');
 await ime(true,0);
 check(await page.locator('.taskbar').isVisible(),'Floating keyboard does not shrink the desktop');
 check(await page.locator('.window.focused [data-keyboard-dismiss]').isVisible(),'Floating keyboard can be dismissed');
 await page.evaluate(()=>window.ReverseShell.back());
 check(await page.evaluate(()=>window.dismissCount)===2,'Back dismisses the keyboard before leaving the desktop');
 check(await page.evaluate(()=>window.ReverseShell.back())===false,'Back leaves normal PC navigation intact after dismissal');
 await page.evaluate(()=>{const w=getWindow(activeId);window.noteWindow=createWindow('notes').id;});
 const note=page.locator('.window.focused [data-note-editor]');await note.fill('Nota con acento á y eñe ñ');await note.focus();
 await ime(true,230,160);
 check(await inside('.window.focused [data-note-editor]'),'Note editor remains accessible');
 await page.keyboard.insertText(' guardada');
 check(await page.evaluate(()=>fileById(getWindow(activeId).file).content.endsWith(' guardada')),'Note text is actually stored');
 await page.screenshot({path:path.join(out,'note-keyboard-160.png')});await ime(false,0);
 await page.locator('[data-task-app="xi"]').click();const source=page.locator('[data-xi-source]');await source.fill('#!reverse-pc-machine-v1\nprint 7\nhalt\n');await source.focus();
 await ime(true,230,160);
 check(await inside('[data-xi-source]'),'XI source remains accessible');
 await page.keyboard.insertText('# edición ñ');
 check(await page.evaluate(()=>getWindow(activeId).xiSource.includes('edición ñ')),'XI source input is retained');
 await ime(false,0);
 await page.evaluate(()=>{window.dialogResult='pending';ask({title:'Nombre del archivo',value:'Proyecto ñ.txt',confirm:'Guardar'}).then(value=>window.dialogResult=value);});
 await page.locator('#dialog-input').focus();await ime(true,230,160);
 check(await inside('#dialog-input'),'Dialog input is accessible with the keyboard');
 check(await page.locator('#input-dialog [data-keyboard-dismiss]').isVisible(),'Dialog exposes its keyboard dismiss control');
 await page.locator('#input-dialog [data-keyboard-dismiss]').click();await ime(false,0);
 await page.locator('#dialog-confirm').click();
 check(await page.evaluate(()=>window.dialogResult)==='Proyecto ñ.txt','Dialog preserves and confirms its text');
 check(host.stats().turns===1&&host.stats().experiments===0,'Keyboard controls never execute work');
 check(errors.length===0,JSON.stringify(errors));
 const report={checks,inset_cases:cases,native_conversation_turns:host.stats().turns,draft_and_selection_retained:true,window_frame_retained:true,errors,scope:'Chromium with simulated native IME insets and a real XI/RXE conversation. This does not execute an Android IME or prove device-specific keyboard behavior.'};
 fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log('KEYBOARD_UI_PASS',checks);
 await browser.close();
})().catch(async e=>{console.error(e.stack);console.error('PAGE_ERRORS',errors);if(browser)await browser.close();process.exitCode=1;});
