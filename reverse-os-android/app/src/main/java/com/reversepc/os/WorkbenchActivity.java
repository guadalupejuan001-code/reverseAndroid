package com.reversepc.os;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.graphics.Insets;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Base64;
import android.util.Log;
import android.util.AtomicFile;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import android.widget.Toast;
import org.json.JSONObject;
import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

/** Local packaged desktop. Only owned assets can access the native bridge. */
public final class WorkbenchActivity extends Activity {
    static final String ORIGIN="https://reverse.local/";
    private static final int PICK=7101,EXPORT=7102,IMPORT_RXE=7103,IMPORT_GUEST_ELF=7104,IMPORT_LINUX_IMAGE=7105,IMPORT_SYSTEM_ELF=7106,IMPORT_GUEST_CHECKPOINT=7107,IMPORT_GUEST_DISK=7108;
    private final Handler ui=new Handler(Looper.getMainLooper());
    private volatile boolean closed;
    private volatile boolean activityActive;
    private volatile long activityEpoch;
    private SessionStore store;
    private WorkbenchPrograms programs;
    private GuestStore guest;
    private WebView web;
    private ValueCallback<Uri[]> chooser;
    private byte[] pendingExport;
    private boolean pendingGuestExport;
    private int insetTop,insetBottom,insetLeft,insetRight,keyboardInset;
    private boolean keyboardVisible;

