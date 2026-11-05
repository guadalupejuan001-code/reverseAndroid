package com.reversepc.os;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.InputType;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONObject;
import java.io.IOException;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Desktop is independent of the JNI engine at startup.
 * Native initialization, programs and RVPC data are loaded on a worker thread.
 * Even if the engine fails on a particular Android device, the user sees the
 * desktop plus a concrete diagnostic instead of an empty/black activity.
 */
public final class DesktopActivity extends Activity {
    private static final String LOG = "ReverseDesktop";
    private static final int EXPORT_RVPC=5112, IMPORT_RVPC=5113;
    private static final int BG=0xff131922, CARD=0xff222b36, CARD_BRIGHT=0xff303c47;
    private static final int WHITE=0xfff5f3ef, MUTED=0xffbdc3c8, ACCENT=0xffaec3d2;
    private static final int BLUE=0xffb4c6d4, VIOLET=0xffcbb9cb, AMBER=0xffdec4a6;
    private static final ExecutorService ENGINE=RuntimeExecutor.INSTANCE;
    private final ExecutorService engine=ENGINE;
    private final Handler clockHandler=new Handler(Looper.getMainLooper());
    private SessionStore store;
    private OrbitalWallpaper wallpaper;
    private TextView fpsLabel;
    private DesktopWindows windows;
    private FrameLayout windowLayer;
    private View desktopTop,desktopBar;
    private WebView browserView;
    private LinearLayout taskList;
    private android.content.SharedPreferences preferences;
    private WorkspaceFiles files;
    private String fileClipboard="",terminalDirectory="Programas";
    private boolean clipboardCut=false,wallpaperRunning=true;
    private final java.util.Map<String,Runnable> fileRefreshers=new java.util.HashMap<>();
    private final java.util.Map<String,Runnable> draftWriters=new java.util.HashMap<>();
    private static final int IMPORT_FILE=5120,EXPORT_FILE=5121,EXPORT_DESKTOP=5122;
    private static final int IMPORT_RXE=5140,EXPORT_APP_MEMORY=5141;
    private LinearLayout rxeShortcuts;
    private final java.util.Map<String,Runnable> rxeRefreshers=new java.util.HashMap<>();
    private java.util.List<RxeApp> rxeApps=java.util.Collections.emptyList();
    private boolean rxeLoading=false,rxeRefreshPending=false;
    private static final class RxeApp {
        final String path,title,abi,version;final int instructions;
        RxeApp(String path,JSONObject metadata){this.path=path;title=metadata.optString("title",path);instructions=metadata.optInt("instructions");abi=metadata.optString("abi");version=metadata.optString("app_version");}
    }
    private String engineFailure="";
    private TextView statusLabel, clockLabel, statusDetail;
    private final StringBuilder consoleHistory=new StringBuilder(
        "REVERSE TERMINAL   /   XI MACHINE\n"
        + "Sistema de comandos y ejecución real.\n"
        + "Escribe help para comenzar.\n\n");
    private boolean executing=false;
    private final Runnable tick=new Runnable() {
        @Override public void run(){
            if(clockLabel!=null)clockLabel.setText(
                new SimpleDateFormat("HH:mm",Locale.getDefault()).format(new Date()));
            clockHandler.postDelayed(this,60000L);
        }
    };

