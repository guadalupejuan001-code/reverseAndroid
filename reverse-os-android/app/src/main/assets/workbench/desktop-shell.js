'use strict';
// The native parent owns IME padding; web code fits the existing PC windows.
let shellImeVisible=false,shellInsetFrame;
function fitDesktopToInput(){
  cancelAnimationFrame(shellInsetFrame);
  shellInsetFrame=requestAnimationFrame(()=>{
    for(const w of state.windows)geometry(w,document.getElementById(w.id));
    const field=document.activeElement;
    if(shellImeVisible&&field?.matches('input,textarea,[contenteditable="true"]'))field.scrollIntoView({block:'nearest',inline:'nearest'});
  });
}
function dismissDesktopKeyboard(){
  if(typeof window.ReverseNative?.hideKeyboard==='function')window.ReverseNative.hideKeyboard();
  else document.activeElement?.blur();
}
window.ReverseShell={
  mode:'desktop',
  setInsets(insets){
    const root=document.documentElement;
    for(const side of ['top','bottom','left','right'])root.style.setProperty('--native-safe-'+side,Math.max(0,Number(insets[side])||0)+'px');
    shellImeVisible=!!insets.imeVisible;
    document.body.dataset.imeVisible=String(shellImeVisible);
    document.body.dataset.keyboard=String(shellImeVisible&&Number(insets.imeHeight)>0);
    fitDesktopToInput();
  },
  back(){
    if(shellImeVisible){dismissDesktopKeyboard();return true;}
    if($('#input-dialog').open){resolveDialog(null);return true;}
    if(!$('#context-menu').hidden){hideContext();return true;}
    if(!$('#launcher').hidden){hideLauncher();return true;}
    return false;
  }
};
document.addEventListener('pointerdown',e=>{if(e.target.closest('[data-keyboard-dismiss]'))e.preventDefault();});
document.addEventListener('click',e=>{if(e.target.closest('[data-keyboard-dismiss]'))dismissDesktopKeyboard();});
document.addEventListener('keydown',e=>{if(e.key==='Escape'&&shellImeVisible){e.preventDefault();dismissDesktopKeyboard();}});
document.addEventListener('focusin',()=>{if(shellImeVisible)fitDesktopToInput();});
window.addEventListener('resize',fitDesktopToInput);