    static void immersive(Activity activity){
        Window window=activity.getWindow();
        WindowManager.LayoutParams layout=window.getAttributes();
        layout.layoutInDisplayCutoutMode=WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        window.setAttributes(layout);
        window.setStatusBarColor(android.graphics.Color.TRANSPARENT);
        window.setNavigationBarColor(android.graphics.Color.TRANSPARENT);
        window.setNavigationBarContrastEnforced(false);
        window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            |View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            |View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if(Build.VERSION.SDK_INT>=30){
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller=window.getInsetsController();
            if(controller!=null){controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);controller.hide(WindowInsets.Type.systemBars());}
        }
    }

    @Override public void onCreate(Bundle previous){
        super.onCreate(previous);
        immersive(this);
        FrameLayout root=new FrameLayout(this);root.setBackgroundColor(0xff090b0d);
        web=new WorkbenchWebView(this);root.addView(web,new FrameLayout.LayoutParams(-1,-1));setContentView(root);
        root.setOnApplyWindowInsetsListener((v,insets)->{
            // The wallpaper fills the display, including cutout/system-bar areas.
            // Only the on-screen keyboard reserves space for the message composer.
            int keyboard=Build.VERSION.SDK_INT>=30&&insets.isVisible(WindowInsets.Type.ime())?insets.getInsets(WindowInsets.Type.ime()).bottom:0;
            if(Build.VERSION.SDK_INT>=30){keyboardInset=keyboard;keyboardVisible=insets.isVisible(WindowInsets.Type.ime());}
            insetTop=insets.getDisplayCutout()==null?0:insets.getDisplayCutout().getSafeInsetTop();
            insetBottom=insets.getMandatorySystemGestureInsets().bottom;
            insetLeft=insets.getDisplayCutout()==null?0:insets.getDisplayCutout().getSafeInsetLeft();
            insetRight=insets.getDisplayCutout()==null?0:insets.getDisplayCutout().getSafeInsetRight();
            if(Build.VERSION.SDK_INT>=30){
                root.setPadding(0,0,0,keyboard);publishInsets();
                // We reserve the IME space once, in the native parent. WebView
                // M139+ must still receive updates, with the handled inset zeroed.
                return new WindowInsets.Builder(insets).setInsets(WindowInsets.Type.ime(),Insets.NONE).build();
            }
            publishInsets();return insets;
        });
        if(Build.VERSION.SDK_INT<30)root.getViewTreeObserver().addOnGlobalLayoutListener(()->{
            Rect visible=new Rect();root.getWindowVisibleDisplayFrame(visible);
            int[] position=new int[2];root.getLocationOnScreen(position);
            int overlap=Math.max(0,position[1]+root.getHeight()-visible.bottom);
            boolean shown=overlap>80*getResources().getDisplayMetrics().density;
            int height=shown?overlap:0;
            if(keyboardInset!=height||keyboardVisible!=shown){keyboardInset=height;keyboardVisible=shown;root.setPadding(0,0,0,height);publishInsets();}
        });
        WebSettings settings=web.getSettings();
        settings.setJavaScriptEnabled(true);settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);settings.setAllowContentAccess(true);
        settings.setAllowFileAccessFromFileURLs(false);settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        settings.setBlockNetworkLoads(true);
        settings.setCacheMode(WebSettings.LOAD_NO_CACHE);
        settings.setSupportMultipleWindows(false);
        web.setBackgroundColor(0xff090b0d);
        web.addJavascriptInterface(new Bridge(),"ReverseNative");
        web.setWebViewClient(new WebViewClient(){
            @Override public WebResourceResponse shouldInterceptRequest(WebView view,WebResourceRequest request){return asset(request.getUrl());}
            @Override public boolean shouldOverrideUrlLoading(WebView view,WebResourceRequest request){
                Uri uri=request.getUrl();
                if(trusted(uri))return false;
                // Never load an external document into a WebView holding the bridge.
                if(request.isForMainFrame()&&"https".equals(uri.getScheme()))ui.post(()->{
                    try{startActivity(new Intent(Intent.ACTION_VIEW,uri));}catch(Exception error){message("No hay un navegador disponible.");}
                });
                return true;
            }
            @Override public void onPageFinished(WebView view,String url){Log.i("ReverseWorkbench","WORKBENCH_READY "+url);publishInsets();}
        });
        web.setWebChromeClient(new WebChromeClient(){
            @Override public boolean onShowFileChooser(WebView view,ValueCallback<Uri[]> callback,FileChooserParams params){
                if(chooser!=null)chooser.onReceiveValue(null);chooser=callback;
                try{startActivityForResult(params.createIntent(),PICK);}catch(Exception error){chooser=null;callback.onReceiveValue(null);message("No se pudo abrir el selector de archivos.");}
                return true;
            }
            @Override public boolean onConsoleMessage(android.webkit.ConsoleMessage message){
                Log.i("ReverseWorkbenchJS",message.messageLevel()+": "+message.message());return true;
            }
        });
        web.loadUrl(ORIGIN);
    }
    private void publishInsets(){
        if(web==null)return;float density=getResources().getDisplayMetrics().density;
        String script="window.ReverseShell&&window.ReverseShell.setInsets({top:"+(insetTop/density)+",bottom:"+(insetBottom/density)+",left:"+(insetLeft/density)+",right:"+(insetRight/density)+",imeVisible:"+keyboardVisible+",imeHeight:"+(keyboardInset/density)+"});";
        web.evaluateJavascript(script,null);
    }
    private void hideKeyboard(){
        if(web==null)return;
        ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(web.getWindowToken(),0);
        if(Build.VERSION.SDK_INT>=30){WindowInsetsController controller=web.getWindowInsetsController();if(controller!=null)controller.hide(WindowInsets.Type.ime());}
    }
    private static boolean trusted(Uri uri){return "https".equals(uri.getScheme())&&"reverse.local".equals(uri.getHost())&&(uri.getPort()==-1||uri.getPort()==443);}
    private WebResourceResponse asset(Uri uri){
        try{
            if(!trusted(uri))return denied();
            String name=uri.getPath();if(name==null||name.equals("/"))name="/index.html";
            if(name.contains("..")||name.contains("\\")||name.contains("\u0000"))return denied();
            String mime=name.endsWith(".js")?"text/javascript":name.endsWith(".css")?"text/css":name.endsWith(".png")?"image/png":name.endsWith(".webp")?"image/webp":name.endsWith(".svg")?"image/svg+xml":"text/html";
            InputStream data=getAssets().open("workbench"+name);
            Map<String,String> headers=new HashMap<>();
            headers.put("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'none'; frame-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'");
            headers.put("X-Content-Type-Options","nosniff");
            headers.put("Cache-Control","no-store");
            return new WebResourceResponse(mime,"UTF-8",200,"OK",headers,data);
        }catch(Exception error){return denied();}
    }
    private static WebResourceResponse denied(){return new WebResourceResponse("text/plain","UTF-8",403,"Forbidden",new HashMap<>(),new ByteArrayInputStream(new byte[0]));}
    private void message(String text){ui.post(()->{if(!closed)Toast.makeText(this,text,Toast.LENGTH_LONG).show();});}
    private SessionStore session()throws Exception{if(store==null)store=new SessionStore(getApplicationContext());return store;}
    private WorkbenchPrograms programs()throws Exception{if(programs==null)programs=new WorkbenchPrograms(getApplicationContext());return programs;}
    private GuestStore guest(){if(guest==null)guest=new GuestStore(getApplicationContext());return guest;}
    private String task(Callable<JSONObject> action){
        if(closed)return failure(new IllegalStateException("La mesa está cerrada"));
        try{
            // A timeout does not cancel an in-flight native transaction. It stays on
            // the same queue and commits once; the UI is told the result is unknown.
            Future<JSONObject> future=RuntimeExecutor.INSTANCE.submit(action);
            return future.get(45,TimeUnit.SECONDS).toString();
        }catch(Throwable error){return failure(error);}
    }
    private static String failure(Throwable error){
        Throwable cause=error.getCause()==null?error:error.getCause();
        try{return new JSONObject().put("ok",false).put("error",cause.getClass().getSimpleName()+": "+String.valueOf(cause.getMessage())).toString();}
        catch(Exception ignored){return "{\"ok\":false,\"error\":\"Error del runtime\"}";}
    }
    private JSONObject inspect()throws Exception{
        SessionStore s=session();byte[] state=s.loadState();
        JSONObject core=new JSONObject(NativeRuntime.inspect(state));
        ActivityManager.MemoryInfo memory=new ActivityManager.MemoryInfo();
        ((ActivityManager)getSystemService(ACTIVITY_SERVICE)).getMemoryInfo(memory);
        JSONObject host=new JSONObject().put("model",Build.MANUFACTURER+" "+Build.MODEL).put("android",Build.VERSION.RELEASE).put("sdk",Build.VERSION.SDK_INT)
            .put("hardware",Build.HARDWARE).put("soc",Build.VERSION.SDK_INT>=31?Build.SOC_MODEL:JSONObject.NULL)
            .put("logical_processors",Runtime.getRuntime().availableProcessors()).put("ram_total_bytes",memory.totalMem).put("ram_available_bytes",memory.availMem)
            .put("process_pss_kib",Debug.getPss()).put("process_cpu_ms",android.os.Process.getElapsedCpuTime()).put("pid",android.os.Process.myPid());
        JSONObject result=new JSONObject().put("ok",true).put("mode","android-native").put("core",core).put("android",host).put("state_bytes",state.length)
            .put("package",getPackageName()).put("profile_bytes",profileFile().getBaseFile().length()).put("captured_at_ms",System.currentTimeMillis());
        try(InputStream input=getAssets().open("workbench/build-info.json")){java.io.ByteArrayOutputStream bytes=new java.io.ByteArrayOutputStream();byte[] part=new byte[2048];int count;while((count=input.read(part))!=-1)bytes.write(part,0,count);result.put("build",new JSONObject(new String(bytes.toByteArray(),StandardCharsets.UTF_8)));}
        return result;
    }
    public final class Bridge {
        private String guestAutoToken;
        private long guestAutoEpoch;
        @JavascriptInterface public void hideKeyboard(){ui.post(WorkbenchActivity.this::hideKeyboard);}
        @JavascriptInterface public void request(String id,String operation,String payload){
            if(closed||id==null||!id.matches("[A-Za-z0-9_-]{1,60}"))return;
            final long submittedEpoch=activityEpoch;
            RuntimeExecutor.INSTANCE.execute(()->{
                String response;
                try{
                    JSONObject result;
                    if("inspect".equals(operation))result=WorkbenchActivity.this.inspect();
                    else if("calculate".equals(operation))result=calculateNative(payload);
                    else if("execute".equals(operation))result=executeNative(payload);
                    else if("experiment".equals(operation))result=experimentNative(payload);
                    else if("guestInspect".equals(operation))result=guest().inspect();
                    else if("guestPause".equals(operation))result=guest().pause();
                    else if("guestDemo".equals(operation))result=guest().create(null);
                    else if("guestSystem".equals(operation))result=guest().createSystem(null,0);
                    else if("guestFloat".equals(operation))result=guest().createSystem(null,3);
                    else if("guestGraphics".equals(operation))result=guest().createSystem(null,4);
                    else if("guestDisplay".equals(operation))result=guest().display();
                    else if("guestRun".equals(operation))result=guest().run(new JSONObject(payload).getInt("steps"));
                    else if("guestRunContinuous".equals(operation)){
                        String token=payload==null||payload.isEmpty()?"single-"+submittedEpoch:new JSONObject(payload).getString("continuation");
                        if(!token.matches("[A-Za-z0-9_-]{1,60}"))throw new IllegalArgumentException("Continuación no válida");
                        if(!activityActive||submittedEpoch!=activityEpoch)throw new IllegalStateException("Ejecución pausada al salir de REVERSE.");
                        if(!token.equals(guestAutoToken)){guestAutoToken=token;guestAutoEpoch=submittedEpoch;}
                        if(guestAutoEpoch!=activityEpoch)throw new IllegalStateException("Pulsa Continuar para reanudar el invitado.");
                        result=guest().runContinuous().put("continuation_paused",activityEpoch!=submittedEpoch);
                    }
                    else if("guestXi".equals(operation))result=guest().xi(payload);
                    else if("guestInput".equals(operation))result=guest().input(payload);
                    else if("saveProfile".equals(operation))result=saveProfile(payload);
                    else if("listApps".equals(operation))result=programs().catalog();
                    else if("openApp".equals(operation))result=programs().open(new JSONObject(payload).getString("path"));
                    else if("askApp".equals(operation))result=programs().ask(new JSONObject(payload));
                    else if("ackControl".equals(operation))result=programs().acknowledge(new JSONObject(payload));
                    else if("runApp".equals(operation))result=programs().run(new JSONObject(payload),session());
                    else if("compileApp".equals(operation))result=programs().compile(new JSONObject(payload));
                    else throw new IllegalArgumentException("Operación no permitida");
                    response=result.toString();
                }catch(Throwable error){response=failure(error);}
                final String script="window.ReverseAndroid&&window.ReverseAndroid.complete("+JSONObject.quote(id)+","+response+");";
                ui.post(()->{if(!closed)web.evaluateJavascript(script,null);});
            });
        }
        @JavascriptInterface public String inspect(){return task(()->WorkbenchActivity.this.inspect());}
        @JavascriptInterface public String loadProfile(){return task(()->new JSONObject().put("ok",true).put("profile",profileFile().getBaseFile().exists()?new String(profileFile().readFully(),StandardCharsets.UTF_8):JSONObject.NULL));}
        @JavascriptInterface public String execute(String source){return task(()->executeNative(source));}
        @JavascriptInterface public String calculate(String expression){return task(()->calculateNative(expression));}
        @JavascriptInterface public void openNativeDesktop(){
            // Compatibility for a restored old shortcut: stay in this Workbench.
            ui.post(()->{if(!closed)web.evaluateJavascript("createWindow('programs')",null);});
        }
        @JavascriptInterface public void importRxe(){
            ui.post(()->{if(!closed)try{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"),IMPORT_RXE);}catch(Exception error){message("No se pudo abrir el selector de ejecutables.");}});
        }
        @JavascriptInterface public void importGuestElf(){
            ui.post(()->{if(!closed)try{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"),IMPORT_GUEST_ELF);}catch(Exception error){cancelGuestImport();message("No se pudo abrir el selector ELF.");}});
        }
        @JavascriptInterface public void importLinuxImage(){
            ui.post(()->{if(!closed)try{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"),IMPORT_LINUX_IMAGE);}catch(Exception error){cancelGuestImport();message("No se pudo abrir el selector Image.");}});
        }
        @JavascriptInterface public void importSystemElf(){
            ui.post(()->{if(!closed)try{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"),IMPORT_SYSTEM_ELF);}catch(Exception error){cancelGuestImport();message("No se pudo abrir el selector ELF de sistema.");}});
        }
        @JavascriptInterface public void importGuestCheckpoint(){
            ui.post(()->{if(!closed)try{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"),IMPORT_GUEST_CHECKPOINT);}catch(Exception error){cancelGuestImport();message("No se pudo abrir el selector de checkpoints.");}});
        }
        @JavascriptInterface public void importGuestDisk(){
            ui.post(()->{if(!closed)try{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"),IMPORT_GUEST_DISK);}catch(Exception error){cancelGuestImport();message("No se pudo abrir el selector de discos.");}});
        }
        @JavascriptInterface public void exportGuest(){
            ui.post(()->{
                if(closed)return;
                if(pendingExport!=null||pendingGuestExport){message("Termina primero la exportación abierta.");return;}
                pendingGuestExport=true;
                try{startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/octet-stream").putExtra(Intent.EXTRA_TITLE,"REVERSE-invitado.rvg"),EXPORT);}
                catch(Exception error){pendingGuestExport=false;message("No se pudo abrir el destino de exportación.");}
            });
        }
        @JavascriptInterface public void exportRxe(String path,String identity,boolean memory){
            RuntimeExecutor.INSTANCE.execute(()->{try{
                JSONObject request=new JSONObject().put("path",path).put("identity",identity);
                byte[] bytes=programs().exportImage(request,memory);
                String name=path.substring(path.lastIndexOf('/')+1);if(memory)name=name.substring(0,name.length()-4)+"-memoria.rvp";
                export(name,bytes,memory?"application/x-reverse-state":"application/x-reverse-executable");
            }catch(Exception error){message("No se pudo exportar: "+error.getMessage());}});
        }
        @JavascriptInterface public void exportText(String name,String contents,String mime){
            if(contents==null||contents.length()>6*1024*1024){message("El archivo supera el límite de exportación.");return;}
            export(name,contents.getBytes(StandardCharsets.UTF_8),mime);
        }
        @JavascriptInterface public void exportBase64(String name,String contents,String mime){
            try{if(contents==null||contents.length()>8*1024*1024)throw new IllegalArgumentException("Archivo demasiado grande");export(name,Base64.decode(contents,Base64.DEFAULT),mime);}
            catch(Exception error){message("No se pudo exportar la imagen.");}
        }
    }
    private AtomicFile profileFile(){return new AtomicFile(new java.io.File(getFilesDir(),"workbench-profile.json"));}
    private JSONObject saveProfile(String profile)throws Exception{
        if(profile==null||profile.length()>3600000)throw new IllegalArgumentException("Perfil supera el límite");
        JSONObject decoded=new JSONObject(profile);
        if(decoded.getInt("version")!=1||decoded.getJSONArray("files").length()>500)throw new IllegalArgumentException("Perfil no válido");
        byte[] bytes=profile.getBytes(StandardCharsets.UTF_8);
        if(bytes.length>12*1024*1024)throw new IllegalArgumentException("Perfil demasiado grande");
        AtomicFile target=profileFile();java.io.FileOutputStream stream=null;
        try{stream=target.startWrite();stream.write(bytes);target.finishWrite(stream);}
        catch(Exception error){if(stream!=null)target.failWrite(stream);throw error;}
        return new JSONObject().put("ok",true).put("bytes",bytes.length).put("source","AtomicFile · workbench-profile.json");
    }
    private JSONObject experimentNative(String source)throws Exception{
        if(source==null||source.getBytes(StandardCharsets.UTF_8).length>256*1024)throw new IllegalArgumentException("Programa demasiado grande");
        long started=System.nanoTime();
        ResultPacket result=ResultPacket.parse(NativeRuntime.execute(NativeRuntime.initialize(),source));
        JSONObject core=new JSONObject(NativeRuntime.inspect(result.state));
        return new JSONObject().put("ok",true).put("output",result.output).put("core",core).put("isolated",true).put("elapsed_ns",System.nanoTime()-started);
    }
    @Override public void onBackPressed(){
        if(keyboardVisible){hideKeyboard();return;}
        if(web==null){super.onBackPressed();return;}
        web.evaluateJavascript("window.ReverseShell?window.ReverseShell.back():false",handled->{if(!"true".equals(handled))WorkbenchActivity.super.onBackPressed();});
    }
    private JSONObject executeNative(String source)throws Exception{
        if(source==null||source.getBytes(StandardCharsets.UTF_8).length>256*1024)throw new IllegalArgumentException("Programa supera 256 KiB");
        SessionStore s=session();long start=SystemClock.elapsedRealtimeNanos();
        ResultPacket result=ResultPacket.parse(NativeRuntime.execute(s.loadState(),source));
        s.writeState(result.state);
        return new JSONObject().put("ok",true).put("output",result.output).put("elapsed_ns",SystemClock.elapsedRealtimeNanos()-start).put("core",new JSONObject(NativeRuntime.inspect(result.state))).put("source","NativeRuntime.execute · XI/Ether · RVPC2");
    }
    private JSONObject calculateNative(String expression)throws Exception{
        if(expression==null||expression.length()>256)throw new IllegalArgumentException("Expresión demasiado larga");
        SessionStore s=session();String program=new CalculatorCompiler().compile(expression);
        ResultPacket result=ResultPacket.parse(NativeRuntime.execute(s.loadState(),program));
        Long value=null;
        for(String line:result.output.split("\n")){if(!line.startsWith("{"))continue;JSONObject row=new JSONObject(line);if("r0".equals(row.optString("register")))value=row.getLong("value");}
        if(value==null)throw new IllegalStateException("XI no devolvió el registro r0");
        s.writeState(result.state);
        return new JSONObject().put("ok",true).put("value",CalculatorCompiler.format(value)).put("source","CalculatorCompiler → NativeRuntime.execute · XI");
    }
    private void export(String name,byte[] bytes,String mime){
        if(bytes.length>8*1024*1024){message("Archivo demasiado grande.");return;}
        String safe=(name==null?"REVERSE.txt":name).replaceAll("[\\\\/\\p{Cntrl}]","_");
        String type=mime!=null&&mime.matches("[a-zA-Z0-9.+-]+/[a-zA-Z0-9.+-]+(?:;.*)?")?mime.split(";")[0]:"application/octet-stream";
        ui.post(()->{
            if(closed)return;
            if(pendingExport!=null||pendingGuestExport){message("Termina primero la exportación abierta.");return;}
            pendingExport=bytes;
            try{startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType(type).putExtra(Intent.EXTRA_TITLE,safe),EXPORT);}
            catch(Exception error){pendingExport=null;message("No se pudo abrir el destino de exportación.");}
        });
    }
    private void cancelGuestImport(){if(!closed&&web!=null)web.evaluateJavascript("window.ReverseGuest&&window.ReverseGuest.cancelImport()",null);}
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        boolean guestImport=request==IMPORT_GUEST_ELF||request==IMPORT_LINUX_IMAGE||request==IMPORT_SYSTEM_ELF||request==IMPORT_GUEST_CHECKPOINT||request==IMPORT_GUEST_DISK;
        if(guestImport&&(result!=RESULT_OK||data==null||data.getData()==null))cancelGuestImport();
        if(guestImport&&result==RESULT_OK&&data!=null&&data.getData()!=null){Uri uri=data.getData();RuntimeExecutor.INSTANCE.execute(()->{
            String response;
            try(InputStream input=getContentResolver().openInputStream(uri)){
                if(request==IMPORT_GUEST_DISK)response=guest().importDisk(input).toString();
                else if(request==IMPORT_GUEST_CHECKPOINT)response=guest().importCheckpoint(input).toString();
                else if(request==IMPORT_LINUX_IMAGE)response=guest().importLinuxImage(input).toString();
                else{byte[] image=GuestStore.readElf(input);response=(request==IMPORT_GUEST_ELF?guest().create(image):guest().createSystem(image,1)).toString();}
            }
            catch(Exception error){response=failure(error);}
            final String outcome=response;ui.post(()->{if(!closed)web.evaluateJavascript("window.ReverseGuest&&window.ReverseGuest.imported("+outcome+");",null);});
        });}
        if(request==IMPORT_RXE&&result==RESULT_OK&&data!=null&&data.getData()!=null){Uri uri=data.getData();RuntimeExecutor.INSTANCE.execute(()->{
            String response;
            try(InputStream input=getContentResolver().openInputStream(uri)){JSONObject imported=programs().importImage(WorkbenchPrograms.read(input));response=imported.toString();Log.i("ReverseWorkbench","WORKBENCH_RXE_IMPORT_OK "+imported.getString("path"));}
            catch(Exception error){response=failure(error);message("Ejecutable rechazado: "+error.getMessage());}
            final String outcome=response;ui.post(()->{if(!closed)web.evaluateJavascript("window.ReversePrograms&&window.ReversePrograms.imported("+outcome+");",null);});
        });}
        if(request==PICK&&chooser!=null){chooser.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(result,data));chooser=null;}
        if(request==EXPORT){byte[] bytes=pendingExport;boolean guestExport=pendingGuestExport;pendingExport=null;pendingGuestExport=false;if(result==RESULT_OK&&(bytes!=null||guestExport)&&data!=null&&data.getData()!=null){Uri target=data.getData();RuntimeExecutor.INSTANCE.execute(()->{try(OutputStream stream=getContentResolver().openOutputStream(target,"wt")){if(stream==null)throw new IllegalStateException("Destino no disponible");if(guestExport)guest().exportTo(stream);else stream.write(bytes);message("Archivo exportado.");}catch(Exception error){message("No se pudo escribir el archivo.");}});}}
    }
    @Override public void onWindowFocusChanged(boolean focused){super.onWindowFocusChanged(focused);if(focused)immersive(this);}
    private void pauseGuestSession(){RuntimeExecutor.INSTANCE.execute(()->{try{if(guest!=null)guest.pause();}catch(Exception error){android.util.Log.e("ReverseGuest","Could not persist paused guest; resident state retained",error);}});}
    @Override protected void onPause(){activityActive=false;activityEpoch++;if(web!=null){web.evaluateJavascript("document.body&&(document.body.dataset.appPaused='true');window.ReverseGuest&&window.ReverseGuest.pause()",null);web.onPause();}pauseGuestSession();super.onPause();}
    @Override protected void onResume(){super.onResume();activityActive=true;immersive(this);if(web!=null){web.onResume();web.evaluateJavascript("document.body&&(document.body.dataset.appPaused='false');window.ReverseGuest&&window.ReverseGuest.resume()",null);}}
    @Override protected void onDestroy(){closed=true;pauseGuestSession();if(chooser!=null){chooser.onReceiveValue(null);chooser=null;}if(web!=null){web.removeJavascriptInterface("ReverseNative");web.destroy();}super.onDestroy();}
    // Used only by the in-repository Android instrumentation tests.
    public WebView testingWebView(){return web;}
}