    private int dp(float value){
        return Math.round(value*getResources().getDisplayMetrics().density);
    }
    private GradientDrawable round(int color,int border,int radius){
        GradientDrawable d=new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(radius));
        if(border!=0)d.setStroke(dp(1),border);
        return d;
    }
    private TextView label(String value,int size,int color,boolean strong){
        TextView t=new TextView(this);
        t.setText(value);
        t.setTextColor(color);
        t.setTextSize(size);
        t.setTypeface(Typeface.create(strong?"sans-serif-medium":"sans-serif",Typeface.NORMAL));
        t.setIncludeFontPadding(true);
        return t;
    }
    private LinearLayout vertical(){
        LinearLayout v=new LinearLayout(this);
        v.setOrientation(LinearLayout.VERTICAL);
        return v;
    }
    private LinearLayout horizontal(){
        LinearLayout v=new LinearLayout(this);
        v.setOrientation(LinearLayout.HORIZONTAL);
        v.setGravity(Gravity.CENTER_VERTICAL);
        return v;
    }
    private void space(LinearLayout v,int size){
        View a=new View(this);
        v.addView(a,new LinearLayout.LayoutParams(1,dp(size)));
    }
    private TextView touch(String text,int size,boolean primary,Runnable action){
        TextView t=label(text,size,primary?BG:WHITE,true);
        t.setGravity(Gravity.CENTER);
        t.setBackground(round(primary?ACCENT:CARD_BRIGHT,primary?0:0xff45535f,9));
        t.setPadding(dp(15),dp(12),dp(15),dp(12));
        t.setClickable(true);
        t.setFocusable(true);
        t.setOnClickListener(v->action.run());
        return t;
    }
    private void margin(LinearLayout parent,View view,int top){
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);
        p.topMargin=dp(top);
        parent.addView(view,p);
    }
    private void rowWeight(LinearLayout parent,View view,float weight){
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-1,weight);
        p.setMargins(dp(4),0,dp(4),0);
        parent.addView(view,p);
    }
    private void info(String message){
        Toast.makeText(this,message,Toast.LENGTH_LONG).show();
    }

    @Override public void onCreate(Bundle previous) {
        super.onCreate(previous);
        if(getPackageName().endsWith(".workbench")){
            // Android Recents may restore the old 2.0 activity after an update.
            // Redirect before drawing its desktop or touching any saved state.
            Log.i(LOG,"WORKBENCH_LEGACY_ENTRY_REDIRECT");
            startActivity(new Intent(this,WorkbenchActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP|Intent.FLAG_ACTIVITY_SINGLE_TOP));
            finish();return;
        }
        getWindow().setStatusBarColor(BG);
        getWindow().setNavigationBarColor(BG);
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        preferences=getSharedPreferences("desktop-v1",MODE_PRIVATE);
        buildDesktop();
        clockHandler.post(tick);
        engine.execute(()->{
            try {
                files=new WorkspaceFiles(getFilesDir());
                SessionStore s=new SessionStore(getApplicationContext());
                seedRxe();
                seedBbyqExecutable();
                String stats=NativeRuntime.inspect(s.loadState());
                store=s;
                Log.i(LOG,"DESKTOP_CORE_READY "+stats);
                runOnUiThread(()->{
                    if(isFinishing()||isDestroyed())return;
                    statusLabel.setText("● Listo");
                    statusLabel.setTextColor(ACCENT);
                    statusDetail.setText("");
                    refreshRxe();
                    windowLayer.post(this::restoreDesktop);
                });
            }catch(Throwable fault){
                engineFailure=fault.getClass().getSimpleName()+": "+fault.getMessage();
                Log.e(LOG,"ENGINE_BOOT_FAILED",fault);
                runOnUiThread(()->{
                    if(isFinishing()||isDestroyed())return;
                    statusLabel.setText("● Error del núcleo");
                    statusLabel.setTextColor(0xffff9898);
                    statusDetail.setText("");
                });
            }
        });
    }

    // REVERSE OS: Windows/macOS-inspired desktop. The orbital shader is a
    // wallpaper only: menus, windows, data and apps retain conventional roles.
    private void buildDesktop(){
        FrameLayout root=new FrameLayout(this);
        root.setBackgroundColor(BG);
        root.addView(new DesktopBackdrop(),new FrameLayout.LayoutParams(-1,-1));

        try {
            wallpaper=new OrbitalWallpaper(this,new OrbitalWallpaper.Events(){
                @Override public void onFps(int frames,float scale){
                    if(fpsLabel!=null)fpsLabel.setText(wallpaperRunning?frames+" fps":"Pausa");
                }
                @Override public void onFailure(String error){
                    Log.e(LOG,"GPU fallback: "+error);
                    if(wallpaper!=null)wallpaper.setVisibility(View.GONE);
                    if(fpsLabel!=null)fpsLabel.setText("Fondo simple");
                }
            });
            root.addView(wallpaper,new FrameLayout.LayoutParams(-1,-1));
        } catch(Throwable error) {
            Log.e(LOG,"OpenGL wallpaper unavailable; retaining CPU fallback",error);
            wallpaper=null;
        }

        LinearLayout system=vertical();
        system.setPadding(dp(12),dp(3),dp(12),dp(0));
        root.addView(system,new FrameLayout.LayoutParams(-1,-1));
        root.setOnApplyWindowInsetsListener((view,insets)->{
            int nav=android.os.Build.VERSION.SDK_INT>=30
                ? insets.getInsets(android.view.WindowInsets.Type.navigationBars()).bottom
                : insets.getSystemWindowInsetBottom();
            int right=android.os.Build.VERSION.SDK_INT>=30
                ? insets.getInsets(android.view.WindowInsets.Type.navigationBars()).right
                : insets.getSystemWindowInsetRight();
            int left=android.os.Build.VERSION.SDK_INT>=30
                ? insets.getInsets(android.view.WindowInsets.Type.navigationBars()).left
                : insets.getSystemWindowInsetLeft();
            boolean ime=android.os.Build.VERSION.SDK_INT>=30
                ? insets.isVisible(android.view.WindowInsets.Type.ime())
                : insets.getSystemWindowInsetBottom()>dp(110);
            if(desktopTop!=null)desktopTop.setVisibility(ime?View.GONE:View.VISIBLE);
            if(desktopBar!=null)desktopBar.setVisibility(ime?View.GONE:View.VISIBLE);
            if(android.os.Build.VERSION.SDK_INT<30&&ime)nav=0;
            system.setPadding(Math.max(dp(12),left+dp(5)),dp(3),
                              Math.max(dp(12),right+dp(5)),nav+dp(5));
            return insets;
        });

        // Tiny unobtrusive top status bar. The OS looks like a desktop rather
        // than a mobile app with tab bars and cards.
        LinearLayout top=horizontal();desktopTop=top;
        TextView mark=label("REVERSE",16,WHITE,true);
        mark.setLetterSpacing(.07f);

        top.addView(mark,new LinearLayout.LayoutParams(0,-2,1f));
        if(getPackageName().endsWith(".workbench"))top.addView(touch("Mesa",11,false,()->{startActivity(new Intent(this,WorkbenchActivity.class).addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP|Intent.FLAG_ACTIVITY_SINGLE_TOP));finish();}));

        statusLabel=label("● Iniciando",10,AMBER,true);
        statusLabel.setOnClickListener(v->{
            if(!engineFailure.isEmpty())
                new AlertDialog.Builder(this).setTitle("REVERSE-PC")
                    .setMessage(engineFailure).setPositiveButton("CERRAR",null).show();
            else openMonitor();
        });
        top.addView(statusLabel);
        statusDetail=label("",10,MUTED,false);
        top.addView(statusDetail);
        LinearLayout.LayoutParams topP=new LinearLayout.LayoutParams(-1,dp(32));
        system.addView(top,topP);

        FrameLayout workspace=new FrameLayout(this);
        system.addView(workspace,new LinearLayout.LayoutParams(-1,0,1f));

        // The primary cosmic orb remains visible. Icons stay aligned to
        // traditional desktop columns on the left, not a "quantum menu".
        LinearLayout shortcutGrid=vertical();
        shortcutGrid.setPadding(dp(3),dp(12),dp(3),dp(2));
        rxeShortcuts=vertical();shortcutGrid.addView(rxeShortcuts);
        String[][] icons={{"▤","Archivos"},{"◎","Navegador"},{"▧","Editor"},
            {"⌘","XI Studio"},{">_","Terminal"},{"=","Calculadora"},
            {"◈","Ether Core"},{"⚙","Ajustes"},{"▦","Aplicaciones"},
            {"♧","Papelera"},{"▣","Ventanas"},{"B","BBY Q"}};
        for(int i=0;i<icons.length;i+=3){
            LinearLayout row=horizontal();
            for(int j=i;j<Math.min(i+3,icons.length);j++){
                final String app=icons[j][1];addDesktopShortcut(row,icons[j][0],app,()->launchApp(app));
            }
            shortcutGrid.addView(row);
        }
        ScrollView shortcuts=new ScrollView(this);shortcuts.setFillViewport(false);shortcuts.addView(shortcutGrid);
        workspace.addView(shortcuts,new FrameLayout.LayoutParams(dp(284),-1,Gravity.TOP|Gravity.LEFT));
        workspace.setOnLongClickListener(v->{desktopMenu(v);return true;});
        workspace.setOnContextClickListener(v->{desktopMenu(v);return true;});

        LinearLayout identity=vertical();
        identity.setGravity(Gravity.CENTER);
        TextView title=label("REVERSE",23,WHITE,true);
        title.setGravity(Gravity.CENTER);
        title.setLetterSpacing(.10f);

        identity.addView(title);
        TextView caption=label("Tu espacio de trabajo",11,MUTED,false);
        caption.setGravity(Gravity.CENTER);
        margin(identity,caption,7);
        FrameLayout.LayoutParams idP=new FrameLayout.LayoutParams(dp(280),dp(77),
                                                              Gravity.CENTER);
        idP.leftMargin=dp(45);
        workspace.addView(identity,idP);

        windowLayer=new FrameLayout(this);
        workspace.addView(windowLayer,new FrameLayout.LayoutParams(-1,-1));
        desktopBar=buildTaskbar();system.addView(desktopBar,new LinearLayout.LayoutParams(-1,dp(52)));
        windows=new DesktopWindows(this,windowLayer,taskList,preferences,this::updateWallpaper);
        updateWallpaper();
        setContentView(root);
        Log.i(LOG,"DESKTOP_SHOWN REVERSE OS MR REVERSE landscape");
    }

    // BBY Q is a native XI guest program, NOT a remote chatbot or a host
    // language model. This window merely transports user input and displays
    // the genuine result produced by XI on the shared Ether RVPC2 state.
    private void openBbyq(){openRxe("Aplicaciones/BBY-Q.rxe",true);}
    private void openBbyqLegacy(){
        WindowUi w=window("BBY Q anterior","Memoria compartida E009 · XI / Ether");
        if(w==null)return;
        TextView heading=label("BBY Q",21,WHITE,true);
        heading.setLetterSpacing(.05f);
        w.content.addView(heading);
        TextView modelStatus=label("Abriendo el organismo XI…",11,ACCENT,false);
        margin(w.content,modelStatus,3);
        ScrollView scroll=new ScrollView(this);
        scroll.setFillViewport(true);
        LinearLayout history=vertical();
        history.setPadding(dp(8),dp(10),dp(8),dp(10));
        history.setBackground(round(0xff19242d,0xff394957,8));
        TextView welcome=label("Lenguaje experimental XI. Pregunta con tres palabras, "
            +"por ejemplo: cuarto contiene llave?",12,MUTED,false);
        margin(history,welcome,5);
        scroll.addView(history);
        LinearLayout.LayoutParams transcript=new LinearLayout.LayoutParams(-1,0,1f);
        transcript.topMargin=dp(8);
        w.content.addView(scroll,transcript);

        LinearLayout inputRow=horizontal();
        EditText question=new EditText(this);
        question.setSingleLine(true);
        question.setTextSize(12);question.setTextColor(WHITE);
        question.setHintTextColor(MUTED);
        question.setHint("cuarto contiene llave?");
        question.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        question.setBackground(round(CARD_BRIGHT,0xff475969,8));
        question.setPadding(dp(11),dp(8),dp(10),dp(8));
        question.setEnabled(false);
        inputRow.addView(question,new LinearLayout.LayoutParams(0,dp(44),1f));
        TextView send=touch("ENVIAR",11,true,()->{
            final String prompt=question.getText().toString().trim();
            if(prompt.isEmpty())return;
            if(executing){info("XI está ocupado");return;}
            executing=true;
            question.setText("");
            addBbyqMessage(history,scroll,"TÚ · "+prompt,MUTED);
            modelStatus.setText("BBY Q está consultando Ether…");
            engine.execute(()->{
                try{
                    if(store==null)throw new IllegalStateException("REVERSE-PC no disponible");
                    BbyqBridge.Answer answer=BbyqBridge.ask(getApplicationContext(),store,prompt);
                    runOnUiThread(()->{
                        executing=false;
                        if(!w.dialog.isShowing())return;
                        addBbyqMessage(history,scroll,"BBY Q · "+answer.text,WHITE);
                        String next=answer.nextStudy==2 ? "Falta aprender una palabra." :
                            answer.nextStudy==1 ? "Falta evidencia sobre esa relación." :
                            "Respuesta respaldada por su conocimiento.";
                        modelStatus.setText(next);
                    });
                }catch(Throwable error){
                    Log.e(LOG,"BBYQ_ANSWER_FAILED",error);
                    runOnUiThread(()->{
                        executing=false;
                        if(!w.dialog.isShowing())return;
                        addBbyqMessage(history,scroll,"XI · "+error.getMessage(),0xffffa7a7);
                        modelStatus.setText("Consulta rechazada; memoria anterior conservada");
                    });
                }
            });
        });
        LinearLayout.LayoutParams sendParams=new LinearLayout.LayoutParams(dp(82),dp(44));
        sendParams.leftMargin=dp(7);
        inputRow.addView(send,sendParams);
        margin(w.content,inputRow,9);

        LinearLayout operations=horizontal();
        TextView teachWord=touch("+ PALABRA",10,false,()->bbyqTeachWord(modelStatus));
        operations.addView(teachWord,new LinearLayout.LayoutParams(0,dp(36),1f));
        TextView teachRelation=touch("+ RELACIÓN",10,false,()->bbyqTeachRelation(modelStatus));
        LinearLayout.LayoutParams teachRelationP=new LinearLayout.LayoutParams(0,dp(36),1f);
        teachRelationP.leftMargin=dp(7);
        operations.addView(teachRelation,teachRelationP);
        margin(w.content,operations,6);

        if(executing){modelStatus.setText("Otro programa XI está trabajando");return;}
        executing=true;
        engine.execute(()->{
            try{
                if(store==null)throw new IllegalStateException(
                    "Núcleo XI indisponible: "+engineFailure);
                BbyqBridge.Status status=BbyqBridge.install(getApplicationContext(),
                    store,message->runOnUiThread(()->{
                        if(w.dialog.isShowing())modelStatus.setText(message);
                    }));
                runOnUiThread(()->{
                    executing=false;
                    if(!w.dialog.isShowing())return;
                    modelStatus.setText(status.text());
                    question.setEnabled(true);
                    addBbyqMessage(history,scroll,
                        "XI · Modelo verificado en Ether. Puedes preguntar o enseñar.",ACCENT);
                });
            }catch(Throwable error){
                Log.e(LOG,"BBYQ_INSTALL_FAILED",error);
                runOnUiThread(()->{
                    executing=false;
                    if(!w.dialog.isShowing())return;
                    modelStatus.setText("Instalación XI no completada");
                    addBbyqMessage(history,scroll,"DIAGNÓSTICO · "+error.getMessage(),
                        0xffffa7a7);
                });
            }
        });
    }
    private void addBbyqMessage(LinearLayout history,ScrollView scroll,
                                String message,int color){
        TextView line=label(message,12,color,false);
        line.setPadding(dp(10),dp(8),dp(10),dp(8));
        line.setBackground(round(0xff27343e,0,6));
        margin(history,line,6);
        scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));
    }
    private EditText bbyqField(LinearLayout parent,String hint){
        EditText field=new EditText(this);
        field.setSingleLine(true);field.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);field.setTextSize(13);
        field.setTextColor(WHITE);field.setHintTextColor(MUTED);
        field.setHint(hint);
        field.setInputType(InputType.TYPE_CLASS_TEXT|
            InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        field.setBackground(round(0xff263541,0xff4c5962,8));
        field.setPadding(dp(12),dp(7),dp(12),dp(7));
        margin(parent,field,7);
        return field;
    }
    private void bbyqTeachWord(TextView status){
        LinearLayout fields=vertical();
        fields.setPadding(dp(18),dp(12),dp(18),dp(12));
        TextView notice=label("Nombre nuevo y tres señales observadas (0..3). "
            +"Es un vocabulario experimental, no un diccionario externo.",12,MUTED,false);
        fields.addView(notice);
        EditText word=bbyqField(fields,"palabra en minúsculas: mesa");
        EditText sensory=bbyqField(fields,"señales x,y,z: 1,2,3");
        new AlertDialog.Builder(this).setTitle("BBY Q · Aprender palabra")
            .setView(fields).setNegativeButton("CANCELAR",null)
            .setPositiveButton("ENSEÑAR",(dialog,which)->{
                if(executing){info("XI está ocupado");return;}
                executing=true;
                final String text=word.getText().toString().trim();
                final String triple=sensory.getText().toString().trim();
                status.setText("Aprendiendo en Ether…");
                engine.execute(()->{
                    try{
                        String[] values=triple.split(",",-1);
                        if(values.length!=3)throw new IllegalArgumentException(
                            "Necesito tres valores 0..3 separados por comas");
                        BbyqBridge.Status result=BbyqBridge.teachWord(
                            getApplicationContext(),store,text,
                            Integer.parseInt(values[0].trim()),
                            Integer.parseInt(values[1].trim()),
                            Integer.parseInt(values[2].trim()));
                        runOnUiThread(()->{
                            executing=false;status.setText(result.text());
                            info("Palabra aprendida y guardada");
                        });
                    }catch(Throwable error){
                        runOnUiThread(()->{
                            executing=false;
                            status.setText("No se guardó: "+error.getMessage());
                        });
                    }
                });
            }).show();
    }
    private void bbyqTeachRelation(TextView status){
        LinearLayout fields=vertical();
        fields.setPadding(dp(18),dp(12),dp(18),dp(12));
        fields.addView(label("Enseña una relación observada. "
            +"Los tres términos deben haber sido aprendidos.",12,MUTED,false));
        EditText subject=bbyqField(fields,"Sujeto: caja");
        EditText relation=bbyqField(fields,"Relación: contiene");
        EditText object=bbyqField(fields,"Objeto: bolsa");
        android.widget.CheckBox observed=new android.widget.CheckBox(this);
        observed.setText("La relación fue observada");
        observed.setTextColor(WHITE);observed.setChecked(true);
        margin(fields,observed,6);
        new AlertDialog.Builder(this).setTitle("BBY Q · Enseñar relación")
            .setView(fields).setNegativeButton("CANCELAR",null)
            .setPositiveButton("APRENDER",(dialog,which)->{
                if(executing){info("XI está ocupado");return;}
                executing=true;
                final String s=subject.getText().toString().trim();
                final String p=relation.getText().toString().trim();
                final String o=object.getText().toString().trim();
                final boolean truth=observed.isChecked();
                status.setText("Consolidando la evidencia XI…");
                engine.execute(()->{
                    try{
                        BbyqBridge.Status result=BbyqBridge.teachFact(
                            getApplicationContext(),store,s,p,o,truth);
                        runOnUiThread(()->{
                            executing=false;status.setText(result.text());
                            info("Evidencia aprendida en Ether");
                        });
                    }catch(Throwable error){
                        runOnUiThread(()->{
                            executing=false;
                            status.setText("No se guardó: "+error.getMessage());
                        });
                    }
                });
            }).show();
    }

    private void showStudio(){openStudio(0,null);}

    private void addDesktopShortcut(LinearLayout row,String glyph,String text,
                                    Runnable callback) {
        LinearLayout icon=vertical();
        icon.setGravity(Gravity.CENTER);
        icon.setPadding(dp(4),dp(6),dp(4),dp(6));
        icon.setBackground(round(0x00000000,0,10));
        icon.setClickable(true);
        icon.setFocusable(true);
        icon.setOnClickListener(v->callback.run());
        TextView sign=label(glyph,22,ACCENT,true);
        sign.setGravity(Gravity.CENTER);
        sign.setBackground(round(0xff2b3a48,0,12));
        icon.addView(sign,new LinearLayout.LayoutParams(dp(34),dp(34)));
        TextView name=label(text,11,WHITE,true);
        name.setGravity(Gravity.CENTER);
        name.setSingleLine(true);
        name.setEllipsize(android.text.TextUtils.TruncateAt.END);
        margin(icon,name,3);
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(dp(90),dp(62));
        p.setMargins(dp(1),dp(2),dp(1),dp(2));
        row.addView(icon,p);
    }

    private LinearLayout buildTaskbar(){
        LinearLayout bar=horizontal();bar.setPadding(dp(5),dp(4),dp(5),dp(4));
        bar.setBackground(round(0xff1c2631,0xff364651,8));
        TextView start=touch("R",20,true,this::openStartMenu);start.setContentDescription("INICIO");
        start.setPadding(0,0,0,0);bar.addView(start,new LinearLayout.LayoutParams(dp(45),-1));
        TextView search=touch("⌕",21,false,this::openSearch);search.setContentDescription("BUSCAR");
        search.setPadding(0,0,0,0);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(dp(44),-1);p.leftMargin=dp(5);bar.addView(search,p);
        TextView home=touch("▤",18,false,this::openFiles);home.setContentDescription("ABRIR ARCHIVOS");
        home.setPadding(0,0,0,0);LinearLayout.LayoutParams f=new LinearLayout.LayoutParams(dp(44),-1);f.leftMargin=dp(5);bar.addView(home,f);
        android.widget.HorizontalScrollView strip=new android.widget.HorizontalScrollView(this);strip.setHorizontalScrollBarEnabled(false);
        taskList=horizontal();strip.addView(taskList);bar.addView(strip,new LinearLayout.LayoutParams(0,-1,1));
        TextView desktop=touch("▣",16,false,()->windows.showDesktop());desktop.setContentDescription("MOSTRAR ESCRITORIO");
        desktop.setPadding(0,0,0,0);bar.addView(desktop,new LinearLayout.LayoutParams(dp(42),-1));
        fpsLabel=label("GPU",10,MUTED,false);fpsLabel.setPadding(dp(8),0,dp(8),0);bar.addView(fpsLabel);
        clockLabel=label("",12,WHITE,true);clockLabel.setPadding(dp(7),0,dp(7),0);clockLabel.setContentDescription("PANEL DEL SISTEMA");
        clockLabel.setOnClickListener(v->openSystemPanel());bar.addView(clockLabel);
        TextView power=touch("⋮",19,false,this::openPower);power.setPadding(0,0,0,0);power.setContentDescription("ENCENDIDO");bar.addView(power,new LinearLayout.LayoutParams(dp(39),-1));
        return bar;
    }

    private void addTaskButton(LinearLayout taskbar,String symbol,String title,
                               Runnable callback) {
        LinearLayout item=vertical();
        item.setGravity(Gravity.CENTER);
        TextView glyph=label(symbol,20,ACCENT,true);
        glyph.setGravity(Gravity.CENTER);
        item.addView(glyph);
        TextView sub=label(title,9,WHITE,false);
        sub.setGravity(Gravity.CENTER);
        item.addView(sub);
        item.setOnClickListener(v->callback.run());
        item.setContentDescription(title);
        taskbar.addView(item,new LinearLayout.LayoutParams(dp(65),-1));
    }

    private void openHome(){
        WindowUi w=window("HOME","REVERSE / PERSONAL COMPUTER");
        if(w==null)return;
        LinearLayout header=vertical();
        header.setGravity(Gravity.CENTER);
        TextView headline=label("Bienvenido a REVERSE",22,WHITE,true);
        headline.setLetterSpacing(.01f);
        headline.setGravity(Gravity.CENTER);

        header.addView(headline);
        TextView welcome=label("Tus archivos, programas y herramientas.",12,MUTED,false);
        welcome.setGravity(Gravity.CENTER);
        margin(header,welcome,7);
        w.content.addView(header,new LinearLayout.LayoutParams(-1,0,1f));
        LinearLayout buttons=horizontal();
        buttons.addView(touch("Terminal",12,false,()->{
            w.dialog.dismiss();openTerminal();
        }),new LinearLayout.LayoutParams(0,dp(43),1f));
        TextView studio=touch("XI Studio",12,true,()->{
            w.dialog.dismiss();openStudio(0,null);
        });
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(43),1f);
        p.leftMargin=dp(9);
        buttons.addView(studio,p);
        w.content.addView(buttons);
    }

    private static final String[] APPLICATIONS={"Archivos","Navegador","Editor","Calculadora","XI Studio","Terminal","Aplicaciones","Papelera","Ventanas","Ether Core","Ajustes","BBY Q","BBY Q anterior"};
    private void launchApp(String app){
        switch(app){
            case "Archivos":openFiles();break;case "Navegador":openBrowser();break;
            case "Editor":newDocument("Documentos");break;case "Calculadora":openCalculator();break;
            case "XI Studio":openStudio(0,null);break;case "Terminal":openTerminal();break;
            case "Aplicaciones":openAndroidApps();break;case "Papelera":openFiles("Papelera");break;
            case "Ventanas":openTasks();break;case "Ether Core":openMonitor();break;
            case "Ajustes":openSettings();break;case "BBY Q":openBbyq();break;
            case "BBY Q anterior":openBbyqLegacy();break;
        }
    }
    private void restoreDesktop(){
        if(isDestroyed()||windows==null||!windows.all().isEmpty())return;
        final String previousFocus=preferences.getString("focused","");
        for(String key:windows.savedKeys()){
            if(key.startsWith("xi:"))openStudio(0,key.substring(3));
            else if(key.startsWith("text:"))openText(key.substring(5));
            else if(key.startsWith("image:"))openImage(key.substring(6));
            else if(key.startsWith("files:"))openFiles(key.substring(6));
            else if(key.startsWith("rxe:"))openRxe(key.substring(4),false);
            else if(key.equals("Android"))openHostApps();
            else if(java.util.Arrays.asList(APPLICATIONS).contains(key)&&!key.equals("Editor"))launchApp(key);
        }
        if(!previousFocus.isEmpty())windows.focusExisting(previousFocus);
    }
    private void updateWallpaper(){
        if(wallpaper==null||preferences==null)return;
        wallpaper.quality(preferences.getInt("quality",0));
        boolean run=preferences.getBoolean("motion",true)&&(windows==null||!windows.coversDesktop());
        if(run!=wallpaperRunning){wallpaperRunning=run;wallpaper.motionEnabled(run);}
        if(!run&&fpsLabel!=null)fpsLabel.setText("Pausa");
    }
    private void desktopMenu(View anchor){
        android.widget.PopupMenu menu=new android.widget.PopupMenu(this,anchor);
        for(String name:new String[]{"Nueva nota","Archivos","Ventanas","Mostrar escritorio","Ajustes"})menu.getMenu().add(name);
        menu.setOnMenuItemClickListener(item->{String name=item.getTitle().toString();
            if(name.equals("Nueva nota"))newDocument("Escritorio");else if(name.equals("Mostrar escritorio"))windows.showDesktop();else launchApp(name);return true;});menu.show();
    }
    private void openStartMenu(){
        WindowUi w=window("Inicio","Tus aplicaciones");if(w==null)return;
        ScrollView scroll=new ScrollView(this);LinearLayout list=vertical();scroll.addView(list);
        for(int i=0;i<APPLICATIONS.length;i+=3){
            LinearLayout row=horizontal();
            for(int j=i;j<Math.min(i+3,APPLICATIONS.length);j++){
                final String app=APPLICATIONS[j];TextView b=touch(app,12,false,()->{w.dialog.dismiss();launchApp(app);});b.setPadding(dp(3),0,dp(3),0);
                LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(41),1);p.setMargins(dp(3),dp(3),dp(3),dp(3));row.addView(b,p);
            }
            list.addView(row);
        }
        for(RxeApp app:rxeApps)if(isRxePinned(app.path))margin(list,touch(app.title+" · RXE",12,false,()->{w.dialog.dismiss();openRxe(app.path,true);}),5);
        w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        margin(w.content,touch("Apagar / Reiniciar",11,false,this::openPower),8);
    }
    private void openSearch(){
        WindowUi w=window("Buscar","Aplicaciones y documentos");if(w==null)return;
        EditText field=bbyqField(w.content,"Busca una aplicación o un archivo");
        field.setContentDescription("BUSQUEDA GLOBAL");
        ScrollView scroll=new ScrollView(this);LinearLayout list=vertical();scroll.addView(list);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        Runnable refresh=()->{
            String q=field.getText().toString().toLowerCase(Locale.ROOT).trim();list.removeAllViews();
            for(String app:APPLICATIONS)if(app.toLowerCase(Locale.ROOT).contains(q))margin(list,touch(app,12,false,()->{w.dialog.dismiss();launchApp(app);}),5);
            if(!q.isEmpty())for(RxeApp app:rxeApps)if((app.title+" "+app.path).toLowerCase(Locale.ROOT).contains(q)){
                TextView hit=touch(app.title+" · RXE",12,false,()->{w.dialog.dismiss();openRxe(app.path,true);});hit.setContentDescription("RESULTADO RXE "+app.path);margin(list,hit,5);
            }
            if(files!=null&&!q.isEmpty())engine.execute(()->{
                try{java.util.ArrayList<WorkspaceFiles.Entry> hits=new java.util.ArrayList<>();searchFiles("",q,hits,0);
                    runOnUiThread(()->{if(!w.dialog.isShowing()||!field.getText().toString().toLowerCase(Locale.ROOT).trim().equals(q))return;
                        for(WorkspaceFiles.Entry e:hits)margin(list,touch(e.path,11,false,()->{w.dialog.dismiss();if(e.directory)openFiles(e.path);else openFile(e.path);}),4);});
                }catch(Exception ex){Log.w(LOG,"Search",ex);}
            });
        };
        field.addTextChangedListener(watcher(refresh));refresh.run();
    }
    private void searchFiles(String path,String q,List<WorkspaceFiles.Entry> hits,int depth)throws IOException{
        if(depth>6||hits.size()>=40||path.equals("Papelera"))return;
        for(WorkspaceFiles.Entry entry:files.list(path)){
            if(entry.name.toLowerCase(Locale.ROOT).contains(q)){hits.add(entry);if(hits.size()>=40)return;}
            if(entry.directory)searchFiles(entry.path,q,hits,depth+1);
        }
    }
    private android.text.TextWatcher watcher(Runnable action){return new android.text.TextWatcher(){
        public void beforeTextChanged(CharSequence s,int st,int c,int a){}public void onTextChanged(CharSequence s,int st,int b,int c){}
        public void afterTextChanged(android.text.Editable e){action.run();}
    };}

    private void addTile(LinearLayout row,String glyph,String name,String subtitle,int color,Runnable open){
        LinearLayout cell=vertical();
        cell.setGravity(Gravity.CENTER_VERTICAL);
        cell.setPadding(dp(9),dp(7),dp(6),dp(7));
        cell.setBackground(round(0xe9142333,0xff2a3c4b,20));
        cell.setClickable(true);
        cell.setFocusable(true);
        cell.setOnClickListener(v->open.run());

        TextView symbol=label(glyph,19,color,true);
        symbol.setGravity(Gravity.CENTER);
        symbol.setBackground(round(0xff233549,0,13));
        cell.addView(symbol,new LinearLayout.LayoutParams(dp(35),dp(35)));
        TextView title=label(name,11,WHITE,true);
        title.setSingleLine(true);
        title.setEllipsize(android.text.TextUtils.TruncateAt.END);
        margin(cell,title,5);
        TextView caption=label(subtitle,9,MUTED,false);
        caption.setSingleLine(true);
        caption.setEllipsize(android.text.TextUtils.TruncateAt.END);
        margin(cell,caption,1);
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-1,1f);
        p.setMargins(dp(4),0,dp(4),0);
        row.addView(cell,p);
    }
    private LinearLayout buildDock(){
        LinearLayout dock=horizontal();
        dock.setGravity(Gravity.CENTER);
        dock.setPadding(dp(8),dp(6),dp(8),dp(6));
        dock.setBackground(round(0xee122334,0xff53616d,23));
        addDock(dock,"⌂","HOME",ACCENT,()->{});
        addDock(dock,"⌘","XI",WHITE,()->openStudio(0,null));
        addDock(dock,">_","TERM",WHITE,this::openTerminal);
        addDock(dock,"▤","Archivos",WHITE,this::openFiles);
        addDock(dock,"◎","WEB",WHITE,this::openBrowser);
        addDock(dock,"◈","CORE",WHITE,this::openMonitor);
        return dock;
    }
    private void addDock(LinearLayout dock,String glyph,String name,int color,Runnable open){
        LinearLayout item=vertical();
        item.setGravity(Gravity.CENTER);
        TextView icon=label(glyph,21,color,true);
        icon.setGravity(Gravity.CENTER);
        item.addView(icon);
        TextView sub=label(name,9,color==ACCENT?ACCENT:MUTED,true);
        sub.setGravity(Gravity.CENTER);
        item.addView(sub);
        item.setClickable(true);
        item.setFocusable(true);
        item.setOnClickListener(v->open.run());
        dock.addView(item,new LinearLayout.LayoutParams(0,-1,1f));
    }

    private static final class WindowUi {
        final DesktopWindows.Handle dialog;
        final LinearLayout content;
        WindowUi(DesktopWindows.Handle handle){dialog=handle;content=handle.content;}
    }
    private WindowUi window(String title,String subtitle){return window(title,subtitle,title);}
    private WindowUi window(String title,String subtitle,String key){
        if(windows.focusExisting(key))return null;
        if(windows.all().size()>=12){info("Hay 12 ventanas abiertas. Cierra una para continuar.");return null;}
        return new WindowUi(windows.open(key,title,subtitle));
    }

    private void openAbout(){
        WindowUi w=window("ABOUT","Acerca de REVERSE OS");
        if(w==null)return;
        TextView text=label("REVERSE OS 1.0\n\n"+
            "Escritorio horizontal con ventanas funcionales, GPU GLES 3 "+
            "de REVERSE Space Glitch y la computadora XI/Ether.\n\n"+
            "El fondo orbital representa cálculos gráficos en tiempo real; "+
            "no es prueba de aceleración cuántica.\n\n"+
            "Android sigue siendo el sistema operativo anfitrión; "+
            "XI ejecuta programas y conserva estados RVPC2. "+
            "Chromium utiliza Android WebView.",13,WHITE,false);
        margin(w.content,text,9);
        TextView button=touch("ABRIR ETHER CORE",12,true,()->{
            w.dialog.dismiss();openMonitor();
        });
        margin(w.content,button,17);
    }

    private void appError(LinearLayout parent,String message){
        TextView t=label(message,13,0xffffa5a5,false);
        t.setPadding(dp(12),dp(15),dp(12),dp(15));
        parent.addView(t);
    }
    // A real program editor inside a floating window; JNI remains the one
    // authority for execution. A failed program never replaces RVPC2.
    private void openStudio(int tab,String file){
        final String chosen;
        if(tab==2)chosen="nuevo_"+System.currentTimeMillis()+".xi";
        else chosen=file==null?"01_euclides.xi":SessionStore.normalized(file);
        WindowUi w=window("XI Studio","Editor de programas de REVERSE-PC","xi:"+chosen);
        if(w==null)return;
        TextView filename=label(chosen+"   •  XI SCRIPT",11,ACCENT,true);
        filename.setSingleLine(true);filename.setEllipsize(android.text.TextUtils.TruncateAt.END);
        LinearLayout sourceHeader=horizontal();sourceHeader.addView(filename,new LinearLayout.LayoutParams(0,-2,1));margin(w.content,sourceHeader,1);

        EditText editor=new EditText(this);
        editor.setContentDescription("CODIGO XI");
        editor.setTypeface(Typeface.MONOSPACE);
        editor.setTextColor(WHITE);editor.setHintTextColor(MUTED);
        editor.setTextSize(12);
        editor.setGravity(Gravity.TOP|Gravity.START);
        editor.setSingleLine(false);editor.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        editor.setInputType(InputType.TYPE_CLASS_TEXT|
            InputType.TYPE_TEXT_FLAG_MULTI_LINE|
            InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        editor.setBackground(round(0xff1b2531,0xff4a5b69,9));
        editor.setPadding(dp(12),dp(8),dp(12),dp(8));
        editor.setText(tab==2?"#!reverse-pc-machine-v1\nset r0 1\nprint r0\nhalt\n":
            "#!reverse-pc-machine-v1\n# Cargando...\n");
        TextView build=touch("CREAR .RXE",10,true,()->createRxe(editor.getText().toString(),chosen.substring(0,chosen.length()-3)+".rxe"));build.setPadding(dp(5),0,dp(5),0);sourceHeader.addView(build,new LinearLayout.LayoutParams(dp(100),dp(28)));
        String draftKey="draft:xi:"+chosen;
        final String[] baseline={""};final boolean[] ready={tab==2};
        Runnable persistDraft=()->{if(!ready[0])return;String value=editor.getText().toString();
            if(value.equals(baseline[0]))preferences.edit().remove(draftKey).apply();else preferences.edit().putString(draftKey,value).apply();};
        draftWriters.put(w.dialog.key,persistDraft);
        Runnable deferredDraft=persistDraft;
        editor.addTextChangedListener(watcher(()->{if(ready[0]){clockHandler.removeCallbacks(deferredDraft);clockHandler.postDelayed(deferredDraft,400);}}));
        w.dialog.setOnDismissListener(h->{clockHandler.removeCallbacks(deferredDraft);persistDraft.run();draftWriters.remove(h.key);});
        LinearLayout.LayoutParams editP=new LinearLayout.LayoutParams(-1,0,1f);
        editP.topMargin=dp(8);
        w.content.addView(editor,editP);

        TextView output=label("Estado: preparando editor...",11,MUTED,false);
        output.setTypeface(Typeface.MONOSPACE);
        output.setMaxLines(3);
        margin(w.content,output,7);
        LinearLayout buttons=horizontal();
        TextView save=touch("GUARDAR",11,false,()->{
            final String data=editor.getText().toString();
            engine.execute(()->{
                try{
                    if(store==null)throw new IllegalStateException("Núcleo no disponible");
                    store.saveProgram(chosen,data);
                    runOnUiThread(()->{baseline[0]=data;persistDraft.run();output.setText("Guardado: "+chosen);});
                }catch(Throwable e){
                    runOnUiThread(()->output.setText("Error: "+e.getMessage()));
                }
            });
        });
        w.dialog.saveAction=save::performClick;
        buttons.addView(save,new LinearLayout.LayoutParams(0,dp(42),1f));
        TextView run=touch("▶ EJECUTAR",12,true,()->{
            if(executing){info("XI está ejecutando otro programa");return;}
            executing=true;
            final String data=editor.getText().toString();
            output.setText("EJECUTANDO "+chosen+"...");
            engine.execute(()->{
                try{
                    if(store==null)throw new IllegalStateException(
                        "Núcleo XI no disponible: "+engineFailure);
                    store.saveProgram(chosen,data);
                    long t0=SystemClock.elapsedRealtimeNanos();
                    ResultPacket packet=ResultPacket.parse(NativeRuntime.execute(
                        store.loadState(),data));
                    store.writeState(packet.state);
                    long elapsed=SystemClock.elapsedRealtimeNanos()-t0;
                    final String result=pretty(packet.output)+
                        String.format(Locale.US,"RVPC2 guardado  •  %.2f ms",
                                      elapsed/1000000.0);
                    Log.i("ReverseOS","RUN_OK "+chosen+" output="+packet.output.length());
                    runOnUiThread(()->{
                        executing=false;
                        baseline[0]=data;persistDraft.run();output.setText(result);
                        consoleHistory.append("$ run "+chosen+"\n"+result+"\n");
                    });
                }catch(Throwable error){
                    Log.e(LOG,"XI studio run failed",error);
                    runOnUiThread(()->{
                        executing=false;
                        output.setText("ERROR: "+error.getMessage()+
                            "\nEstado RVPC2 anterior conservado");
                    });
                }
            });
        });
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(42),1f);
        p.leftMargin=dp(8);
        buttons.addView(run,p);
        TextView open=touch("ABRIR",11,false,()->{
            if(store==null){info("Leyendo programas...");return;}
            List<String> programs=store.listPrograms();
            if(programs.isEmpty()){info("No hay programas");return;}
            new AlertDialog.Builder(this)
                .setTitle("Abrir programa XI")
                .setItems(programs.toArray(new String[0]),(d,pos)->{
                    w.dialog.dismiss();openStudio(0,programs.get(pos));
                }).show();
        });
        LinearLayout.LayoutParams opener=new LinearLayout.LayoutParams(dp(73),dp(42));
        opener.leftMargin=dp(8);
        buttons.addView(open,opener);
        margin(w.content,buttons,6);

        engine.execute(()->{
            if(tab==2){runOnUiThread(()->output.setText("Nuevo programa · borrador conservado al cerrar"));return;}
            try{
                if(store==null)throw new IllegalStateException("Núcleo no disponible");
                String loaded;
                try{loaded=store.readProgram(chosen);}catch(IOException e){if(!preferences.contains(draftKey))throw e;loaded="";}
                final String code=loaded;
                runOnUiThread(()->{
                    if(!w.dialog.isShowing())return;
                    baseline[0]=code;editor.setText(preferences.getString(draftKey,code));ready[0]=true;
                    output.setText("Listo: "+chosen+" · editando programa XI");
                });
            }catch(Throwable error){
                runOnUiThread(()->output.setText("Error: "+error.getMessage()));
            }
        });
    }

    // Terminal executes programs through the exact JNI bridge used by the original GUI.
    private void openTerminal(){
        WindowUi w=window("Terminal","XI SHELL  •  ejecución nativa");
        if(w==null)return;
        ScrollView scroll=new ScrollView(this);
        TextView output=label(consoleHistory.toString(),12,0xffdddacb,false);
        output.setTypeface(Typeface.MONOSPACE);
        output.setPadding(dp(12),dp(13),dp(12),dp(13));
        output.setBackground(round(0xff171f29,0xff425160,14));
        scroll.addView(output);
        w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1f));
        space(w.content,11);

        LinearLayout inputLine=horizontal();
        EditText command=new EditText(this);
        command.setSingleLine(true);command.setTextColor(WHITE);command.setTextSize(14);
        command.setHintTextColor(MUTED);command.setHint("help / ls / run 01_euclides.xi");
        command.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        command.setBackground(round(CARD_BRIGHT,0xff304657,10));
        command.setPadding(dp(10),dp(10),dp(10),dp(10));
        inputLine.addView(command,new LinearLayout.LayoutParams(0,dp(52),1f));
        TextView send=touch("RUN",12,true,()->{
            String entry=command.getText().toString().trim();
            command.setText("");
            terminal(entry,w,output,scroll);
        });
        LinearLayout.LayoutParams sendParams=new LinearLayout.LayoutParams(dp(67),dp(52));
        sendParams.leftMargin=dp(8);
        inputLine.addView(send,sendParams);
        w.content.addView(inputLine);
        TextView help=label("help · bbyq · ls · run nombre.xi · core · edit nombre.xi",10,MUTED,false);
        margin(w.content,help,12);
        scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));
    }
    private void putConsole(String data,TextView output,ScrollView scroll){
        consoleHistory.append(data);
        if(consoleHistory.length()>45000)consoleHistory.delete(0,9000);
        output.setText(consoleHistory.toString());
        scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));
    }
    private String pretty(String raw){
        if(raw.trim().isEmpty())return "(Programa completado sin salida)\n";
        StringBuilder formatted=new StringBuilder();
        for(String line:raw.split("\n")){
            if(line.trim().isEmpty())continue;
            try{
                JSONObject json=new JSONObject(line);
                if("register".equals(json.optString("type"))){
                    formatted.append(json.optString("register")).append(" = ")
                             .append(json.optString("value")).append("\n");
                }else formatted.append(line).append("\n");
            }catch(Exception e){formatted.append(line).append('\n');}
        }
        return formatted.toString();
    }
    private void terminal(String input,WindowUi w,TextView output,ScrollView scroll){
        if(input.isEmpty())return;
        if(input.equalsIgnoreCase("clear")){
            consoleHistory.setLength(0);output.setText("");return;
        }
        putConsole("reverse@xi ~> "+input+"\n",output,scroll);
        String[] words=input.split("\\s+",2);
        String cmd=words[0].toLowerCase(Locale.ROOT);
        String arg=words.length==2?words[1].trim():"";
        if(cmd.equals("help")){
            putConsole("help : comandos\nls : listar programas\n"
                +"run programa.xi : ejecutar en Ether\ncore : estado nativo\n"
                +"edit programa.xi : abrir estudio XI\nbbyq : abrir BBY Q\n"
                +"clear : limpiar consola\n\n",output,scroll);
        }else if(cmd.equals("bbyq")){
            w.dialog.dismiss();openBbyq();
        }else if(cmd.equals("edit")){
            if(arg.isEmpty()){putConsole("Falta nombre de archivo.\n",output,scroll);return;}
            w.dialog.dismiss();openStudio(0,SessionStore.normalized(arg));
        }else if(cmd.equals("ls")||cmd.equals("core")||cmd.equals("run")){
            if(executing){putConsole("Ya hay una ejecución en curso.\n",output,scroll);return;}
            executing=true;
            engine.execute(()->{
                String result;
                try{
                    if(store==null)throw new IllegalStateException("Núcleo no disponible: "+engineFailure);
                    if(cmd.equals("ls")){
                        List<String> files=store.listPrograms();
                        StringBuilder b=new StringBuilder();
                        for(String f:files)b.append("  ").append(f).append('\n');
                        result=b.toString();
                    }else if(cmd.equals("core")){
                        result=NativeRuntime.inspect(store.loadState())+"\n";
                    }else{
                        String name=SessionStore.normalized(arg);
                        String program=store.readProgram(name);
                        long start=SystemClock.elapsedRealtimeNanos();
                        ResultPacket payload=ResultPacket.parse(
                            NativeRuntime.execute(store.loadState(),program));
                        store.writeState(payload.state);
                        long ns=SystemClock.elapsedRealtimeNanos()-start;
                        result=pretty(payload.output)
                             +String.format(Locale.US,"[PASS] %.2f ms / RVPC2 guardado\n",ns/1e6);
                        Log.i("ReverseOS","DESKTOP_RUN_OK "+name+" output="+payload.output.length());
                    }
                }catch(Throwable error){
                    result="[ERROR] "+error.getClass().getSimpleName()+": "
                        +error.getMessage()+"\nSin cambios aplicados al estado si XI falló.\n";
                    Log.e(LOG,"Terminal command failed",error);
                }
                final String text=result;
                runOnUiThread(()->{
                    executing=false;
                    putConsole(text+"\n",output,scroll);
                });
            });
        }else putConsole("Comando no reconocido. Escribe help.\n\n",output,scroll);
    }

    private void openFiles(){openFiles("");}
    private void openFiles(String initial){
        if(files==null){info("Los archivos se están preparando");return;}
        WindowUi w=window("Archivos","Carpetas y documentos","files:"+initial);if(w==null)return;
        final String[] folder={preferences.getString("folder:"+w.dialog.key,initial)};
        TextView address=label("",12,ACCENT,true);address.setContentDescription("RUTA DE ARCHIVOS");w.content.addView(address);
        LinearLayout toolbar=horizontal();w.content.addView(toolbar);
        ScrollView scroll=new ScrollView(this);LinearLayout list=vertical();scroll.addView(list);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        TextView status=label("",10,MUTED,false);margin(w.content,status,4);
        final Runnable[] refresh=new Runnable[1];
        refresh[0]=()->{
            preferences.edit().putString("folder:"+w.dialog.key,folder[0]).apply();address.setText("REVERSE / "+folder[0]);list.removeAllViews();status.setText("Leyendo…");
            engine.execute(()->{try{
                List<WorkspaceFiles.Entry> entries=files.list(folder[0]);
                runOnUiThread(()->{if(!w.dialog.isShowing())return;status.setText(entries.size()+" elementos");
                    for(WorkspaceFiles.Entry e:entries){
                        LinearLayout row=horizontal();row.setPadding(dp(7),dp(2),dp(7),dp(2));
                        TextView name=label((e.directory?"▤  ":e.path.toLowerCase(Locale.ROOT).endsWith(".rxe")?"RX  ":"▧  ")+e.name,12,WHITE,e.directory);name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.END);
                        name.setPadding(dp(5),dp(10),dp(5),dp(10));name.setContentDescription("ARCHIVO "+e.name);
                        Runnable open=()->{if(folder[0].equals("Papelera"))fileMenu(e,folder[0],name,refresh[0]);else if(e.directory){folder[0]=e.path;refresh[0].run();}else openFile(e.path);};
                        name.setOnClickListener(v->open.run());name.setOnLongClickListener(v->{fileMenu(e,folder[0],name,refresh[0]);return true;});
                        row.addView(name,new LinearLayout.LayoutParams(0,-2,1));
                        TextView size=label(e.directory?"Carpeta":e.bytes+" B",10,MUTED,false);row.addView(size);
                        TextView menu=touch("⋮",15,false,()->fileMenu(e,folder[0],name,refresh[0]));menu.setPadding(0,0,0,0);menu.setContentDescription("OPCIONES "+e.name);row.addView(menu,new LinearLayout.LayoutParams(dp(38),dp(36)));
                        margin(list,row,3);
                    }
                });
            }catch(Exception ex){runOnUiThread(()->status.setText("No se pudo abrir: "+ex.getMessage()));}});
        };
        String[] labels={"↑","Actualizar","+ Carpeta","+ Texto","Importar","Pegar"};
        Runnable[] actions={()->{folder[0]=WorkspaceFiles.parent(folder[0]);refresh[0].run();},refresh[0],
            ()->promptName("Nueva carpeta","Carpeta",name->fileWork(()->files.mkdir(WorkspaceFiles.join(folder[0],name)),refresh[0])),
            ()->newDocument(folder[0].isEmpty()?"Documentos":folder[0]),
            ()->importFile(folder[0]),()->fileWork(()->{if(fileClipboard.isEmpty())throw new IOException("Primero copia o corta un archivo");files.paste(fileClipboard,folder[0],clipboardCut);if(clipboardCut)fileClipboard="";},refresh[0])};
        for(int i=0;i<labels.length;i++){final Runnable action=actions[i];TextView b=touch(labels[i],10,false,action);b.setPadding(dp(2),0,dp(2),0);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(35),1);p.setMargins(0,dp(4),dp(3),dp(4));toolbar.addView(b,p);}
        fileRefreshers.put(w.dialog.key,refresh[0]);
        w.dialog.setOnDismissListener(h->fileRefreshers.remove(h.key));
        refresh[0].run();
    }
    private interface FileAction {void run()throws Exception;}
    private void fileWork(FileAction action,Runnable done){engine.execute(()->{try{action.run();runOnUiThread(()->{if(isDestroyed())return;info("Listo");for(Runnable refresh:new java.util.ArrayList<>(fileRefreshers.values()))if(refresh!=done)refresh.run();if(done!=null)done.run();});}catch(Exception ex){runOnUiThread(()->info(ex.getMessage()));}});}
    private void promptName(String title,String defaultName,java.util.function.Consumer<String> action){
        if(isFinishing()||isDestroyed())return;
        EditText field=new EditText(this);field.setContentDescription("NOMBRE DE ARCHIVO");field.setSingleLine(true);field.setText(defaultName);field.selectAll();
        AlertDialog d=new AlertDialog.Builder(this).setTitle(title).setView(field).setNegativeButton("Cancelar",null).setPositiveButton("Aceptar",null).create();d.show();
        d.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{try{String name=WorkspaceFiles.name(field.getText().toString());action.accept(name);d.dismiss();}catch(Exception ex){field.setError(ex.getMessage());}});
    }
    private void fileMenu(WorkspaceFiles.Entry entry,String folder,View anchor,Runnable refresh){
        android.widget.PopupMenu menu=new android.widget.PopupMenu(this,anchor);
        if(folder.equals("Papelera")){menu.getMenu().add("Restaurar");menu.getMenu().add("Eliminar definitivamente");}
        else if(!WorkspaceFiles.parent(entry.path).isEmpty())for(String option:entry.directory?new String[]{"Cortar","Renombrar","A la papelera"}:new String[]{"Copiar","Cortar","Renombrar","Exportar","A la papelera"})menu.getMenu().add(option);
        else menu.getMenu().add("Abrir");
        menu.setOnMenuItemClickListener(item->{switch(item.getTitle().toString()){
            case "Abrir":if(entry.directory)openFiles(entry.path);else openFile(entry.path);break;
            case "Copiar":fileClipboard=entry.path;clipboardCut=false;info("Copiado. Abre el destino y pulsa Pegar.");break;
            case "Cortar":fileClipboard=entry.path;clipboardCut=true;info("Seleccionado para mover");break;
            case "Renombrar":promptName("Renombrar",entry.name,n->fileWork(()->files.rename(entry.path,n),refresh));break;
            case "Exportar":if(entry.directory)info("Abre la carpeta para exportar un archivo");else exportFile(entry.path);break;
            case "A la papelera":new AlertDialog.Builder(this).setTitle("Mover a la papelera").setMessage(entry.name).setNegativeButton("Cancelar",null).setPositiveButton("Mover",(d,n)->fileWork(()->files.trash(entry.path),refresh)).show();break;
            case "Restaurar":fileWork(()->files.restore(entry.path),refresh);break;
            case "Eliminar definitivamente":new AlertDialog.Builder(this).setTitle("Eliminar definitivamente").setMessage(entry.name+" se eliminará de la papelera.").setNegativeButton("Cancelar",null).setPositiveButton("Eliminar",(d,n)->fileWork(()->files.eraseTrash(entry.path),refresh)).show();break;
        }return true;});menu.show();
    }
    private void newDocument(String folder){
        if(files==null){info("Preparando archivos…");return;}
        if(folder.equals("Programas")){openStudio(2,null);return;}
        if(folder.equals("Papelera"))folder="Documentos";
        final String destination=folder;
        promptName("Nuevo documento","Nota "+new SimpleDateFormat("HHmmss",Locale.ROOT).format(new Date())+".txt",name->{
            try{String path=WorkspaceFiles.join(destination,name);fileWork(()->files.write(path,new byte[0],false),()->openText(path));}catch(IOException e){info(e.getMessage());}
        });
    }
    private void openFile(String path){
        String ext=path.toLowerCase(Locale.ROOT);
        if(ext.endsWith(".rxe")){openRxe(path,true);return;}
        if(path.startsWith("Programas/")&&ext.endsWith(".xi")){openStudio(0,path.substring(10));return;}
        if(ext.matches(".*\\.(png|jpe?g|webp|gif|bmp)$")){openImage(path);return;}
        if(ext.matches(".*\\.(txt|md|json|csv|log|xml|css|js|java|h|cpp|c|py|ini|xi)$")){openText(path);return;}
        info("Este formato se puede copiar, mover y exportar. El editor abre texto e imágenes.");
    }

    private void openText(String path){
        if(files==null){info("Preparando archivos…");return;}
        WindowUi w=window("Editor","Documento de texto","text:"+path);if(w==null)return;
        TextView filename=label(path,11,ACCENT,true);w.content.addView(filename);
        EditText editor=new EditText(this);editor.setTextColor(WHITE);editor.setHintTextColor(MUTED);editor.setTextSize(13);
        editor.setGravity(Gravity.TOP|Gravity.START);editor.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        editor.setBackground(round(0xff18232d,0xff415664,7));editor.setPadding(dp(10),dp(8),dp(10),dp(8));editor.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);editor.setEnabled(false);editor.setContentDescription("CONTENIDO DEL DOCUMENTO");
        LinearLayout.LayoutParams ep=new LinearLayout.LayoutParams(-1,0,1);ep.topMargin=dp(7);w.content.addView(editor,ep);
        TextView status=label("Cargando…",10,MUTED,false);margin(w.content,status,5);
        String[] baseline={""};long[] modified={0};boolean[] loading={true};
        String draftKey="draft:"+path;
        Runnable persist=()->{if(loading[0])return;String value=editor.getText().toString();
            if(value.equals(baseline[0]))preferences.edit().remove(draftKey).apply();else preferences.edit().putString(draftKey,value).apply();};
        draftWriters.put(w.dialog.key,persist);
        Runnable deferred=()->{persist.run();status.setText(editor.getText().toString().equals(baseline[0])?"Guardado":"Borrador conservado · pulsa Guardar para actualizar el archivo");};
        editor.addTextChangedListener(watcher(()->{if(loading[0])return;clockHandler.removeCallbacks(deferred);clockHandler.postDelayed(deferred,400);status.setText("Cambios sin guardar");}));
        Runnable save=()->{
            if(loading[0])return;String value=editor.getText().toString();
            fileWork(()->{if(files.modified(path)!=modified[0])throw new IOException("El archivo cambió. Tu borrador se conserva; abre de nuevo el documento.");
                if(value.getBytes(java.nio.charset.StandardCharsets.UTF_8).length>WorkspaceFiles.MAX_TEXT)throw new IOException("El editor admite hasta 256 KiB");
                files.write(path,value.getBytes(java.nio.charset.StandardCharsets.UTF_8),true);modified[0]=files.modified(path);
            },()->{baseline[0]=value;persist.run();status.setText(editor.getText().toString().equals(value)?"Guardado":"Guardado; hay cambios más recientes en el borrador");});
        };
        LinearLayout buttons=horizontal();TextView storeButton=touch("GUARDAR",11,true,save);storeButton.setContentDescription("GUARDAR DOCUMENTO");buttons.addView(storeButton,new LinearLayout.LayoutParams(0,dp(38),1));
        TextView export=touch("EXPORTAR",11,false,()->{persist.run();exportFile(path,true);});LinearLayout.LayoutParams xp=new LinearLayout.LayoutParams(0,dp(38),1);xp.leftMargin=dp(7);buttons.addView(export,xp);margin(w.content,buttons,5);
        w.dialog.saveAction=save;w.dialog.setOnDismissListener(h->{clockHandler.removeCallbacks(deferred);persist.run();draftWriters.remove(h.key);});
        engine.execute(()->{try{String loaded;
            try{loaded=files.text(path);}catch(IOException e){if(files.modified(path)!=0||!preferences.contains(draftKey))throw e;loaded="";}
            final String saved=loaded;long stamp=files.modified(path);runOnUiThread(()->{
            if(!w.dialog.isShowing())return;baseline[0]=saved;modified[0]=stamp;editor.setText(preferences.getString(draftKey,saved));loading[0]=false;editor.setEnabled(true);
            status.setText(editor.getText().toString().equals(saved)?"Guardado":"Borrador recuperado");
        });}catch(Exception ex){runOnUiThread(()->status.setText("No se pudo leer. El borrador anterior se conserva: "+ex.getMessage()));}});
    }
    private void openImage(String path){
        if(files==null)return;
        WindowUi w=window("Imagen","Visor de imágenes","image:"+path);if(w==null)return;
        TextView name=label(path,11,ACCENT,false);w.content.addView(name);
        android.widget.ImageView image=new android.widget.ImageView(this);image.setContentDescription("IMAGEN ABIERTA");image.setAdjustViewBounds(true);image.setScaleType(android.widget.ImageView.ScaleType.FIT_CENTER);w.content.addView(image,new LinearLayout.LayoutParams(-1,0,1));
        engine.execute(()->{try{
            java.io.File f=files.image(path);android.graphics.BitmapFactory.Options o=new android.graphics.BitmapFactory.Options();o.inJustDecodeBounds=true;
            android.graphics.BitmapFactory.decodeFile(f.getPath(),o);if(o.outWidth<1||o.outHeight<1)throw new IOException("La imagen no es válida");
            int width=o.outWidth,height=o.outHeight;o.inSampleSize=1;while(width/o.inSampleSize>1280||height/o.inSampleSize>1280)o.inSampleSize*=2;
            o.inJustDecodeBounds=false;android.graphics.Bitmap bitmap=android.graphics.BitmapFactory.decodeFile(f.getPath(),o);if(bitmap==null)throw new IOException("No se pudo leer la imagen");
            runOnUiThread(()->{if(w.dialog.isShowing()){image.setImageBitmap(bitmap);name.setText(path+" · "+width+" × "+height);}});
        }catch(Exception ex){runOnUiThread(()->name.setText(ex.getMessage()));}});
        w.dialog.setOnDismissListener(h->image.setImageDrawable(null));
    }
    private void openCalculator(){
        WindowUi w=window("Calculadora","Operaciones ejecutadas en XI");if(w==null)return;
        EditText field=bbyqField(w.content,"2.5 * (3 + 1)");field.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        field.setText(preferences.getString("calculator-expression",""));field.setContentDescription("EXPRESIÓN CALCULADORA");
        TextView result=label(preferences.getString("calculator-result","0"),21,WHITE,true);result.setTextIsSelectable(true);result.setContentDescription("RESULTADO CALCULADORA");margin(w.content,result,5);
        String[][] keys={{"7","8","9","÷"},{"4","5","6","×"},{"1","2","3","−"},{"0",".","(",")"},{"C","⌫","=","+"}};
        LinearLayout keypad=vertical();w.content.addView(keypad,new LinearLayout.LayoutParams(-1,0,1));
        Runnable evaluate=()->{
            if(executing){info("XI está ocupado");return;}final String expression=field.getText().toString();
            final String program;try{program=new CalculatorCompiler().compile(expression);}catch(Exception e){result.setText(e.getMessage());return;}
            executing=true;engine.execute(()->{try{
                if(store==null)throw new IOException("Núcleo XI no disponible");
                ResultPacket packet=ResultPacket.parse(NativeRuntime.execute(store.loadState(),program));
                String answer=null;for(String line:packet.output.split("\n")){if(!line.startsWith("{"))continue;JSONObject j=new JSONObject(line);if("r0".equals(j.optString("register")))answer=CalculatorCompiler.format(j.getLong("value"));}
                if(answer==null)throw new IOException("XI no devolvió una cantidad");final String value=answer;
                runOnUiThread(()->{executing=false;result.setText(value);preferences.edit().putString("calculator-expression",expression).putString("calculator-result",value).apply();Log.i(LOG,"CALCULATOR_XI_OK "+expression+"="+value);});
            }catch(Throwable ex){runOnUiThread(()->{executing=false;result.setText("Revisa la operación o utiliza cantidades menores");});}});
        };
        for(String[] row:keys){LinearLayout line=horizontal();for(String key:row){
            TextView button=touch(key,14,key.equals("="),()->{
                if(key.equals("="))evaluate.run();else if(key.equals("C"))field.setText("");
                else if(key.equals("⌫")){int start=Math.max(0,field.getSelectionStart());if(start>0)field.getText().delete(start-1,start);}
                else{int pos=Math.max(0,field.getSelectionStart());field.getText().insert(pos,key);}
            });button.setPadding(dp(2),0,dp(2),0);LinearLayout.LayoutParams k=new LinearLayout.LayoutParams(0,-1,1);k.setMargins(dp(2),dp(1),dp(2),dp(1));line.addView(button,k);
        }keypad.addView(line,new LinearLayout.LayoutParams(-1,0,1));}
        field.setOnEditorActionListener((v,id,event)->{evaluate.run();return true;});
        w.dialog.saveAction=evaluate;
    }

    private void openTasks(){
        WindowUi w=window("Ventanas","Aplicaciones de este escritorio");if(w==null)return;
        TextView memory=label("",11,MUTED,false);w.content.addView(memory);
        ScrollView scroll=new ScrollView(this);LinearLayout list=vertical();scroll.addView(list);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        Runnable refresh=()->{
            memory.setText(windows.all().size()+" ventanas · Proceso "+String.format(Locale.ROOT,"%.1f",android.os.Debug.getPss()/1024.0)+" MiB PSS · CPU "+android.os.Process.getElapsedCpuTime()+" ms acumulados");
            list.removeAllViews();for(DesktopWindows.Handle handle:windows.all()){
                LinearLayout row=horizontal();TextView title=label(handle.title+(handle.isMinimized()?" · minimizada":" · visible"),12,WHITE,false);row.addView(title,new LinearLayout.LayoutParams(0,dp(38),1));
                TextView show=touch("Mostrar",10,false,handle::show);show.setPadding(dp(8),0,dp(8),0);row.addView(show,new LinearLayout.LayoutParams(dp(70),dp(34)));
                TextView close=touch("Cerrar ventana",10,false,()->{handle.dismiss();if(w.dialog.isShowing())openTasksRefresh(list,memory);});close.setPadding(dp(5),0,dp(5),0);row.addView(close,new LinearLayout.LayoutParams(dp(100),dp(34)));margin(list,row,5);
            }
        };
        margin(w.content,touch("Actualizar",11,false,refresh),5);refresh.run();
    }
    private void openTasksRefresh(LinearLayout list,TextView memory){
        list.removeAllViews();memory.setText(windows.all().size()+" ventanas abiertas. Pulsa Actualizar para ver la lista.");
    }
    private void openHostApps(){
        WindowUi w=window("Android","Aplicaciones instaladas en Android");if(w==null)return;
        TextView note=label("Se abren en Android. Usa Atrás o Recientes para volver al escritorio.",11,MUTED,false);w.content.addView(note);
        ScrollView scroll=new ScrollView(this);LinearLayout list=vertical();scroll.addView(list);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        engine.execute(()->{try{
            android.content.pm.PackageManager pm=getPackageManager();Intent query=new Intent(Intent.ACTION_MAIN);query.addCategory(Intent.CATEGORY_LAUNCHER);
            List<android.content.pm.ResolveInfo> apps=pm.queryIntentActivities(query,0);apps.sort((a,b)->a.loadLabel(pm).toString().compareToIgnoreCase(b.loadLabel(pm).toString()));
            runOnUiThread(()->{if(!w.dialog.isShowing())return;
                for(android.content.pm.ResolveInfo app:apps){if(app.activityInfo.packageName.equals(getPackageName()))continue;
                    String title=app.loadLabel(pm).toString();margin(list,touch(title,12,false,()->{
                        try{Intent launch=new Intent(Intent.ACTION_MAIN);launch.addCategory(Intent.CATEGORY_LAUNCHER);launch.setClassName(app.activityInfo.packageName,app.activityInfo.name);launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);startActivity(launch);}catch(Exception e){info("No se pudo abrir "+title);}
                    }),5);
                }
            });
        }catch(Exception ex){runOnUiThread(()->note.setText("No se pudo leer la lista de aplicaciones"));}});
    }
    private JSONObject rxeInfo(byte[] image)throws Exception{return new JSONObject(new String(NativeRuntime.inspectRxe(image),java.nio.charset.StandardCharsets.UTF_8));}
    private void seedBbyqExecutable()throws Exception{
        if(preferences.getBoolean("bbyq-rxe-e016-seeded",false))return;
        boolean exists=false;for(WorkspaceFiles.Entry e:files.list("Aplicaciones"))if(e.path.equals("Aplicaciones/BBY-Q.rxe"))exists=true;
        if(!exists){try(InputStream in=getAssets().open("native-apps/BBY-Q.rxe")){ByteArrayOutputStream data=new ByteArrayOutputStream();byte[] buffer=new byte[8192];int n;while((n=in.read(buffer))!=-1){if(data.size()+n>WorkspaceFiles.MAX_FILE)throw new IOException("Aplicación BBY Q demasiado grande");data.write(buffer,0,n);}byte[] executable=data.toByteArray();rxeInfo(executable);files.write("Aplicaciones/BBY-Q.rxe",executable,false);}}
        preferences.edit().putBoolean("bbyq-rxe-e016-seeded",true).apply();
    }
    private void seedRxe()throws Exception{
        if(preferences.getBoolean("rxe-seeded-v1",false))return;
        String path="Aplicaciones/Hola-REVERSE.rxe";boolean exists=false;for(WorkspaceFiles.Entry e:files.list("Aplicaciones"))if(e.path.equals(path))exists=true;
        if(!exists)files.write(path,NativeRuntime.compileRxe("#!reverse-pc-machine-v1\nset r0 6\nmul r0 7\nprint r0\nhalt\n".getBytes(java.nio.charset.StandardCharsets.UTF_8),"Hola REVERSE".getBytes(java.nio.charset.StandardCharsets.UTF_8)),false);
        preferences.edit().putBoolean("rxe-seeded-v1",true).apply();
    }
    private void refreshRxe(){
        if(files==null||isDestroyed())return;if(rxeLoading){rxeRefreshPending=true;return;}rxeLoading=true;
        engine.execute(()->{try{java.util.ArrayList<RxeApp> found=new java.util.ArrayList<>();
            for(WorkspaceFiles.Entry e:files.list("Aplicaciones"))if(!e.directory&&e.path.toLowerCase(Locale.ROOT).endsWith(".rxe"))try{found.add(new RxeApp(e.path,rxeInfo(files.read(e.path))));}catch(Exception invalid){Log.w(LOG,"RXE_CATALOG_REJECT "+e.path,invalid);}
            runOnUiThread(()->{rxeLoading=false;if(isDestroyed())return;rxeApps=found;renderRxeShortcuts();for(Runnable r:new java.util.ArrayList<>(rxeRefreshers.values()))r.run();if(rxeRefreshPending){rxeRefreshPending=false;refreshRxe();}});
        }catch(Exception e){runOnUiThread(()->{rxeLoading=false;Log.e(LOG,"RXE_CATALOG_FAILED",e);});}});
    }
    private boolean isRxePinned(String path){return preferences.getStringSet("rxe-pins",java.util.Collections.emptySet()).contains(path);}
    private void pinRxe(RxeApp app){java.util.Set<String> pins=new java.util.HashSet<>(preferences.getStringSet("rxe-pins",java.util.Collections.emptySet()));if(!pins.remove(app.path)){if(pins.size()>=12){info("El escritorio admite hasta 12 accesos personales");return;}pins.add(app.path);}preferences.edit().putStringSet("rxe-pins",pins).apply();renderRxeShortcuts();Log.i(LOG,"RXE_PIN_CHANGED "+app.path);}
    private void renderRxeShortcuts(){
        if(rxeShortcuts==null)return;rxeShortcuts.removeAllViews();int count=0;LinearLayout row=null;
        for(RxeApp app:rxeApps)if(isRxePinned(app.path)){
            if(count++%3==0){row=horizontal();rxeShortcuts.addView(row);}addDesktopShortcut(row,"RX",app.title,()->openRxe(app.path,true));View icon=row.getChildAt(row.getChildCount()-1);icon.setContentDescription("ACCESO RXE "+app.path);icon.setOnLongClickListener(v->{rxeMenu(app,v);return true;});
        }
    }
    private void openAndroidApps(){
        WindowUi w=window("Aplicaciones","Ejecutables propios de REVERSE · .rxe");if(w==null)return;
        LinearLayout toolbar=horizontal();String[] names={"Agregar .rxe","Crear .rxe","Android","Actualizar"};Runnable[] actions={this::pickRxe,()->openStudio(2,null),this::openHostApps,this::refreshRxe};
        for(int i=0;i<names.length;i++){TextView b=touch(names[i],10,i==0,actions[i]);b.setPadding(dp(2),0,dp(2),0);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(35),1);p.rightMargin=dp(3);toolbar.addView(b,p);}w.content.addView(toolbar);
        ScrollView scroll=new ScrollView(this);LinearLayout list=vertical();scroll.addView(list);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        Runnable render=()->{if(!w.dialog.isShowing())return;list.removeAllViews();for(RxeApp app:rxeApps){LinearLayout row=horizontal();TextView name=touch(app.title,12,false,()->openRxe(app.path,true));name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.END);name.setContentDescription("ABRIR RXE "+app.path);row.addView(name,new LinearLayout.LayoutParams(0,dp(40),1));TextView menu=touch("⋮",16,false,()->{});menu.setPadding(0,0,0,0);menu.setContentDescription("OPCIONES RXE "+app.path);menu.setOnClickListener(v->rxeMenu(app,menu));row.addView(menu,new LinearLayout.LayoutParams(dp(36),dp(40)));margin(list,row,6);}if(list.getChildCount()==0)margin(list,label("Agrega un .rxe o créalo desde XI Studio.",12,MUTED,false),10);};
        rxeRefreshers.put(w.dialog.key,render);w.dialog.setOnDismissListener(h->rxeRefreshers.remove(h.key));render.run();refreshRxe();
    }
    private void rxeMenu(RxeApp app,View anchor){
        android.widget.PopupMenu menu=new android.widget.PopupMenu(this,anchor);String pin=isRxePinned(app.path)?"Quitar acceso":"Anclar al escritorio";for(String item:new String[]{"Ejecutar",pin,"Exportar","Propiedades","A la papelera"})menu.getMenu().add(item);
        menu.setOnMenuItemClickListener(item->{String action=item.getTitle().toString();if(action.equals("Ejecutar"))openRxe(app.path,true);else if(action.equals(pin))pinRxe(app);else if(action.equals("Exportar"))exportFile(app.path);else if(action.equals("Propiedades"))new AlertDialog.Builder(this).setTitle(app.title).setMessage("Ejecutable REVERSE .rxe\n"+app.abi+" · "+app.version+"\n"+app.instructions+" instrucciones\n"+app.path).setPositiveButton("Cerrar",null).show();else new AlertDialog.Builder(this).setTitle("Mover a la papelera").setMessage(app.title).setNegativeButton("Cancelar",null).setPositiveButton("Mover",(d,n)->fileWork(()->files.trash(app.path),this::refreshRxe)).show();return true;});menu.show();
    }
    private void createRxe(String source,String defaultName){
        promptName("Crear ejecutable .rxe",defaultName,chosen->{String name=chosen.toLowerCase(Locale.ROOT).endsWith(".rxe")?chosen:chosen+".rxe";final String path="Aplicaciones/"+name;final String title=name.substring(0,name.length()-4);
            engine.execute(()->{try{byte[] binary=NativeRuntime.compileRxe(source.getBytes(java.nio.charset.StandardCharsets.UTF_8),title.getBytes(java.nio.charset.StandardCharsets.UTF_8));files.write(path,binary,false);Log.i(LOG,"RXE_BUILD_OK "+path+" bytes="+binary.length);runOnUiThread(()->{info("Ejecutable creado: "+name);refreshRxe();});}catch(Exception e){runOnUiThread(()->info("No se pudo crear el ejecutable: "+e.getMessage()));}});
        });
    }
    private void openRxe(String path,boolean start){
        if(files==null){info("Preparando aplicaciones…");return;}
        engine.execute(()->{try{byte[] image=files.read(path);JSONObject metadata=rxeInfo(image);String title=metadata.getString("title");runOnUiThread(()->{
            if(isFinishing()||isDestroyed())return;if("chat".equals(metadata.optString("ui"))){openRxeChat(path,image,metadata);return;}WindowUi w=window(title,"Ejecutable REVERSE .rxe","rxe:"+path);if(w==null)return;
            TextView output=label(preferences.getString("rxe-output:"+path,"Pulsa Ejecutar para iniciar el programa."),12,WHITE,false);output.setTypeface(Typeface.MONOSPACE);output.setTextIsSelectable(true);output.setContentDescription("SALIDA RXE "+path);ScrollView scroll=new ScrollView(this);scroll.addView(output);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
            TextView run=touch("Ejecutar",12,true,()->{});margin(w.content,run,6);
            Runnable execute=()->{run.setEnabled(false);output.setText("Ejecutando…");engine.execute(()->{String text;try{if(store==null)throw new IOException("Núcleo no disponible");byte[] current=files.read(path);ResultPacket result=ResultPacket.parse(NativeRuntime.executeRxe(store.loadState(),current));store.writeState(result.state);text=pretty(result.output);if(text.length()>48000)text=text.substring(0,48000)+"\n[Salida visible recortada]";preferences.edit().putString("rxe-output:"+path,text).apply();Log.i(LOG,"RXE_RUN_OK "+path);}catch(Exception e){text="No se pudo ejecutar: "+e.getMessage()+"\nEstado anterior conservado";Log.e(LOG,"RXE_RUN_FAILED "+path,e);}final String result=text;runOnUiThread(()->{if(w.dialog.isShowing()){output.setText(result);run.setEnabled(true);}});});};run.setOnClickListener(v->execute.run());if(start)execute.run();
        });}catch(Exception e){runOnUiThread(()->info("Ejecutable rechazado: "+e.getMessage()));}});
    }
    private void pickRxe(){Intent select=new Intent(Intent.ACTION_OPEN_DOCUMENT);select.addCategory(Intent.CATEGORY_OPENABLE);select.setType("*/*");try{startActivityForResult(select,IMPORT_RXE);}catch(Exception e){info("No hay selector de archivos disponible");}}
    private void openRxeChat(String path,byte[] executable,JSONObject metadata){
        WindowUi w=window(metadata.optString("title","BBY Q"),"Aplicación .rxe · "+metadata.optString("app_version"),"rxe:"+path);if(w==null)return;
        TextView status=label("Abriendo la memoria de BBY Q…",11,ACCENT,false);status.setContentDescription("ESTADO BBY Q");margin(w.content,status,3);
        LinearLayout history=vertical();history.setPadding(dp(5),dp(5),dp(5),dp(5));ScrollView scroll=new ScrollView(this);scroll.setContentDescription("CHAT BBY Q");scroll.addView(history);w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        final int[] language={1};final RxeAppStore[] data={null};final boolean[] busy={true};
        EditText input=bbyqField(w.content,"Escribe un mensaje: hola");input.setContentDescription("ENTRADA BBY Q");input.setEnabled(false);
        LinearLayout controls=horizontal();TextView spanish=touch("Español",11,false,()->language[0]=1),english=touch("English",11,false,()->language[0]=2),send=touch("Enviar",11,true,()->{}),memory=touch("Memoria",11,false,()->{});
        spanish.setContentDescription("IDIOMA BBY Q ES");english.setContentDescription("IDIOMA BBY Q EN");send.setContentDescription("ENVIAR BBY Q");memory.setContentDescription("EXPORTAR MEMORIA BBY Q");
        for(TextView control:new TextView[]{spanish,english,send,memory})controls.addView(control,new LinearLayout.LayoutParams(0,dp(34),1));margin(w.content,controls,3);send.setEnabled(false);memory.setEnabled(false);
        Runnable paintLanguage=()->{spanish.setTextColor(language[0]==1?ACCENT:MUTED);english.setTextColor(language[0]==2?ACCENT:MUTED);};
        spanish.setOnClickListener(v->{language[0]=1;paintLanguage.run();});english.setOnClickListener(v->{language[0]=2;paintLanguage.run();});
        java.util.function.Consumer<RxeAppStore.Saved> render=saved->{
            history.removeAllViews();if(saved.history.length()==0)addBbyqMessage(history,scroll,"Memoria E016 lista. Puedes empezar con «hola».",MUTED);
            for(int i=0;i<saved.history.length();i++){JSONObject message=saved.history.optJSONObject(i);if(message==null)continue;boolean user="user".equals(message.optString("role"));addBbyqMessage(history,scroll,(user?"TÚ · ":"BBY Q · ")+message.optString("text"),user?MUTED:WHITE);history.getChildAt(history.getChildCount()-1).setContentDescription(!user&&i==saved.history.length()-1?"RESPUESTA BBY Q":"MENSAJE BBY Q "+i);}
            status.setText(metadata.optString("app_version")+" · Memoria guardada · "+(saved.language==1?"Español":"English"));
        };
        send.setOnClickListener(v->{
            String text=input.getText().toString().trim();if(text.isEmpty()||busy[0]||data[0]==null)return;if(text.getBytes(java.nio.charset.StandardCharsets.UTF_8).length>255){input.setError("Escribe un mensaje más corto");return;}final int selectedLanguage=language[0];busy[0]=true;send.setEnabled(false);input.setEnabled(false);memory.setEnabled(false);status.setText("BBY Q está pensando…");
            ((android.view.inputmethod.InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(input.getWindowToken(),0);
            engine.execute(()->{try{long started=SystemClock.elapsedRealtimeNanos();RxeAppStore.Saved saved=data[0].ask(executable,text,selectedLanguage);Log.i(LOG,"BBYQ_APP_TURN_OK identity="+data[0].identity+" messages="+saved.history.length()+" ns="+(SystemClock.elapsedRealtimeNanos()-started));runOnUiThread(()->{if(!w.dialog.isShowing())return;render.accept(saved);busy[0]=false;send.setEnabled(true);input.setEnabled(true);memory.setEnabled(true);input.setText("");});}catch(Exception error){Log.e(LOG,"BBYQ_APP_TURN_FAILED",error);runOnUiThread(()->{if(!w.dialog.isShowing())return;busy[0]=false;send.setEnabled(true);input.setEnabled(true);memory.setEnabled(true);status.setText("No se pudo completar o guardar el turno. Memoria anterior conservada.");});}});
        });
        memory.setOnClickListener(v->{if(busy[0]||data[0]==null)return;preferences.edit().putString("app-export-path",path).putString("app-export-identity",data[0].identity).apply();Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);save.addCategory(Intent.CATEGORY_OPENABLE);save.setType("application/x-reverse-state");save.putExtra(Intent.EXTRA_TITLE,"BBY-Q-memoria.rvp");try{startActivityForResult(save,EXPORT_APP_MEMORY);}catch(Exception error){info("No hay selector de archivos disponible");}});
        engine.execute(()->{try{RxeAppStore app=new RxeAppStore(getApplicationContext(),executable);RxeAppStore.Saved saved=app.load(executable);data[0]=app;Log.i(LOG,"BBYQ_APP_READY identity="+app.identity+" messages="+saved.history.length());runOnUiThread(()->{if(!w.dialog.isShowing())return;language[0]=saved.language;paintLanguage.run();render.accept(saved);busy[0]=false;input.setEnabled(true);send.setEnabled(true);memory.setEnabled(true);});}catch(Exception error){Log.e(LOG,"BBYQ_APP_OPEN_FAILED",error);runOnUiThread(()->status.setText("No se pudo abrir la memoria de BBY Q."));}});
    }
    private void importRxe(Uri uri){
        engine.execute(()->{try{byte[] image=readBounded(uri,WorkspaceFiles.MAX_FILE);JSONObject meta=rxeInfo(image);String suggested=meta.getString("title")+".rxe";runOnUiThread(()->promptName("Agregar ejecutable .rxe",suggested,chosen->{String name=chosen.toLowerCase(Locale.ROOT).endsWith(".rxe")?chosen:chosen+".rxe";String path="Aplicaciones/"+name;fileWork(()->{files.write(path,image,false);Log.i(LOG,"RXE_IMPORT_OK "+path);},this::refreshRxe);}));}catch(Exception e){Log.e(LOG,"RXE_IMPORT_REJECTED",e);runOnUiThread(()->info("Archivo rechazado: "+e.getMessage()));}});
    }
    private void openSystemPanel(){
        WindowUi w=window("Panel del sistema","Fecha, energía y controles de Android");if(w==null)return;
        android.os.BatteryManager battery=(android.os.BatteryManager)getSystemService(BATTERY_SERVICE);int level=battery==null?-1:battery.getIntProperty(android.os.BatteryManager.BATTERY_PROPERTY_CAPACITY);
        margin(w.content,label(new SimpleDateFormat("EEEE, d MMMM yyyy",Locale.getDefault()).format(new Date()),18,WHITE,true),4);
        margin(w.content,label(level>=0&&level<=100?"Batería: "+level+" %":"Batería no disponible",12,MUTED,false),8);
        for(String[] item:new String[][]{{"Wi-Fi",android.provider.Settings.ACTION_WIFI_SETTINGS},{"Sonido",android.provider.Settings.ACTION_SOUND_SETTINGS},{"Pantalla",android.provider.Settings.ACTION_DISPLAY_SETTINGS}}){
            margin(w.content,touch(item[0],12,false,()->{try{startActivity(new Intent(item[1]));}catch(Exception e){info("Este ajuste no está disponible");}}),8);
        }
        margin(w.content,touch("Ajustes de REVERSE",12,true,this::openSettings),10);
    }
    @Override public boolean onKeyDown(int keyCode,android.view.KeyEvent event){
        if(event.isAltPressed()&&keyCode==android.view.KeyEvent.KEYCODE_TAB){windows.cycle();return true;}
        if(event.isCtrlPressed()&&keyCode==android.view.KeyEvent.KEYCODE_S){DesktopWindows.Handle h=windows.focused();if(h!=null&&h.saveAction!=null)h.saveAction.run();return true;}
        if(event.isCtrlPressed()&&event.isAltPressed()&&keyCode==android.view.KeyEvent.KEYCODE_T){openTerminal();return true;}
        if(event.isMetaPressed()&&keyCode==android.view.KeyEvent.KEYCODE_D){windows.showDesktop();return true;}
        if(event.isAltPressed()&&keyCode==android.view.KeyEvent.KEYCODE_F4){if(windows.focused()!=null)windows.focused().dismiss();return true;}
        if(event.isMetaPressed()&&(keyCode==android.view.KeyEvent.KEYCODE_DPAD_LEFT||keyCode==android.view.KeyEvent.KEYCODE_DPAD_RIGHT)){
            if(windows.focused()!=null)windows.focused().snap(keyCode==android.view.KeyEvent.KEYCODE_DPAD_LEFT?-1:1);return true;
        }
        return super.onKeyDown(keyCode,event);
    }
    private void importFile(String folder){
        if(folder.isEmpty()||folder.equals("Papelera"))folder="Descargas";
        preferences.edit().putString("import-folder",folder).apply();
        Intent open=new Intent(Intent.ACTION_OPEN_DOCUMENT);open.addCategory(Intent.CATEGORY_OPENABLE);open.setType("*/*");
        try{startActivityForResult(open,IMPORT_FILE);}catch(Exception ex){info("No hay selector de archivos disponible");}
    }
    private void exportFile(String path){exportFile(path,false);}
    private void exportFile(String path,boolean includeDraft){
        preferences.edit().putString("export-path",path).putBoolean("export-draft",includeDraft).apply();
        String name=path.substring(path.lastIndexOf('/')+1);
        int dot=name.lastIndexOf('.');
        String mime=dot<0?null:android.webkit.MimeTypeMap.getSingleton().getMimeTypeFromExtension(name.substring(dot+1).toLowerCase(Locale.ROOT));
        Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);save.addCategory(Intent.CATEGORY_OPENABLE);save.setType(name.toLowerCase(Locale.ROOT).endsWith(".rxe")?"application/x-reverse-executable":mime==null?"application/octet-stream":mime);
        save.putExtra(Intent.EXTRA_TITLE,name);try{startActivityForResult(save,EXPORT_FILE);}catch(Exception e){info("No hay selector de archivos disponible");}
    }
    private byte[] readBounded(Uri uri,int limit)throws IOException{
        try(InputStream in=getContentResolver().openInputStream(uri);ByteArrayOutputStream out=new ByteArrayOutputStream()){
            if(in==null)throw new IOException("El archivo no está disponible");byte[] buffer=new byte[8192];int n;
            while((n=in.read(buffer))!=-1){if(out.size()+n>limit)throw new IOException("El archivo supera el límite de tamaño");out.write(buffer,0,n);}return out.toByteArray();
        }
    }
    private void backupDesktop(){
        Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);save.addCategory(Intent.CATEGORY_OPENABLE);save.setType("application/zip");save.putExtra(Intent.EXTRA_TITLE,"REVERSE-Desktop-respaldo.zip");
        try{startActivityForResult(save,EXPORT_DESKTOP);}catch(Exception ex){info("No hay selector de archivos disponible");}
    }
    private void backupFolder(java.util.zip.ZipOutputStream zip,String path,int depth,int[] count,long[] total)throws IOException{
        if(depth>10)throw new IOException("Demasiadas carpetas anidadas");
        for(WorkspaceFiles.Entry e:files.list(path)){
            if(++count[0]>2048)throw new IOException("El respaldo admite hasta 2.048 elementos");
            zip.putNextEntry(new java.util.zip.ZipEntry("archivos/"+e.path+(e.directory?"/":"")));
            if(!e.directory){byte[] data=files.read(e.path);total[0]+=data.length;if(total[0]>64*1024*1024)throw new IOException("El respaldo supera 64 MiB");zip.write(data);}zip.closeEntry();
            if(e.directory)backupFolder(zip,e.path,depth+1,count,total);
        }
    }

    private void openMonitor(){
        WindowUi w=window("Ether Core","Autoridad compartida XI / Inverse Memory");
        if(w==null)return;
        TextView state=label("Leyendo núcleo nativo...",12,MUTED,false);
        state.setTypeface(Typeface.MONOSPACE);
        state.setPadding(dp(13),dp(15),dp(13),dp(15));
        state.setBackground(round(0xff1a2430,0xff475664,15));
        ScrollView scroll=new ScrollView(this);
        scroll.addView(state);
        w.content.addView(scroll,new LinearLayout.LayoutParams(-1,0,1f));
        TextView refresh=touch("↻ ACTUALIZAR ESTADO",12,true,()->loadStats(state));
        margin(w.content,refresh,12);
        loadStats(state);
    }
    private void loadStats(TextView target){
        engine.execute(()->{
            try {
                if(store==null)throw new IllegalStateException("Núcleo no disponible: "+engineFailure);
                JSONObject j=new JSONObject(NativeRuntime.inspect(store.loadState()));
                String info="CORE ONLINE\n"
                    +"────────────────────────\n\n"
                    +"Estado: "+j.optString("state_format")+"\n"
                    +"Qubits activos: "+j.optInt("active_qubits")+"\n"
                    +"Componentes: "+j.optInt("components")+"\n"
                    +"Mayor componente: "+j.optInt("largest_component")+"\n"
                    +"Reglas Ether: "+j.optInt("ether_rules")+"\n"
                    +"Memoria tableau: "+j.optInt("tableau_bytes")+" bytes\n"
                    +"Revisión de núcleo: "+j.optInt("core_revision")+"\n"
                    +"Revisión de memoria: "+j.optInt("memory_revision")+"\n"
                    +"Excepciones: "+j.optInt("memory_overrides")+"\n"
                    +"Enlaces cuánticos: "+j.optInt("quantum_bindings")+"\n\n"
                    +"Este es el núcleo real de REVERSE-PC.\n"
                    +"No son estadísticas simuladas de la interfaz.";
                runOnUiThread(()->target.setText(info));
            }catch(Throwable err){
                runOnUiThread(()->{
                    target.setTextColor(0xffff9c9c);
                    target.setText("Núcleo no disponible.\n\n"+err);
                });
            }
        });
    }

    private void openBrowser(){
        WindowUi w=window("Navegador","Internet");
        if(w==null)return;
        LinearLayout address=horizontal();
        TextView back=touch("‹",20,false,()->{});
        TextView forward=touch("›",20,false,()->{});
        TextView refresh=touch("↻",17,false,()->{});
        EditText field=new EditText(this);
        field.setTextColor(WHITE);field.setTextSize(12);field.setHintTextColor(MUTED);
        field.setSingleLine(true);field.setHint("https://www.chromium.org");
        field.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_URI);
        field.setBackground(round(CARD_BRIGHT,0xff34506a,12));
        field.setPadding(dp(11),dp(9),dp(11),dp(9));

        try{
            WebView web=new WebView(this);browserView=web;
            web.setBackgroundColor(Color.WHITE);
            WebSettings settings=web.getSettings();
            settings.setJavaScriptEnabled(true);
            settings.setDomStorageEnabled(true);
            settings.setAllowFileAccess(false);
            settings.setAllowContentAccess(false);
            settings.setJavaScriptCanOpenWindowsAutomatically(false);
            settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
            settings.setBuiltInZoomControls(true);
            settings.setDisplayZoomControls(false);
            settings.setSafeBrowsingEnabled(true);
            web.setWebViewClient(new WebViewClient(){
                @Override public boolean shouldOverrideUrlLoading(WebView w,WebResourceRequest request){
                    String scheme=request.getUrl().getScheme();
                    return !"http".equalsIgnoreCase(scheme)&&!"https".equalsIgnoreCase(scheme);
                }
                @Override public void onPageFinished(WebView w,String url){
                    Log.i(LOG,"BROWSER_PAGE_READY "+url);
                    if(url.startsWith("https://")&&!url.startsWith("https://reverse.local")){
                        preferences.edit().putString("browser-url",url).apply();if(!field.hasFocus())field.setText(url);
                    }
                }
            });
            // Window content is a real Android WebView: Chromium-based but NOT an
            // independently emulated Chromium binary executing on XI.
            back.setOnClickListener(v->{if(web.canGoBack())web.goBack();});
            forward.setOnClickListener(v->{if(web.canGoForward())web.goForward();});
            refresh.setOnClickListener(v->web.reload());
            address.addView(back,new LinearLayout.LayoutParams(dp(36),dp(44)));
            address.addView(forward,new LinearLayout.LayoutParams(dp(36),dp(44)));
            address.addView(refresh,new LinearLayout.LayoutParams(dp(39),dp(44)));
            LinearLayout.LayoutParams editParams=new LinearLayout.LayoutParams(0,dp(44),1f);
            editParams.leftMargin=dp(7);
            address.addView(field,editParams);
            TextView go=touch("GO",12,true,()->navigate(web,field.getText().toString(),field));
            LinearLayout.LayoutParams goParams=new LinearLayout.LayoutParams(dp(51),dp(44));
            goParams.leftMargin=dp(6);
            address.addView(go,goParams);
            w.content.addView(address);
            TextView note=label("Escribe una dirección o busca en la web.",
                                10,MUTED,false);
            margin(w.content,note,8);
            LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,0,1f);
            p.topMargin=dp(11);
            w.content.addView(web,p);

            TextView chrome=touch("ABRIR EN CHROME INSTALADO",11,false,()->openChrome());
            margin(w.content,chrome,9);
            w.dialog.setOnDismissListener(x->{
                web.stopLoading();web.destroy();if(browserView==web)browserView=null;
            });
            String previous=preferences.getString("browser-url","");
            field.setText(previous);
            if(previous.startsWith("https://"))web.loadUrl(previous);
            else web.loadDataWithBaseURL("https://reverse.local/","<html><meta name='viewport' content='width=device-width,initial-scale=1'><body style='font-family:sans-serif;background:#f4f6f8;color:#263746;padding:28px'><h1>REVERSE</h1><p>Tu ventana a la web.</p><p>Escribe una dirección o una búsqueda en la barra superior.</p></body></html>","text/html","UTF-8",null);
        }catch(Throwable error){
            Log.e(LOG,"Browser could not start",error);
            appError(w.content,"No se pudo iniciar el motor WebView de Android.\n"+error.getMessage());
            TextView chrome=touch("ABRIR CHROME INSTALADO",12,true,this::openChrome);
            margin(w.content,chrome,12);
        }
    }
    private void navigate(WebView web,String userInput,EditText field){
        String trimmed=userInput.trim();
        if(trimmed.isEmpty())return;
        String url;
        if(trimmed.startsWith("https://")||trimmed.startsWith("http://"))url=trimmed;
        else if(trimmed.contains(" ")||!trimmed.contains(".")){
            try {
                url="https://www.google.com/search?q="+
                    java.net.URLEncoder.encode(trimmed,"UTF-8");
            }catch(java.io.UnsupportedEncodingException e){url="https://www.google.com";}
        }else url="https://"+trimmed;
        if(!url.startsWith("https://")&&!url.startsWith("http://")){
            info("Solo se admiten HTTP y HTTPS.");return;
        }
        field.setText(url);
        web.loadUrl(url);
    }
    private void openChrome(){
        try{
            Intent intent=new Intent(Intent.ACTION_VIEW,Uri.parse("https://www.chromium.org"));
            intent.setPackage("com.android.chrome");
            startActivity(intent);
        }catch(ActivityNotFoundException e){
            info("Chrome no está instalado. Puedes utilizar el navegador WebView.");
        }catch(Exception e){info("No se pudo abrir Chrome: "+e.getMessage());}
    }
    private void openSettings(){
        WindowUi w=window("Ajustes","Apariencia y funcionamiento");
        if(w==null)return;
        LinearLayout settingsBody=vertical();ScrollView settingsScroll=new ScrollView(this);settingsScroll.addView(settingsBody);
        w.content.addView(settingsScroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout panel=vertical();
        panel.setPadding(dp(16),dp(16),dp(16),dp(16));
        panel.setBackground(round(CARD,0xff29485a,16));
        panel.addView(label("REVERSE OS  /  PERSONAL COMPUTER",16,WHITE,true));
        margin(panel,label("Un espacio sencillo para tus programas.",13,MUTED,false),9);
        margin(panel,label("Arquitectura XI y Ether con estado RVPC2.\n"
                +"Android proporciona interfaz, procesos, teclado y controladores.\n\n"
                +"La computadora huésped XI aún no arranca como un sistema operativo independiente.",
                12,MUTED,false),11);
        settingsBody.addView(panel);
        TextView quality=label("FONDO ORBITAL — GPU GLES 3",12,ACCENT,true);
        margin(settingsBody,quality,12);
        LinearLayout qualityControls=horizontal();
        qualityControls.addView(touch("ECO",11,false,()->{
            preferences.edit().putInt("quality",1).apply();updateWallpaper();
            info("Calidad ECO seleccionada");
        }),new LinearLayout.LayoutParams(0,dp(38),1f));
        TextView automatic=touch("AUTO",11,true,()->{
            preferences.edit().putInt("quality",0).apply();updateWallpaper();
            info("Calidad adaptativa seleccionada");
        });
        LinearLayout.LayoutParams center=new LinearLayout.LayoutParams(0,dp(38),1f);
        center.leftMargin=dp(7);qualityControls.addView(automatic,center);
        TextView maximum=touch("ALTA",11,false,()->{
            preferences.edit().putInt("quality",2).apply();updateWallpaper();
            info("Calidad alta; medir FPS en la barra");
        });
        LinearLayout.LayoutParams right=new LinearLayout.LayoutParams(0,dp(38),1f);
        right.leftMargin=dp(7);qualityControls.addView(maximum,right);
        margin(settingsBody,qualityControls,7);
        LinearLayout motionControls=horizontal();
        motionControls.addView(touch("⏸ PAUSA",11,false,()->{
            preferences.edit().putBoolean("motion",false).apply();updateWallpaper();
            info("Fondo orbital en pausa");
        }),new LinearLayout.LayoutParams(0,dp(38),1f));
        TextView play=touch("▶ MOVIMIENTO",11,true,()->{
            preferences.edit().putBoolean("motion",true).apply();updateWallpaper();
            info("Fondo orbital animado");
        });
        LinearLayout.LayoutParams motionP=new LinearLayout.LayoutParams(0,dp(38),1f);
        motionP.leftMargin=dp(7);motionControls.addView(play,motionP);
        margin(settingsBody,motionControls,7);
        margin(settingsBody,touch("USAR COMO ESCRITORIO DE INICIO",12,false,()->{
            try{
                android.app.role.RoleManager roles=(android.app.role.RoleManager)getSystemService(android.app.role.RoleManager.class);
                if(roles!=null&&roles.isRoleAvailable(android.app.role.RoleManager.ROLE_HOME))startActivityForResult(roles.createRequestRoleIntent(android.app.role.RoleManager.ROLE_HOME),5150);
                else startActivity(new Intent(android.provider.Settings.ACTION_HOME_SETTINGS));
            }catch(Exception ex){info("Puedes elegir REVERSE en los ajustes de aplicaciones de inicio de Android");}
        }),12);
        margin(settingsBody,touch("EXPORTAR RESPALDO DEL ESCRITORIO",12,false,this::backupDesktop),12);
        TextView restart=touch("REINICIAR LA PANTALLA DE BIENVENIDA",12,false,()->{
            w.dialog.dismiss();
            Intent i=new Intent(this,BootActivity.class);
            i.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP|Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(i);finish();
        });
        margin(settingsBody,restart,20);
        TextView backup=touch("EXPORTAR MI MEMORIA RVPC2",12,false,()->{
            Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);
            save.addCategory(Intent.CATEGORY_OPENABLE);
            save.setType("application/octet-stream");
            save.putExtra(Intent.EXTRA_TITLE,"REVERSE-BBYQ-Ether.rvp");
            try{startActivityForResult(save,EXPORT_RVPC);}
            catch(ActivityNotFoundException error){info("No hay gestor de archivos disponible");}
        });
        margin(settingsBody,backup,13);
        TextView restore=touch("IMPORTAR RVPC2 VALIDADO",12,false,()->{
            Intent open=new Intent(Intent.ACTION_OPEN_DOCUMENT);
            open.addCategory(Intent.CATEGORY_OPENABLE);
            open.setType("*/*");
            try{startActivityForResult(open,IMPORT_RVPC);}
            catch(ActivityNotFoundException error){info("No hay gestor de archivos disponible");}
        });
        margin(settingsBody,restore,8);
        TextView reset=touch("RESTABLECER NÚCLEO ETHER",12,false,()->{
            new AlertDialog.Builder(this)
                .setTitle("Reiniciar el estado de la computadora")
                .setMessage("Se perderá el estado actual RVPC2. Tus programas XI seguirán guardados.")
                .setNegativeButton("CANCELAR",null)
                .setPositiveButton("RESTABLECER",(d,x)->{
                    engine.execute(()->{
                        try{
                            if(store==null)throw new IOException("Núcleo no disponible");
                            store.reset();
                            runOnUiThread(()->info("Estado RVPC2 restablecido"));
                        }catch(Throwable ex){
                            runOnUiThread(()->info("Error: "+ex.getMessage()));
                        }
                    });
                }).show();
        });
        margin(settingsBody,reset,11);
        TextView close=touch("CERRAR",12,true,w.dialog::dismiss);
        margin(settingsBody,close,16);
    }
    @Override protected void onActivityResult(int request,int code,Intent intent){
        super.onActivityResult(request,code,intent);
        if(code!=RESULT_OK||intent==null||intent.getData()==null)return;
        final Uri uri=intent.getData();
        if(request==IMPORT_RXE){importRxe(uri);return;}
        if(request==EXPORT_APP_MEMORY){
            fileWork(()->{byte[] executable=files.read(preferences.getString("app-export-path",""));RxeAppStore app=new RxeAppStore(getApplicationContext(),executable);if(!app.identity.equals(preferences.getString("app-export-identity","")))throw new IOException("El ejecutable cambió; vuelve a abrir su memoria");byte[] memory=app.load(executable).state;try(OutputStream out=getContentResolver().openOutputStream(uri)){if(out==null)throw new IOException("Destino no disponible");out.write(memory);}Log.i(LOG,"BBYQ_APP_MEMORY_EXPORTED bytes="+memory.length);},null);return;
        }
        if(request==IMPORT_FILE){
            fileWork(()->{
                String name="archivo";try(android.database.Cursor c=getContentResolver().query(uri,new String[]{android.provider.OpenableColumns.DISPLAY_NAME},null,null,null)){
                    if(c!=null&&c.moveToFirst())name=c.getString(0);
                }
                String path=WorkspaceFiles.join(preferences.getString("import-folder","Descargas"),name);files.write(path,readBounded(uri,WorkspaceFiles.MAX_FILE),false);
                Log.i(LOG,"FILE_IMPORT_OK "+path);
            },null);return;
        }else if(request==EXPORT_FILE){
            fileWork(()->{String path=preferences.getString("export-path","");
                byte[] data=preferences.getBoolean("export-draft",false)&&preferences.contains("draft:"+path)?preferences.getString("draft:"+path,"").getBytes(java.nio.charset.StandardCharsets.UTF_8):files.read(path);
                try(OutputStream out=getContentResolver().openOutputStream(uri)){if(out==null)throw new IOException("Destino no disponible");out.write(data);}
                Log.i(LOG,"FILE_EXPORT_OK "+path+" bytes="+data.length);
                runOnUiThread(()->info("Archivo exportado: "+path.substring(path.lastIndexOf('/')+1)));
            },null);return;
        }else if(request==EXPORT_DESKTOP){
            for(Runnable save:draftWriters.values())save.run();if(windows!=null)windows.save();
            fileWork(()->{
                if(store==null||files==null)throw new IOException("El escritorio no está listo");
                try(OutputStream out=getContentResolver().openOutputStream(uri);java.util.zip.ZipOutputStream zip=new java.util.zip.ZipOutputStream(out)){
                    byte[] state=store.loadState();NativeRuntime.inspect(state);zip.putNextEntry(new java.util.zip.ZipEntry("session.rvp"));zip.write(state);zip.closeEntry();
                    zip.putNextEntry(new java.util.zip.ZipEntry("escritorio.json"));zip.write(new JSONObject(preferences.getAll()).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));zip.closeEntry();
                    zip.putNextEntry(new java.util.zip.ZipEntry("papelera.json"));zip.write(new JSONObject(files.trashOrigins()).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));zip.closeEntry();
                    int[] count={0};long[] total={0};backupFolder(zip,"",0,count,total);RxeAppStore.backup(getApplicationContext(),zip,count,total);
                }
                Log.i(LOG,"DESKTOP_BACKUP_OK");
                runOnUiThread(()->info("Respaldo del escritorio exportado"));
            },null);return;
        }
        if(request==EXPORT_RVPC){
            engine.execute(()->{
                try{
                    if(store==null)throw new IOException("Núcleo Ether no disponible");
                    byte[] state=store.loadState();
                    NativeRuntime.inspect(state);
                    try(OutputStream out=getContentResolver().openOutputStream(uri)){
                        if(out==null)throw new IOException("Destino RVPC2 no disponible");
                        out.write(state);out.flush();
                    }
                    runOnUiThread(()->info("Memoria Ether exportada: "+state.length+" bytes"));
                }catch(Throwable error){
                    Log.e(LOG,"RVPC_EXPORT_FAILED",error);
                    runOnUiThread(()->info("Error exportando RVPC2: "+error.getMessage()));
                }
            });
        }else if(request==IMPORT_RVPC){
            new AlertDialog.Builder(this)
                .setTitle("Importar memoria Ether")
                .setMessage("Se sustituirá la sesión RVPC2 del escritorio. "+
                            "Haz una copia de seguridad antes de continuar. "+
                            "Los programas XI guardados no se borrarán.")
                .setNegativeButton("CANCELAR",null)
                .setPositiveButton("VALIDAR E IMPORTAR",(d,which)->engine.execute(()->{
                    try{
                        if(store==null)throw new IOException("Núcleo Ether no disponible");
                        byte[] incoming;
                        try(InputStream in=getContentResolver().openInputStream(uri);
                            ByteArrayOutputStream out=new ByteArrayOutputStream()){
                            if(in==null)throw new IOException("Origen RVPC2 no disponible");
                            byte[] chunk=new byte[8192];
                            int len;
                            while((len=in.read(chunk))!=-1){
                                if(out.size()+len>32*1024*1024+29)
                                    throw new IOException("RVPC2 supera el límite del núcleo");
                                out.write(chunk,0,len);
                            }
                            incoming=out.toByteArray();
                        }
                        NativeRuntime.inspect(incoming); // checksum, size, replay validation
                        store.writeState(incoming); // AtomicFile: commit only after validation
                        runOnUiThread(()->info("Memoria RVPC2 restaurada y validada"));
                    }catch(Throwable error){
                        Log.e(LOG,"RVPC_IMPORT_FAILED",error);
                        runOnUiThread(()->info("Importación rechazada: "+error.getMessage()));
                    }
                })).show();
        }
    }

    private void openPower(){
        new AlertDialog.Builder(this).setTitle("REVERSE OS")
            .setItems(new String[]{"Reiniciar escritorio","Configuración","Cerrar REVERSE OS"},(d,which)->{
                if(which==0){
                    Intent i=new Intent(this,BootActivity.class);
                    i.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP|Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(i);finish();
                }else if(which==1)openSettings();
                else {
                    android.app.role.RoleManager role=getSystemService(android.app.role.RoleManager.class);
                    if(role!=null&&role.isRoleHeld(android.app.role.RoleManager.ROLE_HOME))startActivity(new Intent(android.provider.Settings.ACTION_HOME_SETTINGS));
                    else finish();
                }
            }).show();
    }
    @Override public void onBackPressed(){
        if(windows!=null&&windows.focused()!=null){windows.focused().dismiss();return;}
        android.app.role.RoleManager role=getSystemService(android.app.role.RoleManager.class);
        if(role!=null&&role.isRoleHeld(android.app.role.RoleManager.ROLE_HOME))return;
        new AlertDialog.Builder(this).setTitle("REVERSE OS")
            .setMessage("¿Cerrar el escritorio?")
            .setNegativeButton("CANCELAR",null)
            .setPositiveButton("CERRAR",(d,which)->finish()).show();
    }
    @Override protected void onResume(){
        super.onResume();
        if(store!=null)refreshRxe();
        if(wallpaper!=null)wallpaper.onResume();
        if(browserView!=null){browserView.onResume();browserView.resumeTimers();}
    }
    @Override protected void onPause(){
        for(Runnable save:draftWriters.values())save.run();
        if(windows!=null)windows.save();
        if(wallpaper!=null)wallpaper.onPause();
        if(browserView!=null){browserView.onPause();browserView.pauseTimers();}
        super.onPause();
    }
    @Override protected void onDestroy(){
        clockHandler.removeCallbacks(tick);
        for(Runnable save:draftWriters.values())save.run();
        if(windows!=null)windows.release();
        // The process-wide worker serializes unfinished commits with a new activity.
        // Android terminates it with the app process; recreation never adds a writer.
        super.onDestroy();
    }

    // Consistent flat-color fallback when the GPU shader is unavailable.
    private final class DesktopBackdrop extends View {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        DesktopBackdrop(){super(DesktopActivity.this);}
        @Override protected void onDraw(Canvas canvas){
            float w=getWidth(),h=getHeight();
            canvas.drawColor(0xff131922);
            float cx=w*.60f,cy=h*.50f,scale=Math.min(w,h);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(0xff1c303d);
            canvas.drawCircle(cx,cy,scale*.25f,paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dp(1));
            paint.setColor(0xff394d5c);
            canvas.drawOval(cx-scale*.44f,cy-scale*.16f,
                            cx+scale*.44f,cy+scale*.16f,paint);
            paint.setColor(0xff354954);
            canvas.drawCircle(cx,cy,scale*.30f,paint);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(0xffacbfcd);
            canvas.drawCircle(cx+scale*.42f,cy,dp(3),paint);
        }
    }
}
