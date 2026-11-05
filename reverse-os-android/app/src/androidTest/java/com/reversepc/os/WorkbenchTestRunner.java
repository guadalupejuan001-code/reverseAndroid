package com.reversepc.os;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.webkit.WebView;
import org.json.JSONArray;
import org.json.JSONObject;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

/** Framework-only instrumentation: actual APK, WebView, JNI and saved RVPC2. */
public final class WorkbenchTestRunner extends Instrumentation {
    private Bundle arguments;
    private volatile WorkbenchActivity activity;
    private volatile Activity resumed,pausedActivity;
    @Override public void callActivityOnResume(Activity current){super.callActivityOnResume(current);pausedActivity=null;resumed=current;if(current instanceof WorkbenchActivity)activity=(WorkbenchActivity)current;}
    @Override public void callActivityOnPause(Activity current){super.callActivityOnPause(current);pausedActivity=current;}
    @Override public void onCreate(Bundle args){arguments=args;start();}
    private static void require(boolean ok,String message){if(!ok)throw new AssertionError(message);}
    private String js(String script)throws Exception{
        CountDownLatch done=new CountDownLatch(1);AtomicReference<String> result=new AtomicReference<>();
        runOnMainSync(()->activity.testingWebView().evaluateJavascript(script,value->{result.set(value);done.countDown();}));
        require(done.await(30,TimeUnit.SECONDS),"WebView callback timed out: "+script.substring(0,Math.min(script.length(),160)));
        Object value=new JSONArray("["+result.get()+"]").get(0);return value==JSONObject.NULL?"null":String.valueOf(value);
    }
    private String waitJs(String expression)throws Exception{
        for(int i=0;i<120;i++){String result=js(expression);if(!result.equals("null")&&!result.equals("undefined")&&!result.equals("false"))return result;Thread.sleep(250);}
        throw new AssertionError("Timed out: "+expression);
    }
    private void waitDesktopReady()throws Exception{
        WorkbenchActivity observed=null;int stable=0;
        for(int i=0;i<200;i++){
            WorkbenchActivity current=activity;
            if(current==observed&&current==resumed&&!current.isDestroyed()&&current.hasWindowFocus())stable++;
            else{observed=current;stable=0;}
            if(stable>=10&&js("document.readyState==='complete'&&innerWidth>innerHeight&&!!document.querySelector('[data-task-app]')&&typeof nativeRequest==='function'").equals("true"))return;
            Thread.sleep(100);
        }
        throw new AssertionError("Desktop did not settle after orientation/activity startup");
    }
    private JSONObject startupSnapshot()throws Exception{
        // The initial page can finish before a later orientation navigation.
        // Only this read-only inspection may be resubmitted after that navigation;
        // state-changing op() calls must never replay an uncertain transaction.
        for(int navigation=0;navigation<4;navigation++){
            waitDesktopReady();
            // A navigation can occur after waitDesktopReady. Capture the
            // document and submit its read-only request in one JavaScript turn.
            String document=js("(()=>{if(document.readyState!=='complete'||typeof nativeRequest!=='function')return null;"+
                "window.testSnapshot=null;const origin=String(performance.timeOrigin);"+
                "nativeRequest('inspect').then(r=>window.testSnapshot=r).catch(e=>window.testSnapshot={ok:false,error:String(e)});return origin;})()");
            if(document.equals("null"))continue;
            boolean changed=false;
            for(int i=0;i<120;i++){
                JSONObject probe=new JSONObject(js("JSON.stringify({document:String(performance.timeOrigin),snapshot:window.testSnapshot||null})"));
                if(!document.equals(probe.getString("document"))){
                    android.util.Log.i("ReverseWorkbenchTest","Startup inspection crossed a WebView navigation; inspecting the current document");
                    changed=true;break;
                }
                if(!probe.isNull("snapshot"))return probe.getJSONObject("snapshot");
                Thread.sleep(250);
            }
            if(!changed)throw new AssertionError("Native startup inspection did not answer in a stable document");
        }
        throw new AssertionError("Desktop kept navigating during startup inspection");
    }
    private void openGuestView()throws Exception{
        String document=null;int stable=0;
        for(int i=0;i<300;i++){
            String ready=js("(()=>{if(document.readyState!=='complete'||typeof createWindow!=='function'||typeof guestUi==='undefined'||typeof state==='undefined')return null;"+
                "if(!document.querySelector('[data-guest=inspect]')||!state.windows.some(w=>w.app==='xi'&&w.xiTab==='arm64'&&!w.min&&w.space===state.space))createWindow('guest');"+
                "return guestUi.snapshot&&!guestUi.busy?String(performance.timeOrigin):null;})()");
            if(!ready.equals("null")){if(ready.equals(document))stable++;else{document=ready;stable=1;}}
            else{document=null;stable=0;}
            if(stable>=15)return;
            Thread.sleep(100);
        }
        throw new AssertionError("Guest view did not settle across initial WebView navigation: "+js(
            "JSON.stringify({ready:document.readyState,url:location.href,guest:typeof guestUi==='undefined'?null:guestUi,windows:typeof state==='undefined'?null:state.windows})"));
    }
    private JSONObject op(String op,String payload,String target)throws Exception{
        js("window."+target+"=null;nativeRequest("+JSONObject.quote(op)+","+JSONObject.quote(payload)+").then(r=>window."+target+"=r).catch(e=>window."+target+"={ok:false,error:String(e)});true");
        return new JSONObject(waitJs("window."+target+"?JSON.stringify(window."+target+"):null"));
    }
    private File dataFile(String path){return new File(getTargetContext().getFilesDir(),path);}
    private static String digest(byte[] data)throws Exception{
        StringBuilder out=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(data))out.append(String.format(java.util.Locale.ROOT,"%02x",b&255));return out.toString();
    }
    private String hash(String path)throws Exception{return digest(Files.readAllBytes(dataFile(path).toPath()));}
    private void tapDom(String selector)throws Exception{
        String quoted=JSONObject.quote(selector);
        js("document.querySelector("+quoted+").scrollIntoView({block:'nearest',inline:'nearest'});true");
        Thread.sleep(150); // A scroll or resize must reach the compositor before touch.
        JSONObject point=new JSONObject(js("(()=>{const r=document.querySelector("+quoted+").getBoundingClientRect();return JSON.stringify({x:r.x+r.width/2,y:r.y+r.height/2,width:innerWidth});})()"));
        require(js("(()=>{const e=document.querySelector("+quoted+"),r=e.getBoundingClientRect(),hit=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);return !!hit&&(e===hit||e.contains(hit));})()").equals("true"),"Touch target is covered or outside the viewport: "+selector);
        int[] origin=new int[2];int[] width={0};runOnMainSync(()->{activity.testingWebView().getLocationOnScreen(origin);width[0]=activity.testingWebView().getWidth();});
        float scale=(float)(width[0]/point.getDouble("width")),x=origin[0]+(float)point.getDouble("x")*scale,y=origin[1]+(float)point.getDouble("y")*scale;
        long time=SystemClock.uptimeMillis();MotionEvent down=MotionEvent.obtain(time,time,MotionEvent.ACTION_DOWN,x,y,0),up=MotionEvent.obtain(time,time+80,MotionEvent.ACTION_UP,x,y,0);
        try{sendPointerSync(down);sendPointerSync(up);}finally{down.recycle();up.recycle();}Thread.sleep(100);
    }
    private void sendMessage(String question,String answer)throws Exception{
        int before=Integer.parseInt(js("programSessions.get(state.windows.find(w=>w.rxePath==='Aplicaciones/BBY-Q.rxe').id).snapshot.history.length"));
        js("(()=>{const field=document.querySelector('[data-program-input]');field.value="+JSONObject.quote(question)+";field.dispatchEvent(new Event('input',{bubbles:true}));})()");
        tapDom(".rxe-composer button[type=submit]");
        waitJs("(()=>{const w=state.windows.find(w=>w.rxePath==='Aplicaciones/BBY-Q.rxe'),d=programSessions.get(w.id);return d.snapshot.history.length==="+(before+2)+"&&!d.busy;})()");
        require(js("document.querySelector('[aria-label=\"Respuesta BBY Q\"]').textContent").equals(answer),"Unexpected real BBY Q answer: "+question);
    }
    private void seedUpgrade()throws Exception{
        byte[] image;try(java.io.InputStream input=getTargetContext().getAssets().open("native-apps/BBY-Q.rxe")){image=WorkbenchProgramsRead(input);}
        final byte[] executable=image;
        RuntimeExecutor.INSTANCE.submit(()->{try{
            WorkspaceFiles files=new WorkspaceFiles(getTargetContext().getFilesDir());files.write("Aplicaciones/BBY-Q.rxe",executable,false);
            RxeAppStore app=new RxeAppStore(getTargetContext(),executable);
            app.ask(executable,"hola",1);app.ask(executable,"ana tiene dos manzanas",1);app.ask(executable,"ella recibe tres mas",1);
            return true;
        }catch(Exception error){throw new RuntimeException(error);}}).get(60,TimeUnit.SECONDS);
        String identity=digest(image);JSONObject proof=new JSONObject().put("identity",identity).put("record_sha256",hash("rxe-apps/"+identity+"/session.rxas")).put("desktop_sha256",hash("session.rvp"));
        Files.write(dataFile("upgrade-before.json").toPath(),proof.toString().getBytes(StandardCharsets.UTF_8));
    }
    // This fixture also runs on the previous 2.0 APK, which has no WorkbenchPrograms.
    private static byte[] WorkbenchProgramsRead(java.io.InputStream input)throws Exception{
        java.io.ByteArrayOutputStream output=new java.io.ByteArrayOutputStream();byte[] part=new byte[8192];int n;while((n=input.read(part))!=-1)output.write(part,0,n);return output.toByteArray();
    }
    private void retainedProof(boolean write)throws Exception{
        String identity=js("programSessions.get(state.windows.find(w=>w.rxePath==='Aplicaciones/BBY-Q.rxe').id).snapshot.identity");
        String record="rxe-apps/"+identity+"/session.rxas";
        if(write){JSONObject proof=new JSONObject().put("record_sha256",hash(record)).put("desktop_sha256",hash("session.rvp"));Files.write(dataFile("retained-app.json").toPath(),proof.toString().getBytes(StandardCharsets.UTF_8));}
        else{JSONObject proof=new JSONObject(new String(Files.readAllBytes(dataFile("retained-app.json").toPath()),StandardCharsets.UTF_8));require(hash(record).equals(proof.getString("record_sha256")),"Opening after restart executed or changed BBY Q");require(hash("session.rvp").equals(proof.getString("desktop_sha256")),"Restart executed an RXE again");}
    }
    private void fullDisplay()throws Exception{
        int[] bounds=new int[4];boolean[] bars={true};
        int displayWidth=0,displayHeight=0;
        for(int attempt=0;attempt<30;attempt++){
            runOnMainSync(()->{
                WebView view=activity.testingWebView();int[] point=new int[2];view.getLocationOnScreen(point);
                bounds[0]=point[0];bounds[1]=point[1];bounds[2]=view.getWidth();bounds[3]=view.getHeight();
                android.view.WindowInsets insets=view.getRootWindowInsets();bars[0]=insets==null||insets.isVisible(android.view.WindowInsets.Type.statusBars())||insets.isVisible(android.view.WindowInsets.Type.navigationBars());
            });
            Bitmap screen=getUiAutomation().takeScreenshot();displayWidth=screen.getWidth();displayHeight=screen.getHeight();screen.recycle();
            if(!bars[0]&&bounds[0]==0&&bounds[1]==0&&bounds[2]==displayWidth&&bounds[3]==displayHeight)return;
            Thread.sleep(100);
        }
        throw new AssertionError("Display has reserved borders: "+Arrays.toString(bounds)+" display="+displayWidth+"x"+displayHeight+" bars="+bars[0]);
    }
    private void appearanceWorkflow()throws Exception{
        fullDisplay();
        require(js("!document.querySelector('#pinned-apps [data-task-app=wallpaper]')&&!document.querySelector('#running-apps [data-task-app=wallpaper]')").equals("true"),"Wallpaper editor is in the taskbar");
        js("createWindow('settings');true");tapDom(".window[aria-label=Ajustes] [data-open=wallpaper]");
        require(js("!!document.querySelector('.window[aria-label=\"Editor de fondo\"]')&&!document.querySelector('.taskbar [data-task-app=wallpaper]')").equals("true"),"Wallpaper editor is not contained in Settings");
        tapDom(".window[aria-label=\"Editor de fondo\"] [data-pref-toggle=motion]");
        require(js("state.prefs.motion").equals("false"),"Motion toggle did not pause");
        String stopped=js("getComputedStyle(document.querySelector('#wall-image')).transform");Thread.sleep(500);
        require(js("getComputedStyle(document.querySelector('#wall-image')).transform").equals(stopped),"Paused wallpaper still moves");
        tapDom(".window[aria-label=\"Editor de fondo\"] [data-pref-toggle=motion]");
        waitJs("document.querySelector('#wall-image').getAnimations().some(a=>a.playState==='running')");
        js("for(const w of [...state.windows])if(w.app==='settings'||w.app==='wallpaper')closeWindow(w);true");
        tapDom("#show-desktop");Thread.sleep(500);
        String before=js("getComputedStyle(document.querySelector('#wall-image')).transform");
        Bitmap first=getUiAutomation().takeScreenshot();Thread.sleep(1800);Bitmap second=getUiAutomation().takeScreenshot();
        require(!js("getComputedStyle(document.querySelector('#wall-image')).transform").equals(before),"Animated wallpaper transform did not advance");
        int changed=0;
        for(int y=16;y<first.getHeight()-130;y+=4)for(int x=16;x<first.getWidth()-16;x+=4){
            int a=first.getPixel(x,y),b=second.getPixel(x,y);
            if(Math.abs(((a>>16)&255)-((b>>16)&255))+Math.abs(((a>>8)&255)-((b>>8)&255))+Math.abs((a&255)-(b&255))>8)changed++;
        }
        File folder=new File(getTargetContext().getExternalFilesDir(null),"visual-evidence");folder.mkdirs();
        try(FileOutputStream output=new FileOutputStream(new File(folder,"wall-motion-before.png"))){first.compress(Bitmap.CompressFormat.PNG,100,output);}
        try(FileOutputStream output=new FileOutputStream(new File(folder,"wall-motion-after.png"))){second.compress(Bitmap.CompressFormat.PNG,100,output);}
        first.recycle();second.recycle();require(changed>30,"Wallpaper pixels did not move: "+changed);
        tapDom("#show-desktop");fullDisplay();
        js("window.appearanceSaved=null;persist().then(ok=>window.appearanceSaved=ok);true");require(waitJs("window.appearanceSaved").equals("true"),"Appearance was not saved");
    }
    private void programNavigation(boolean upgrade)throws Exception{
        ActivityMonitor oldDesktop=addMonitor(DesktopActivity.class.getName(),null,true);
        byte[] desktopBefore=Files.readAllBytes(dataFile("session.rvp").toPath());
        tapDom("#launcher-toggle");fullDisplay();tapDom("#launcher [data-open=programs]");
        waitJs("document.querySelector('.window[aria-label=\"Aplicaciones .rxe\"] [data-program-path=\"Aplicaciones/BBY-Q.rxe\"]')");
        tapDom(".program-list [data-program-path=\"Aplicaciones/BBY-Q.rxe\"]");waitJs("document.querySelector('[data-program-input]:not(:disabled)')");
        require(resumed instanceof WorkbenchActivity&&oldDesktop.getHits()==0,"Applications left Workbench for the old desktop");
        require(Arrays.equals(desktopBefore,Files.readAllBytes(dataFile("session.rvp").toPath())),"Opening BBY Q changed the shared machine");
        if(upgrade){
            JSONObject proof=new JSONObject(new String(Files.readAllBytes(dataFile("upgrade-before.json").toPath()),StandardCharsets.UTF_8));
            require(hash("rxe-apps/"+proof.getString("identity")+"/session.rxas").equals(proof.getString("record_sha256")),"Upgrade replaced the retained agent");
            require(js("document.querySelector('[aria-label=\"Respuesta BBY Q\"]').textContent").equals("Ana tiene cinco manzanas."),"Old conversation not restored in the new window");
        }else{sendMessage("hola","Hola. Soy BBY Q.");sendMessage("ana tiene dos manzanas","Ana tiene dos manzanas.");sendMessage("ella recibe tres mas","Ana tiene cinco manzanas.");}
        sendMessage("cuantas manzanas tiene ana?","Ana tiene cinco manzanas.");
        tapDom("[data-program-language=\"2\"]");sendMessage("how many apples does she have?","Ana has five apples.");
        require(Arrays.equals(desktopBefore,Files.readAllBytes(dataFile("session.rvp").toPath())),"BBY Q wrote the shared desktop checkpoint");
        require(js("document.querySelector('.task-tray').getBoundingClientRect().right<=innerWidth+1").equals("true"),"Taskbar controls overflow the display");
        visualCheck(upgrade?"upgraded-bbyq":"bbyq-in-workbench");
        // All catalog entry points, including a restored 2.0 shortcut, stay here.
        js("createWindow('control',{tab:'apps'});true");tapDom(".control-app-grid [data-open=programs]");
        js("createWindow('control',{tab:'system'});true");waitJs("document.querySelector('.wall-editor-actions [data-open=programs]')");tapDom(".wall-editor-actions [data-open=programs]");
        js("createWindow('nativeDesktop');window.ReverseNative.openNativeDesktop();true");Thread.sleep(100);
        require(resumed instanceof WorkbenchActivity&&oldDesktop.getHits()==0,"A catalog entry point launched DesktopActivity");
        tapDom(".program-list [data-program-path=\"Aplicaciones/Hola-REVERSE.rxe\"]");
        waitJs("document.querySelector('.window[aria-label=\"Hola REVERSE\"] [data-program-action=run]')");
        tapDom(".window[aria-label=\"Hola REVERSE\"] [data-program-action=run]");waitJs("document.querySelector('.rxe-output').textContent.includes('42')");
        // Compile through XI Studio: output must not be executed until Run.
        js("createWindow('xi');true");
        String source="#!reverse-pc-machine-v1\nset r0 7\nmul r0 9\nstore8 314 r0\nprint r0\nhalt\n";
        js("(()=>{const field=document.querySelector('[data-xi-source]');field.value="+JSONObject.quote(source)+";field.dispatchEvent(new Event('input',{bubbles:true}));})()");
        String beforeCompile=hash("session.rvp");tapDom("[data-program-action=compile]");waitJs("document.querySelector('#input-dialog').open");
        js("document.querySelector('#dialog-input').value='Prueba-workbench.rxe';true");tapDom("#dialog-confirm");
        waitJs("document.querySelector('.window[aria-label=\"Prueba-workbench\"] [data-program-action=run]')");
        require(hash("session.rvp").equals(beforeCompile),"Compilation executed the source");
        tapDom(".window[aria-label=\"Prueba-workbench\"] [data-program-action=run]");waitJs("document.querySelector('.window[aria-label=\"Prueba-workbench\"] .rxe-output').textContent.includes('63')");
        require(!hash("session.rvp").equals(beforeCompile),"RXE did not update the real machine");
        String beforeSearch=hash("session.rvp");tapDom("#launcher-toggle");
        js("(()=>{const field=document.querySelector('#global-search');field.value='Prueba-workbench';field.dispatchEvent(new Event('input',{bubbles:true}));})()");
        tapDom("#launcher [data-program-path=\"Aplicaciones/Prueba-workbench.rxe\"]");
        require(hash("session.rvp").equals(beforeSearch),"Searching reopened and executed the program");
        tapDom("#pinned-apps [data-task-app=bbyq]");
        require(js("getWindow(activeId).rxePath").equals("Aplicaciones/BBY-Q.rxe"),"BBY Q taskbar shortcut opened the wrong window");

        // X MARKET is a built-in Workbench window, not an RXE masquerading as a market feed.
        String beforeMarket=hash("session.rvp");
        tapDom("#launcher-toggle");
        tapDom("#launcher [data-open=xmarket]");
        waitJs("document.querySelector('.window[aria-label=\"X MARKET\"] .xm-shell')");
        require(js("getWindow(activeId).app").equals("xmarket"),"Start opened the wrong X MARKET application");
        require(js("document.querySelector('.xm-no-data').textContent.includes('Sin datos de mercado')").equals("true"),"X MARKET fabricated a quote");
        tapDom(".window[aria-label=\"X MARKET\"] [data-xm-tab=laplace]");
        require(js("document.querySelector('.xm-lab-results').textContent.includes('12 620')").equals("true"),"Missing closed-world XI demonstration");
        tapDom(".window[aria-label=\"X MARKET\"] [data-xm-shock=on]");
        require(js("document.querySelector('.xm-lab-results').textContent.includes('+250')").equals("true"),"The unknown-event illustration did not update");
        require(js("document.querySelector('.xm-lab-graph').textContent.includes('SINTÉTICO')").equals("true"),"Missing synthetic-data label");
        visualCheck("x-market-in-workbench");
        require(hash("session.rvp").equals(beforeMarket),"Viewing X MARKET mutated XI/Ether state");
        tapDom("#pinned-apps [data-task-app=xmarket]");
        require(js("getWindow(state.windows.find(x=>x.app==='xmarket').id).min").equals("true"),"X MARKET pin did not minimize its window");
        tapDom("#pinned-apps [data-task-app=xmarket]");
        require(js("getWindow(activeId).app").equals("xmarket"),"X MARKET pinned shortcut failed to restore");
        require(resumed instanceof WorkbenchActivity&&oldDesktop.getHits()==0,"X MARKET escaped REVERSE Workbench");

        RuntimeExecutor.INSTANCE.submit(()->{try{
            WorkbenchPrograms programs=new WorkbenchPrograms(getTargetContext());WorkspaceFiles files=new WorkspaceFiles(getTargetContext().getFilesDir());
            JSONObject opened=programs.open("Aplicaciones/BBY-Q.rxe"),request=new JSONObject().put("path",opened.getString("path")).put("identity",opened.getString("identity"));
            byte[] image=files.read(opened.getString("path"));require(Arrays.equals(programs.exportImage(request,false),image),"Executable export changed bytes");
            require(Arrays.equals(programs.exportImage(request,true),new RxeAppStore(getTargetContext(),image).load(image).state),"Memory export is not the real RVPC2");
            JSONObject copy=programs.importImage(image);require(programs.open(copy.getString("path")).getJSONArray("history").toString().equals(opened.getJSONArray("history").toString()),"Imported duplicate lost the same retained memory");
            try{programs.importImage(source.getBytes(StandardCharsets.UTF_8));throw new AssertionError("Renamed source accepted as RXE");}catch(RuntimeException|java.io.IOException rejected){}
            return true;
        }catch(Exception error){throw new RuntimeException(error);}}).get(60,TimeUnit.SECONDS);
        // Keep the agent visible for cold restart, with no inference on restoration.
        js("for(const w of [...state.windows])if(w.app!=='programs'&&w.rxePath!=='Aplicaciones/BBY-Q.rxe')closeWindow(w);createWindow('bbyq');true");
        require(resumed instanceof WorkbenchActivity&&oldDesktop.getHits()==0,"Program operation left Workbench");removeMonitor(oldDesktop);
        js("window.navigationSaved=null;persist().then(ok=>window.navigationSaved=ok);true");require(waitJs("window.navigationSaved").equals("true"),"App window was not persisted");retainedProof(true);
    }
    private void visualCheck(String mode)throws Exception{
        CountDownLatch painted=new CountDownLatch(1);
        runOnMainSync(()->activity.testingWebView().postVisualStateCallback(1,new WebView.VisualStateCallback(){@Override public void onComplete(long id){painted.countDown();}}));
        require(painted.await(30,TimeUnit.SECONDS),"WebView did not commit a visual frame");
        File folder=new File(getTargetContext().getExternalFilesDir(null),"visual-evidence");folder.mkdirs();
        String geometry=js("JSON.stringify({width:innerWidth,height:innerHeight,body:document.body.getBoundingClientRect().toJSON(),desktop:document.querySelector('#desktop').getBoundingClientRect().toJSON(),taskbar:document.querySelector('.taskbar').getBoundingClientRect().toJSON(),wall:getComputedStyle(document.querySelector('#wall-image')).backgroundImage})");
        try(FileOutputStream output=new FileOutputStream(new File(folder,mode+"-geometry.json"))){output.write(geometry.getBytes(java.nio.charset.StandardCharsets.UTF_8));}
        boolean visible=false;
        for(int attempt=0;attempt<24;attempt++){
            Bitmap screen=getUiAutomation().takeScreenshot();require(screen!=null,"No display screenshot");
            int bright=0,samples=0;
            for(int y=10;y<screen.getHeight()-10;y+=4)for(int x=10;x<screen.getWidth()*9/10;x+=4){int color=screen.getPixel(x,y);if(((color>>16)&255)>85||((color>>8)&255)>85||(color&255)>85)bright++;samples++;}
            try(FileOutputStream output=new FileOutputStream(new File(folder,mode+"-screen.png"))){screen.compress(Bitmap.CompressFormat.PNG,100,output);}
            screen.recycle();visible=bright>samples/250;if(visible)break;Thread.sleep(500);
        }
        runOnMainSync(()->{WebView view=activity.testingWebView();Bitmap bitmap=Bitmap.createBitmap(Math.max(1,view.getWidth()),Math.max(1,view.getHeight()),Bitmap.Config.ARGB_8888);view.draw(new Canvas(bitmap));try(FileOutputStream output=new FileOutputStream(new File(folder,mode+"-view.png"))){bitmap.compress(Bitmap.CompressFormat.PNG,100,output);}catch(Exception error){throw new RuntimeException(error);}finally{bitmap.recycle();}});
        require(visible,"Display stayed blank after committed WebView frame; "+geometry);
    }
    private void keyboardWorkflow()throws Exception{
        String frame=js("JSON.stringify({w:getWindow(activeId).w,h:getWindow(activeId).h,x:getWindow(activeId).x,y:getWindow(activeId).y})");
        tapDom("[data-program-input]");
        runOnMainSync(()->((InputMethodManager)activity.getSystemService(android.content.Context.INPUT_METHOD_SERVICE)).showSoftInput(activity.testingWebView(),InputMethodManager.SHOW_IMPLICIT));
        waitJs("document.body.dataset.imeVisible==='true'");
        EditorInfo info=new EditorInfo();AtomicReference<InputConnection> input=new AtomicReference<>();
        runOnMainSync(()->input.set(activity.testingWebView().onCreateInputConnection(info)));
        require(input.get()!=null,"No live Android input connection");
        require((info.imeOptions&EditorInfo.IME_FLAG_NO_FULLSCREEN)!=0,"Fullscreen IME is not disabled");
        CountDownLatch edited=new CountDownLatch(1);
        Runnable edit=()->{try{input.get().commitText("Prueba á ñ",1);}finally{edited.countDown();}};
        android.os.Handler handler=input.get().getHandler();if(handler!=null)handler.post(edit);else runOnMainSync(edit);
        require(edited.await(10,TimeUnit.SECONDS),"Input connection edit timed out");
        waitJs("document.querySelector('[data-program-input]').value.includes('Prueba á ñ')");
        require(js("getWindow(activeId).rxeDraft.includes('Prueba á ñ')").equals("true"),"Native input did not reach the stored draft");
        waitJs("(()=>{const r=document.querySelector('[data-program-input]').getBoundingClientRect();return r.top>=0&&r.bottom<=innerHeight;})()");
        require(js("!window.visualViewport||Math.abs(visualViewport.height-innerHeight)<2").equals("true"),"IME space was subtracted twice");
        tapDom(".window.focused [data-keyboard-dismiss]");
        waitJs("document.body.dataset.imeVisible==='false'");
        require(js("document.querySelector('[data-program-input]').value.includes('Prueba á ñ')").equals("true"),"Dismiss lost the draft");
        require(js("JSON.stringify({w:getWindow(activeId).w,h:getWindow(activeId).h,x:getWindow(activeId).x,y:getWindow(activeId).y})").equals(frame),"Keyboard changed the saved frame");
    }
    private void counterWorkflow(boolean restore,boolean paced)throws Exception{
        openGuestView();
        if(!restore){
            RuntimeExecutor.INSTANCE.submit(()->{try{return new GuestStore(getTargetContext()).createSystem(null,paced?6:5);}catch(Exception error){throw new RuntimeException(error);}}).get(60,TimeUnit.SECONDS);
            JSONObject run=op("guestRun","{\"steps\":1000}","counterRan");
            require(run.getString("stdout").equals("REVERSE: COUNTER LATENCY OK\n"),"Real counter sampling program did not execute");
        }
        JSONObject report=op("guestInspect","","counterState"),guest=report.getJSONObject("guest");
        require(guest.getBoolean("halted")&&guest.getLong("counter_samples")==64&&guest.getString("counter_latency").equals("host-seeded-hmac-v1"),"Counter hardware state did not persist");
        if(paced)require(guest.getLong("counter_instructions_per_tick")==64,"Counter instruction cadence did not persist");
        String proofName=paced?"counter-rate-proof.json":"counter-proof.json";
        String sha=hash("guest-aarch64.rvg");
        if(!restore){
            require(sha.equals(arguments.getString("expected_counter_sha256")),"Android counter state differs from the independent native result");
            Files.write(dataFile(proofName).toPath(),new JSONObject().put("state_sha256",sha).put("counter_samples",64).put("instructions_per_tick",paced?64:1).put("test_seed_only",true).put("android_boot",false).toString().getBytes(StandardCharsets.UTF_8));
        }else{
            JSONObject prior=new JSONObject(new String(Files.readAllBytes(dataFile(proofName).toPath()),StandardCharsets.UTF_8));
            require(sha.equals(prior.getString("state_sha256")),"Restart changed the counter seed/cursor or guest state");
        }
        js("guestUi.snapshot=window.counterState;guestUi.frame=null;guestUi.frameNumber=-1;refreshGuestViews();true");
    }
    private void graphicsWorkflow(boolean restore)throws Exception{
        openGuestView();
        JSONObject saved;
        if(!restore){
            RuntimeExecutor.INSTANCE.submit(()->{try{byte[] raw=new byte[32768];raw[0]=0x37;raw[8192]=0x42;JSONObject imported=new GuestStore(getTargetContext()).importDisk(new java.io.ByteArrayInputStream(raw));JSONObject disk=imported.getJSONObject("disk_attached");require(disk.getLong("disk_bytes")==raw.length&&disk.getString("disk_sha256").equals(digest(raw)),"Disk catalog did not retain verified content identity");require(Arrays.equals(Files.readAllBytes(dataFile("guest-disks/"+disk.getString("disk_sha256")+".raw").toPath()),raw),"Sparse disk import changed bytes");return true;}catch(Exception error){throw new RuntimeException(error);}}).get(60,TimeUnit.SECONDS);
            JSONObject created=op("guestGraphics","","graphicsCreated");require(created.getJSONObject("guest").getBoolean("pci_bus"),"Graphics test did not select the PCI board");
            saved=op("guestRun","{\"steps\":1000}","graphicsRan");
            require(saved.getJSONObject("guest").getBoolean("halted")&&saved.getString("stdout").equals("REVERSE: PCI GPU 2D OK\n"),"Actual AArch64 graphics program did not finish");
        }else saved=op("guestInspect","","graphicsRan");
        require(saved.getJSONObject("guest").getInt("display_width")==96&&saved.getJSONObject("guest").getInt("display_height")==64,"Wrong persisted GPU dimensions");
        String before=hash("guest-aarch64.rvg");JSONObject frame=op("guestDisplay","","graphicsFrame");require(frame.getBoolean("display_ready"),"Native display is empty");
        String encoded=frame.getString("frame_png");require(encoded.startsWith("data:image/png;base64,"),"Display was not encoded as a real PNG");
        byte[] png=android.util.Base64.decode(encoded.substring(encoded.indexOf(',')+1),android.util.Base64.DEFAULT);
        Bitmap bitmap=android.graphics.BitmapFactory.decodeByteArray(png,0,png.length);
        require(bitmap!=null&&bitmap.getWidth()==96&&bitmap.getHeight()==64,"PNG does not contain the native dimensions");
        require(bitmap.getPixel(0,0)==0xffff4040&&bitmap.getPixel(95,0)==0xff40ff80&&bitmap.getPixel(0,63)==0xff4080ff&&bitmap.getPixel(95,63)==0xffffffff,"Android display pixels differ from actual guest VRAM");bitmap.recycle();
        require(hash("guest-aarch64.rvg").equals(before),"Displaying pixels mutated the guest checkpoint");
        Files.write(dataFile("graphics-frame.png").toPath(),png);
        if(!restore)Files.write(dataFile("graphics-proof.json").toPath(),new JSONObject().put("state_sha256",before).put("gpu_2d",true).put("cpu_program_instructions",saved.getJSONObject("guest").getLong("instructions")).put("android_boot",false).toString().getBytes(StandardCharsets.UTF_8));
        else require(new JSONObject(new String(Files.readAllBytes(dataFile("graphics-proof.json").toPath()),StandardCharsets.UTF_8)).getString("state_sha256").equals(before),"Graphics did not survive process restart");
        js("guestUi.snapshot=window.graphicsRan;guestUi.frame=window.graphicsFrame.frame_png;guestUi.frameNumber=window.graphicsFrame.guest.display_frame;refreshGuestViews();true");
        waitJs("document.querySelector('img[alt=\"Salida gráfica del invitado\"]')?.complete&&document.querySelector('img[alt=\"Salida gráfica del invitado\"]').naturalWidth===96");
        js("document.querySelector('img[alt=\"Salida gráfica del invitado\"]').scrollIntoView({block:'center'});true");
    }
    private void desktopShellWorkflow(String requested,JSONObject snapshot)throws Exception{
        boolean restore=requested.equals("restore")||requested.equals("boot")||requested.equals("legacy")||requested.equals("upgrade");
        boolean upgrade=requested.equals("upgrade");
        require(js("REVERSE_CAPABILITIES.max_active_qubits").equals("4096"),"Missing verified core profile");
        require(js("document.documentElement.scrollWidth<=innerWidth").equals("true"),"Viewport overflow");
        if(restore){
            require(snapshot.getJSONObject("core").getLong("active_qubits")>=128,"Quantum state not retained");
            require(js("state.prefs.dim").equals("23"),"Preferences not retained");
            require(js("state.files.some(f=>f.name==='Prueba-nativa.txt'&&!f.trashRoot)").equals("true"),"File not retained");
        }else{
            JSONObject arithmetic=op("calculate","2+3*4","shellArithmetic");
            require(arithmetic.getString("value").equals("14"),"Native arithmetic failed");
            JSONObject probe=op("execute",js("XI_PROBE"),"shellProbe");
            require(probe.getJSONObject("core").getLong("active_qubits")==128,"Quantum probe failed");
            js("createFile('text','Prueba-nativa.txt','personal','Texto persistente real');setPref('dim',23,false);true");
        }
        js("createWindow('bbyq');true");waitJs("document.querySelector('[data-program-input]:not(:disabled)')");
        if(requested.equals("full"))keyboardWorkflow();
        if(restore&&!upgrade){retainedProof(false);fullDisplay();visualCheck("native-"+requested);return;}
        if(upgrade)sendMessage("cuántas manzanas tiene ana","Ana tiene cinco manzanas.");
        else{sendMessage("hola","Hola. Soy BBY Q.");sendMessage("ana tiene dos manzanas","Ana tiene dos manzanas.");sendMessage("ella recibe tres mas","Ana tiene cinco manzanas.");}
        JSONObject compiled=op("compileApp",new JSONObject().put("name","AAA-native-test.rxe").put("source","#!reverse-pc-machine-v1\nset r0 11\nmul r0 7\nprint r0\nhalt\n").toString(),"shellCompiled");
        require(compiled.getBoolean("ok"),"Internal fixture compilation failed");
        js("createWindow('bbyq');true");sendMessage("ejecuta programa 1","Acción completada.");
        require(js("document.querySelector('.window.focused .rxe-output').textContent.includes('77')").equals("true"),"W4u did not execute the native program");
        String beforeExperiment=hash("session.rvp");
        JSONObject experiment=op("experiment",js("quantumProgram('ising',{coupling:1,field:1,steps:1})"),"shellExperiment");
        require(experiment.getBoolean("ok")&&experiment.getBoolean("isolated"),"Ising did not execute natively");
        require(experiment.getJSONObject("core").getLong("active_qubits")==2,"Ising qubits mismatch");
        require(hash("session.rvp").equals(beforeExperiment),"Experiment replaced the work state");
        require(js("innerWidth>innerHeight").equals("true"),"APK must use the horizontal PC desktop");
        require(js("document.querySelector('#shell-home,#mobile-navigation')===null").equals("true"),"Mobile home is still present");
        js("createWindow('files');createWindow('programs');true");
        require(js("state.windows.filter(w=>w.space===state.space&&!w.min).length>=2").equals("true"),"Focusing a PC window minimized its peers");
        require(js("document.querySelector('.window.focused').getBoundingClientRect().width<document.querySelector('#desktop').clientWidth").equals("true"),"PC window was forced full width");
        require(js("getComputedStyle(document.querySelector('.window.focused [data-window-action=max]')).display!=='none'").equals("true"),"PC controls are missing");
        tapDom("#show-desktop");
        require(js("state.windows.every(w=>w.space!==state.space||w.min)").equals("true"),"Show desktop did not minimize the windows");
        tapDom("#show-desktop");
        require(js("state.windows.filter(w=>w.space===state.space&&!w.min).length>=2").equals("true"),"Show desktop did not restore the windows");
        js("createWindow('xi');getWindow(activeId).xiTab='hamiltonian';renderWindowContent(getWindow(activeId));true");
        require(js("document.querySelector('.hamiltonian-equation').textContent.includes('Z₀Z₁')").equals("true"),"Hamiltonian surface missing");
        js("createWindow('bbyq');true");
        js("window.shellSaved=null;persist().then(ok=>window.shellSaved=ok);true");
        require(waitJs("window.shellSaved").equals("true"),"Profile not committed");
        retainedProof(true);fullDisplay();visualCheck("native-"+requested);
    }
    private void guestWorkflow(boolean restore)throws Exception{
        String desktopBefore=hash("session.rvp");
        if(restore)require(waitJs("!!document.querySelector('[data-guest=inspect]')").equals("true"),
            "The saved ARM64 tab was not rendered during cold restoration");
        openGuestView();
        if(restore){
            require(js("guestUi.snapshot.guest.instructions").equals("13"),"Guest PC/count did not persist after restart");
            require(js("guestUi.snapshot.guest.delta_overrides").equals("2"),"XI/ARM64 deltas did not persist");
        }else{
            tapDom("[data-guest=demo]");
            waitJs("guestUi.snapshot?.loaded&&!guestUi.busy");
            require(js("guestUi.snapshot.guest.delta_overrides").equals("0"),"ELF load allocated guest RAM");
            js("guestUi.steps=4;true");tapDom("[data-guest=run]");
            waitJs("guestUi.snapshot.guest.instructions===4&&!guestUi.busy");
            require(!js("guestUi.snapshot.guest.halted").equals("true"),"Partial guest run did not yield");
            JSONObject xi=op("guestXi","mem_store 90000 42\nmem_read 90000\n","guestDeltaResult");
            require(xi.getBoolean("ok")&&xi.getString("xi_output").contains("42"),"XI guest delta did not execute in native core");
            js("guestUi.steps=1000;true");tapDom("[data-guest=run]");
            waitJs("guestUi.snapshot.guest.halted&&!guestUi.busy");
            require(js("guestUi.output").contains("6*7 = 42"),"Native guest did not produce expected output");
            require(js("guestUi.snapshot.guest.delta_overrides").equals("2"),"Unexpected guest delta count");
            String before=hash("guest-aarch64.rvg");
            boolean emptyRejected=RuntimeExecutor.INSTANCE.submit(()->{
                try{new GuestStore(getTargetContext()).create(new byte[0]);return false;}
                catch(Exception expected){return true;}
            }).get(30,TimeUnit.SECONDS);
            require(emptyRejected&&before.equals(hash("guest-aarch64.rvg")),"Empty ELF replaced the saved guest");
            JSONObject failed=op("guestXi","mem_store 90000 55\nunsupported_operation\n","guestFailure");
            require(!failed.getBoolean("ok"),"Bad guest XI program accepted");
            require(before.equals(hash("guest-aarch64.rvg")),"Rejected guest program changed saved checkpoint");
        }
        require(js("document.querySelector('.guest-boundary').textContent.includes('todavía no arrancan')").equals("true"),"UI falsely presents Android boot as implemented");
        require(desktopBefore.equals(hash("session.rvp")),"Guest operation changed the desktop state");
    }
    private static String streamHash(File file)throws Exception{
        MessageDigest hash=MessageDigest.getInstance("SHA-256");byte[] part=new byte[8192];int count;
        try(java.io.InputStream input=new java.io.FileInputStream(file)){while((count=input.read(part))!=-1)hash.update(part,0,count);}
        StringBuilder out=new StringBuilder();for(byte value:hash.digest())out.append(String.format(java.util.Locale.ROOT,"%02x",value&255));return out.toString();
    }
    private void continuousGuestWorkflow()throws Exception{
        String before=hash("session.rvp");
        openGuestView();js("window.confirm=()=>true;true");
        tapDom("[data-guest=demo]");waitJs("guestUi.snapshot?.loaded&&!guestUi.busy");
        tapDom("[data-guest=continuous]");waitJs("guestUi.snapshot.guest.halted&&!guestUi.busy&&!guestUi.running");
        require(js("guestUi.output").contains("6*7 = 42"),"Continuous own guest did not execute and stop at real halt");
        require(js("!!document.querySelector('[data-guest=checkpoint]')").equals("true"),"Checkpoint import control is missing");
        require(before.equals(hash("session.rvp")),"Continuous guest changed the desktop core");
    }
    private void largeGuestWorkflow()throws Exception{
        String desktopBefore=hash("session.rvp");File input=dataFile("large-guest-input.rvg");
        require(input.length()>32L*1024*1024,"Genuine Android checkpoint did not exceed the former Java/JNI ceiling");
        JSONObject imported=RuntimeExecutor.INSTANCE.submit(()->{
            try(java.io.InputStream stream=new java.io.FileInputStream(input)){return new GuestStore(getTargetContext()).importCheckpoint(stream);}
        }).get(60,TimeUnit.SECONDS);
        require(imported.getJSONObject("guest").getLong("instructions")==180326448,"Imported checkpoint is not the pinned genuine Android state");
        openGuestView();js("window.ReverseGuest.imported("+imported+");true");
        JSONObject ran=op("guestRunContinuous","","largeGuestRun");
        require(ran.getBoolean("ok")&&ran.getLong("executed_this_run")==250000&&
            ran.getJSONObject("guest").getLong("instructions")==180576448,"Real Android checkpoint did not advance through UI/JNI/file persistence");
        js("window.ReverseGuest.imported("+ran+");true");
        JSONObject saved=op("guestPause","","largeGuestPause");
        require(saved.getBoolean("ok")&&!saved.getBoolean("checkpoint_dirty"),"Large guest pause did not publish the accepted block");
        String expected=arguments.getString("expected_guest_sha256");
        require(expected!=null&&expected.matches("[0-9a-f]{64}"),"Missing independent native checkpoint digest");
        require(streamHash(dataFile("guest-aarch64.rvg")).equals(expected),"Android file execution differs from independent host execution");
        File exported=dataFile("large-guest-export.rvg");
        RuntimeExecutor.INSTANCE.submit(()->{try(OutputStream stream=new FileOutputStream(exported)){new GuestStore(getTargetContext()).exportTo(stream);}return true;}).get(60,TimeUnit.SECONDS);
        require(streamHash(exported).equals(expected),"Streaming export changed the real checkpoint");
        tapDom("[data-guest=continuous]");waitJs("guestUi.snapshot.guest.instructions>180576448");
        js("continueGuest();true");waitJs("!guestUi.running&&!guestUi.busy");
        String paused=streamHash(dataFile("guest-aarch64.rvg"));Thread.sleep(500);
        require(streamHash(dataFile("guest-aarch64.rvg")).equals(paused),"Paused continuation submitted another block");
        JSONObject restored=RuntimeExecutor.INSTANCE.submit(()->{
            try(java.io.InputStream stream=new java.io.FileInputStream(exported)){return new GuestStore(getTargetContext()).importCheckpoint(stream);}
        }).get(60,TimeUnit.SECONDS);
        js("window.ReverseGuest.imported("+restored+");true");
        require(streamHash(dataFile("guest-aarch64.rvg")).equals(expected),"Restoring the streamed export changed its exact checkpoint");
        boolean rejected=RuntimeExecutor.INSTANCE.submit(()->{
            try(java.io.RandomAccessFile bad=new java.io.RandomAccessFile(input,"rw")){bad.writeByte('X');}
            try(java.io.InputStream stream=new java.io.FileInputStream(input)){
                try{new GuestStore(getTargetContext()).importCheckpoint(stream);return false;}catch(Exception invalid){return true;}
            }
        }).get(60,TimeUnit.SECONDS);
        require(rejected&&streamHash(dataFile("guest-aarch64.rvg")).equals(expected),"Rejected large import replaced the valid guest");
        require(desktopBefore.equals(hash("session.rvp")),"Large Android guest changed the desktop state");
        Files.write(dataFile("large-guest-proof.json").toPath(),new JSONObject().put("input_instructions",180326448)
            .put("instructions",180576448).put("checkpoint_bytes",exported.length()).put("sha256",expected)
            .put("host_state_identical",true).put("continuous_pause_saved",true).put("corrupt_import_preserved",true).put("android_boot",false).toString().getBytes(StandardCharsets.UTF_8));
    }
    private void residentGuestWorkflow()throws Exception{
        String desktopBefore=hash("session.rvp");File input=dataFile("resident-input.rvg");
        require(input.length()>32L*1024*1024,"Resident fixture does not exercise file transport");
        JSONObject imported=RuntimeExecutor.INSTANCE.submit(()->{try(java.io.InputStream stream=new java.io.FileInputStream(input)){return new GuestStore(getTargetContext()).importCheckpoint(stream);}}).get(90,TimeUnit.SECONDS);
        openGuestView();js("window.ReverseGuest.imported("+imported+");true");
        op("guestPause","","residentReset");String initial=streamHash(dataFile("guest-aarch64.rvg"));
        for(int i=1;i<=4;i++){
            JSONObject block=op("guestRunContinuous","","residentBlock"+i);
            require(block.getBoolean("ok")&&block.getBoolean("resident")&&block.getBoolean("checkpoint_dirty"),"Continuous block was not retained live");
            require(block.getJSONObject("guest").getLong("instructions")==250000L*i,"Resident block reloaded or lost progress");
            require(streamHash(dataFile("guest-aarch64.rvg")).equals(initial),"Continuous request rewrote the checkpoint before its deadline");
        }
        JSONObject paused=op("guestPause","","residentPaused");
        require(paused.getBoolean("paused")&&!paused.getBoolean("resident")&&!paused.getBoolean("checkpoint_dirty"),"Pause did not publish and release the session");
        require(streamHash(dataFile("guest-aarch64.rvg")).equals(arguments.getString("resident_expected_sha256")),"Paused RVG differs from independent host execution");
        JSONObject next=op("guestRunContinuous","","residentBeforeBackground");require(next.getJSONObject("guest").getLong("instructions")==1250000,"Resume did not use the committed state");
        Activity backgrounded=activity;
        try(java.io.InputStream shell=new android.os.ParcelFileDescriptor.AutoCloseInputStream(getUiAutomation().executeShellCommand("input keyevent KEYCODE_HOME"))){while(shell.read()!=-1){}}
        // Input delivery does not wait for Activity.onPause or its queued save.
        // Observe onPause returning, then drain the native queue it submitted to.
        for(int i=0;i<150&&pausedActivity!=backgrounded;i++)Thread.sleep(100);
        require(pausedActivity==backgrounded,"HOME did not pause the resident Activity");
        RuntimeExecutor.INSTANCE.submit(()->true).get(90,TimeUnit.SECONDS);
        require(streamHash(dataFile("guest-aarch64.rvg")).equals(arguments.getString("resident_background_sha256")),"Activity pause did not flush accepted progress");
        runOnMainSync(()->getTargetContext().startActivity(new Intent(getTargetContext(),WorkbenchActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)));
        waitDesktopReady();JSONObject resumed=op("guestInspect","","residentResumed");
        require(resumed.getJSONObject("guest").getLong("instructions")==1250000,"Foreground restore changed the paused count");
        RuntimeExecutor.INSTANCE.submit(()->{
            GuestStore older=new GuestStore(getTargetContext()),newer=new GuestStore(getTargetContext());older.inspect();
            JSONObject live=newer.runContinuous();String durable=streamHash(dataFile("guest-aarch64.rvg"));older.pause();
            require(newer.inspect().getBoolean("checkpoint_dirty"),"Old Activity owner closed a newer live session");
            require(streamHash(dataFile("guest-aarch64.rvg")).equals(durable),"Stale owner published another session's state");
            newer.pause();newer.inspect();
            File current=dataFile("guest-aarch64.rvg"),backup=dataFile("resident-durable-backup.rvg");String before=streamHash(current);
            require(current.renameTo(backup),"Cannot stage publication failure test");require(current.mkdir(),"Cannot block publication target");
            Files.write(new File(current,"blocker").toPath(),new byte[]{1});boolean failed=false;
            long accepted;
            try{
                try{newer.run(1);}catch(Exception expected){failed=true;}
                JSONObject pending=newer.inspect();accepted=pending.getJSONObject("guest").getLong("instructions");
                require(failed&&pending.getBoolean("checkpoint_dirty"),"Publication failure acknowledged volatile state");
                require(streamHash(backup).equals(before),"Publication failure changed the last durable copy");
            }finally{new File(current,"blocker").delete();current.delete();require(backup.renameTo(current),"Cannot restore publication target");}
            MessageDigest exported=MessageDigest.getInstance("SHA-256");
            newer.exportTo(new OutputStream(){public void write(int value){exported.update((byte)value);}public void write(byte[] bytes,int at,int length){exported.update(bytes,at,length);}});
            byte[] raw=exported.digest();StringBuilder digest=new StringBuilder();for(byte value:raw)digest.append(String.format(java.util.Locale.ROOT,"%02x",value&255));
            require(streamHash(current).equals(digest.toString()),"Export omitted accepted resident state");
            JSONObject clean=newer.inspect();require(!clean.getBoolean("checkpoint_dirty")&&clean.getJSONObject("guest").getLong("instructions")==accepted,"Save retry re-executed or lost an accepted block");
            newer.pause();return true;
        }).get(120,TimeUnit.SECONDS);
        require(desktopBefore.equals(hash("session.rvp")),"Resident guest modified the desktop authority");
        Files.write(dataFile("resident-proof.json").toPath(),new JSONObject().put("batch_requests",4).put("batch_instructions",1000000)
            .put("activity_pause_instructions",1250000).put("host_rvg_identical",true).put("failed_publication_retry",true)
            .put("stale_owner_protected",true).put("android_boot",false).toString().getBytes(StandardCharsets.UTF_8));
    }
    private void journalSeedWorkflow(boolean torn)throws Exception{
        File input=dataFile("resident-input.rvg");
        JSONObject imported=RuntimeExecutor.INSTANCE.submit(()->{try(java.io.InputStream stream=new java.io.FileInputStream(input)){return new GuestStore(getTargetContext()).importCheckpoint(stream);}}).get(90,TimeUnit.SECONDS);
        openGuestView();js("window.ReverseGuest.imported("+imported+");true");
        String baseSha=streamHash(dataFile("guest-aarch64.rvg"));
        RuntimeExecutor.INSTANCE.submit(()->{
            GuestStore journalStore=new GuestStore(getTargetContext());journalStore.runContinuous();journalStore.runContinuous();
            JSONObject first=journalStore.checkpoint();require(!first.getBoolean("checkpoint_dirty")&&first.getInt("journal_entries")==1,"First periodic journal was not committed");
            journalStore.runContinuous();JSONObject second=journalStore.checkpoint();
            require(second.getInt("journal_entries")==2&&second.getJSONObject("guest").getLong("instructions")==750000,"Second journal lost guest progress");
            journalStore.runContinuous();JSONObject volatileState=journalStore.inspect();
            require(volatileState.getBoolean("checkpoint_dirty")&&volatileState.getJSONObject("guest").getLong("instructions")==1000000,"Crash seed has no separate unsaved window");
            require(streamHash(dataFile("guest-aarch64.rvg")).equals(baseSha),"Periodic checkpoint rewrote the canonical base");
            if(torn)try(java.io.RandomAccessFile damaged=new java.io.RandomAccessFile(dataFile("guest-aarch64.rvg.journal/generation-2/data"),"rw")){damaged.setLength(7);damaged.getFD().sync();}
            JSONObject proof=new JSONObject().put("committed_instructions",torn?500000:750000).put("volatile_instructions",1000000)
                .put("journal_entries",2).put("journal_payload_bytes",second.getLong("journal_bytes")).put("base_sha256",baseSha).put("torn_tail",torn).put("android_boot",false);
            try(FileOutputStream saved=new FileOutputStream(dataFile("journal-seed-proof.json"))){saved.write(proof.toString().getBytes(StandardCharsets.UTF_8));saved.getFD().sync();}
            return true;
        }).get(120,TimeUnit.SECONDS);
        // Kill this disposable test process before Activity pause can compact.
        android.os.Process.killProcess(android.os.Process.myPid());throw new AssertionError("Expected the seed process to terminate");
    }
    private void journalRecoveryWorkflow(boolean torn)throws Exception{
        String desktopBefore=hash("session.rvp");long expectedCount=torn?500000:750000;
        JSONObject recovered=RuntimeExecutor.INSTANCE.submit(()->new GuestStore(getTargetContext()).inspect()).get(90,TimeUnit.SECONDS);
        require(recovered.getJSONObject("guest").getLong("instructions")==expectedCount,"Cold journal recovery repeated or lost committed CPU work");
        require(recovered.getInt("journal_recovered_entries")== (torn?1:2),"Cold recovery selected an incomplete journal entry");
        require(recovered.getBoolean("journal_rolled_back")==torn,"Torn-tail recovery was not reported accurately");
        openGuestView();js("window.ReverseGuest.imported("+recovered+");true");visualCheck(torn?"journal-torn-recovered":"journal-clean-recovered");
        File exported=dataFile("journal-export.rvg");
        RuntimeExecutor.INSTANCE.submit(()->{try(OutputStream stream=new FileOutputStream(exported)){new GuestStore(getTargetContext()).exportTo(stream);}return true;}).get(90,TimeUnit.SECONDS);
        String expectedSha=arguments.getString("journal_expected_sha256");require(expectedSha!=null&&streamHash(exported).equals(expectedSha),"Recovered export differs from independent original execution");
        require(streamHash(dataFile("guest-aarch64.rvg")).equals(expectedSha)&&!dataFile("guest-aarch64.rvg.journal").exists(),"Export did not compact and retire its journal");
        // Storage failure retains the live block; retry saves it without CPU replay.
        JSONObject finalState=RuntimeExecutor.INSTANCE.submit(()->{
            GuestStore journalStore=new GuestStore(getTargetContext());
            try(java.io.InputStream stream=new java.io.FileInputStream(dataFile("resident-input.rvg"))){journalStore.importCheckpoint(stream);}
            journalStore.inspect();journalStore.runContinuous();File blocker=dataFile("guest-aarch64.rvg.journal");
            try(FileOutputStream output=new FileOutputStream(blocker)){output.write(1);}boolean rejected=false;
            try{journalStore.checkpoint();}catch(Exception expected){rejected=true;}
            JSONObject pending=journalStore.inspect();require(rejected&&pending.getBoolean("checkpoint_dirty")&&pending.getJSONObject("guest").getLong("instructions")==250000,"Failed journal write discarded accepted progress");
            require(blocker.delete(),"Cannot remove journal failure blocker");JSONObject retried=journalStore.checkpoint();
            require(!retried.getBoolean("checkpoint_dirty")&&retried.getJSONObject("guest").getLong("instructions")==250000,"Journal retry replayed CPU");
            // Importing the very same original base must explicitly discard its
            // later journal, rather than replaying that history over the import.
            try(java.io.InputStream stream=new java.io.FileInputStream(dataFile("resident-input.rvg"))){journalStore.importCheckpoint(stream);}
            JSONObject restored=journalStore.inspect();require(restored.getJSONObject("guest").getLong("instructions")==0&&!dataFile("guest-aarch64.rvg.journal").exists(),"Explicit original import replayed an obsolete journal");
            journalStore.pause();return restored;
        }).get(120,TimeUnit.SECONDS);
        openGuestView();js("window.ReverseGuest.imported("+finalState+");true");
        require(desktopBefore.equals(hash("session.rvp")),"Journal changed the desktop/BBY Q authority");
        Files.write(dataFile("journal-recovery-proof.json").toPath(),new JSONObject().put("instructions",expectedCount).put("sha256",expectedSha)
            .put("process_death_recovered",true).put("torn_tail",torn).put("failed_write_retry",true).put("explicit_import_preserved",true)
            .put("host_state_identical",true).put("android_boot",false).toString().getBytes(StandardCharsets.UTF_8));
    }
    private void systemWorkflow(boolean restore)throws Exception{
        String desktopBefore=hash("session.rvp");
        openGuestView();
        require(js("state.windows.some(w=>w.app==='xi'&&w.xiTab==='arm64')&&!state.windows.some(w=>w.app==='guest')").equals("true"),"ARM64 was not integrated into existing XI window");
        require(js("!SHELL_APPS.includes('guest')").equals("true"),"Separate Guest application is still in the launcher");
        if(!restore){
            // Replace the previous EL0 witness through the same native executor.
            js("window.confirm=()=>true;true");tapDom("[data-guest=system]");
            waitJs("guestUi.snapshot?.guest?.system_mode&&!guestUi.busy");
            require(js("guestUi.snapshot.guest.delta_overrides").equals("0"),"System image allocated a RAM array");
            JSONObject quantum=op("guestXi","#!reverse-pc-machine-v1\nset r0 0\nlabel loop\nh $r0\nadd r0 1\ncmp r0 4096\njlt loop\nhalt\n","armQuantumResult");
            require(quantum.getBoolean("ok")&&quantum.getJSONObject("guest").getInt("active_qubits")==4096,"Existing 4096-qubit XI engine is unavailable to ARM64 module");
            js("guestUi.steps=30;true");tapDom("[data-guest=run]");
            waitJs("guestUi.snapshot.guest.instructions===30&&!guestUi.busy");
            js("guestUi.steps=1000;true");tapDom("[data-guest=run]");
            waitJs("guestUi.snapshot.guest.halted&&!guestUi.busy");
            require(js("guestUi.output").contains("EL1 MMU IRQ EL0 SVC FAULT OK"),"System witness failed through actual WebView/JNI/executor");
        }
        require(js("guestUi.snapshot.guest.mmu_enabled&&guestUi.snapshot.guest.exceptions===4&&guestUi.snapshot.guest.active_qubits===4096").equals("true"),"System/quantum state did not survive execution or restart");
        require(js("guestUi.snapshot.guest.core_revision").equals("4096"),"ARM64 operations changed quantum gates/revision");
        require(desktopBefore.equals(hash("session.rvp")),"ARM64 module changed desktop/BBY Q state");
    }
    private void floatingWorkflow(boolean restore)throws Exception{
        String desktopBefore=hash("session.rvp");
        openGuestView();
        if(!restore){
            js("window.confirm=()=>true;true");tapDom("[data-guest=float]");
            waitJs("guestUi.snapshot?.loaded&&!guestUi.busy");
            require(js("guestUi.snapshot.guest.delta_overrides").equals("0"),"FP witness allocated guest RAM before execution");
            JSONObject quantum=op("guestXi","#!reverse-pc-machine-v1\nset r0 0\nlabel loop\nh $r0\nadd r0 1\ncmp r0 4096\njlt loop\nhalt\n","fpQuantum");
            require(quantum.getBoolean("ok"),"FP witness lost the original XI quantum engine");
            js("guestUi.steps=10;true");tapDom("[data-guest=run]");
            waitJs("guestUi.snapshot.guest.instructions===10&&!guestUi.busy");
            require(js("guestUi.snapshot.guest.nonzero_vector_registers>0&&!guestUi.snapshot.guest.halted").equals("true"),"FP state was absent at checkpoint boundary");
            js("guestUi.steps=1000;true");tapDom("[data-guest=run]");
            waitJs("guestUi.snapshot.guest.halted&&!guestUi.busy");
            require(js("guestUi.output").contains("REVERSE: FP32 FP64 SIMD OK\n"),"FP/SIMD witness failed through UI/JNI/checkpoint");
        }
        require(js("guestUi.snapshot.guest.halted&&guestUi.snapshot.guest.exit_code===0&&guestUi.snapshot.guest.nonzero_vector_registers===4&&guestUi.snapshot.guest.active_qubits===4096&&guestUi.snapshot.guest.core_revision===4096").equals("true"),"FP/quantum state did not survive execution or restart");
        require(Files.readAllBytes(dataFile("guest-aarch64.rvg").toPath())[3]=='3',"FP state was not stored as RVG3");
        require(desktopBefore.equals(hash("session.rvp")),"FP/SIMD changed desktop/BBY Q state");
    }
    @Override public void onStart(){
        Bundle result=new Bundle();
        try{
            String requested=arguments==null?"full":arguments.getString("mode","full");
            if(requested.equals("migration")){
                int checks=E025MigrationChecks.run(getTargetContext(),getContext());
                result.putString("stream","WORKBENCH_TESTS_OK mode=migration checks="+checks+" native=XI/Ether\n");
                result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            boolean boot=requested.equals("boot"),seed=requested.equals("upgradeSeed"),upgrade=requested.equals("upgrade"),legacy=requested.equals("legacy");
            ActivityMonitor desktopMonitor=addMonitor(WorkbenchActivity.class.getName(),null,false);
            Class<?> entry=boot?BootActivity.class:legacy?DesktopActivity.class:WorkbenchActivity.class;
            runOnMainSync(()->getTargetContext().startActivity(new Intent(getTargetContext(),entry).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)));
            activity=(WorkbenchActivity)waitForMonitorWithTimeout(desktopMonitor,15000);removeMonitor(desktopMonitor);
            require(activity!=null,"Entry did not reach Workbench: "+requested);
            JSONObject snapshot=startupSnapshot();
            require(snapshot.getBoolean("ok"),snapshot.toString());
            require(snapshot.getJSONObject("android").getLong("ram_total_bytes")>0,"No native RAM reading");
            require(snapshot.getJSONObject("core").getString("state_format").equals("RVPC2"),"Wrong checkpoint format");
            require(snapshot.getJSONObject("build").getString("source_commit").matches("[a-f0-9]{40}"),"Missing build provenance");
            if(requested.equals("guestJournalSeed")||requested.equals("guestJournalTornSeed")){
                journalSeedWorkflow(requested.equals("guestJournalTornSeed"));throw new AssertionError("Journal seed did not terminate");
            }
            if(requested.equals("guestJournalRecover")||requested.equals("guestJournalTornRecover")){
                journalRecoveryWorkflow(requested.equals("guestJournalTornRecover"));visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" original-rvg=true journal-recovery=true\n");result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("guestResident")){
                residentGuestWorkflow();visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode=guestResident native=XI/Ether resident=true original-rvg=true\n");result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("guestContinuous")||requested.equals("guestLarge")){
                if(requested.equals("guestLarge"))largeGuestWorkflow();else continuousGuestWorkflow();
                visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" ");result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("armCounter")||requested.equals("armCounterRestore")||requested.equals("armCounterRate")||requested.equals("armCounterRateRestore")){
                boolean paced=requested.startsWith("armCounterRate");counterWorkflow(requested.endsWith("Restore"),paced);visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" clock=host-seeded-hmac-v1 samples=64 state="+(paced?"v7":"v6")+" test-seed-only\n");result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("armGraphics")||requested.equals("armGraphicsRestore")){
                graphicsWorkflow(requested.equals("armGraphicsRestore"));visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" integration=XI pci=virtio-gpu pixels=verified android-boot=not-implemented\n");result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("armFloat")||requested.equals("armFloatRestore")){
                floatingWorkflow(requested.equals("armFloatRestore"));visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" integration=XI fp=ARM64 SIMD state=RVG3 qubits=4096\n");
                result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("armSystem")||requested.equals("armSystemRestore")){
                systemWorkflow(requested.equals("armSystemRestore"));visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" integration=XI engine=original-QuantumRuntime qubits=4096 android-boot=not-implemented\n");
                result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(requested.equals("guest")||requested.equals("guestRestore")){
                guestWorkflow(requested.equals("guestRestore"));visualCheck(requested);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" guest=own-AArch64 memory=XI/Ether android-boot=not-implemented\n");
                result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            if(!seed&&js("typeof window.ReverseShell==='object'").equals("true")){
                desktopShellWorkflow(requested,snapshot);
                result.putString("stream","WORKBENCH_TESTS_OK mode="+requested+" native=XI/Ether desktop-shell=verified\n");result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);return;
            }
            boolean restore=boot||legacy||upgrade||requested.equals("restore");
            if(restore){
                require(snapshot.getJSONObject("core").getLong("active_qubits")>=128,"Native state did not survive process restart");
                require(js("state.prefs.dim").equals("23"),"Preferences did not survive restart");
                require(js("state.files.some(f=>f.name==='Prueba-nativa.txt'&&!f.trashRoot)").equals("true"),"File did not survive restart");
            }else{
                JSONObject arithmetic=op("calculate","2+3*4","testArithmetic");
                require(arithmetic.getBoolean("ok")&&arithmetic.getString("value").equals("14"),"XI arithmetic failed");
                require(arithmetic.getString("source").contains("NativeRuntime"),"Arithmetic source missing");
                JSONObject decimal=op("calculate","0.1+0.2","testDecimal");require(decimal.getString("value").equals("0.3"),"Fixed-point decimal failed");
                JSONObject invalid=op("calculate","1/0","testInvalid");require(!invalid.getBoolean("ok"),"Division by zero accepted");
                JSONObject probe=op("execute",js("XI_PROBE"),"testProbe");
                require(probe.getBoolean("ok"),probe.toString());
                require(probe.getJSONObject("core").getLong("active_qubits")==128,"GHZ probe did not create 128 active qubits");
                require(probe.getString("output").contains("42"),"Probe arithmetic result missing");
                js("createWindow('calculator');true");
                js("document.querySelector('.window[aria-label=Calculadora] [data-calc=\"7\"]').click();document.querySelector('.window[aria-label=Calculadora] [data-calc=\"×\"]').click();document.querySelector('.window[aria-label=Calculadora] [data-calc=\"6\"]').click();document.querySelector('.window[aria-label=Calculadora] [data-calc=\"=\"]').click();true");
                waitJs("document.querySelector('.calc-history').textContent.includes('XI')");
                require(js("document.querySelector('.calc-display').value").equals("42"),"UI did not display native result");
                js("window.testFile=createFile('text','Prueba-nativa.txt','personal','Texto persistente real');deleteFile(testFile);true");
                require(js("state.files.find(f=>f.id===testFile.id).trashRoot===testFile.id").equals("true"),"Trash did not change index");
                js("restoreFile(testFile);setPref('dim',23,false);window.testSaved=null;persist().then(ok=>window.testSaved=ok);true");
                require(waitJs("window.testSaved").equals("true"),"Native profile was not acknowledged");
                require(js("state.files.find(f=>f.id===testFile.id).trashRoot===undefined").equals("true"),"Trash restore failed");
                js("createWindow('control',{tab:'system'});true");
                waitJs("document.querySelector('.diagnostic-table')&&document.querySelector('.diagnostic-table').textContent.includes('Conectado')");
                require(js("getComputedStyle(document.querySelector('.taskbar')).position").equals("fixed"),"Taskbar is not fixed");
                require(js("getComputedStyle(document.querySelector('#wall-image')).backgroundImage.includes('pliegue-default')").equals("true"),"Approved wallpaper is not loaded");
                // The trusted WebView must reject a remote subframe before it can use the bridge.
                require(js("document.querySelectorAll('iframe').length").equals("0"),"Unexpected third-party frame");
            }
            if(seed)seedUpgrade();
            else if(restore&&!upgrade){waitJs("document.querySelector('[data-program-input]:not(:disabled)')");require(js("document.querySelector('[aria-label=\"Respuesta BBY Q\"]').textContent").equals("Ana has five apples."),"Restored app response differs");retainedProof(false);}
            else{programNavigation(upgrade);appearanceWorkflow();}
            if(!seed)fullDisplay();
            String mode=boot?"boot":requested;visualCheck(mode);
            String text="WORKBENCH_TESTS_OK mode="+mode+" native=XI/Ether checkpoint=RVPC2 visual=drawn\n";
            result.putString("stream",text);result.putBoolean("passed",true);finish(Activity.RESULT_OK,result);
        }catch(Throwable failure){
            android.util.Log.e("ReverseWorkbenchTest","WORKBENCH_TESTS_FAILED",failure);
            if(activity!=null)try{File folder=new File(getTargetContext().getExternalFilesDir(null),"visual-evidence");folder.mkdirs();Bitmap screen=getUiAutomation().takeScreenshot();try(FileOutputStream output=new FileOutputStream(new File(folder,"failure-screen.png"))){screen.compress(Bitmap.CompressFormat.PNG,100,output);}finally{screen.recycle();}Files.write(new File(folder,"failure-dom.txt").toPath(),js("document.documentElement.outerHTML").getBytes(StandardCharsets.UTF_8));}catch(Throwable ignored){}
            result.putString("stream","WORKBENCH_TESTS_FAILED "+failure+"\n");result.putString("error",failure.toString());finish(Activity.RESULT_CANCELED,result);
        }
    }
}


