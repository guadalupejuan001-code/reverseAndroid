package com.reversepc.os;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * REVERSE OS v0.1: interactive, offline application shell for the actual
 * XI programmable machine. Android provides display, storage and scheduling;
 * XI/Ether/InfiniteCore execute the user program (never mocked in Java).
 */
public final class MainActivity extends Activity {
    private static final String TAG="ReverseOS";
    private static final int BG=0xff08131b, PANEL=0xff122733, PANEL2=0xff1a3541;
    private static final int ACCENT=0xff23dfbb, TEXT=0xffeaf6f7, MUTED=0xff96adbc;
    private static final int ERROR=0xffff8c91;
    private static final int IMPORT_PROGRAM=701, IMPORT_STATE=702, EXPORT_PROGRAM=703, EXPORT_STATE=704;
    private final ExecutorService executor=RuntimeExecutor.INSTANCE;
    private SessionStore storage;
    private LinearLayout base,content;
    private int tab=0;
    private String active="01_euclides.xi",draft="",terminal="",lastOutput="",cachedStats="{}";
    private boolean busy=false,editing=false;
    private double lastRunMs=-1;
    private String lastProgram="",errorState="";
    private EditText consoleInput;

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        getWindow().setStatusBarColor(BG);
        getWindow().setNavigationBarColor(BG);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        try {
            storage=new SessionStore(this);
            tab=Math.max(0,Math.min(3,getIntent().getIntExtra("tab",0)));
            active=getPreferences(0).getString("last_file","01_euclides.xi");
            String requested=getIntent().getStringExtra("program");
            if(requested!=null && storage.listPrograms().contains(requested))active=requested;
            if(!storage.listPrograms().contains(active))active="01_euclides.xi";
            draft=storage.readProgram(active);
            terminal="REVERSE OS / XI MACHINE\nSistema local. Núcleo C++ de Ether y REVERSE-PC.\n"
                    +"Escribe help para ver los comandos.\n\n";
            build();
            refreshStats();
        }catch(Throwable ex){
            Log.e(TAG,"Startup failed",ex);
            errorState=message(ex);
            fatalScreen(errorState);
        }
    }
    private int dp(float v){return Math.round(v*getResources().getDisplayMetrics().density);}
    private GradientDrawable shape(int color,int stroke){
        GradientDrawable g=new GradientDrawable();
        g.setColor(color);g.setCornerRadius(dp(15));
        if(stroke!=0)g.setStroke(dp(1),stroke);
        return g;
    }
    private TextView label(String text,int sp,int color,boolean bold){
        TextView t=new TextView(this);t.setText(text);t.setTextSize(sp);t.setTextColor(color);
        t.setFontFeatureSettings("kern");
        if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        return t;
    }
    private LinearLayout column(){
        LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.VERTICAL);return v;
    }
    private LinearLayout row(){
        LinearLayout h=new LinearLayout(this);h.setGravity(Gravity.CENTER_VERTICAL);h.setOrientation(LinearLayout.HORIZONTAL);return h;
    }
    private void gap(LinearLayout p,int h){
        View space=new View(this);p.addView(space,new LinearLayout.LayoutParams(1,dp(h)));
    }
    private void margin(LinearLayout target,View child,int top){
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,-2);params.topMargin=dp(top);
        target.addView(child,params);
    }
    private LinearLayout card(){
        LinearLayout v=column();v.setPadding(dp(15),dp(15),dp(15),dp(15));
        v.setBackground(shape(PANEL,0));return v;
    }
    private Button button(String title,boolean main,Runnable action){
        Button b=new Button(this);
        b.setAllCaps(false);b.setText(title);b.setTextSize(13);
        b.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        b.setTextColor(main?BG:TEXT);b.setBackground(shape(main?ACCENT:PANEL2,0));
        b.setMinHeight(dp(45));b.setMinimumHeight(dp(45));
        b.setPadding(dp(12),0,dp(12),0);
        b.setOnClickListener(v->{if(!busy)action.run();});
        b.setEnabled(!busy);
        return b;
    }
    private void flex(LinearLayout parent,View view,int w){
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(46),w);
        p.setMargins(dp(3),0,dp(3),0);parent.addView(view,p);
    }
    private void heading(String h,String small){
        margin(content,label(h,21,TEXT,true),0);
        margin(content,label(small,12,MUTED,false),5);
        gap(content,18);
    }
    private void build(){
        base=column();base.setBackgroundColor(BG);
        // Explicitly respect Android 15+ navigation insets: never put execute
        // and console controls beneath the system gesture bar.
        base.setOnApplyWindowInsetsListener((view,insets)->{
            int bottom=android.os.Build.VERSION.SDK_INT>=30
                ? insets.getInsets(android.view.WindowInsets.Type.navigationBars()).bottom
                : insets.getSystemWindowInsetBottom();
            base.setPadding(0,0,0,bottom);
            return insets;
        });
        setContentView(base);
        LinearLayout header=column();header.setPadding(dp(19),dp(18),dp(19),dp(15));
        header.setBackgroundColor(PANEL);
        LinearLayout first=row();
        TextView home=label("‹",30,ACCENT,true);
        home.setGravity(Gravity.CENTER);
        home.setContentDescription("REGRESAR AL ESCRITORIO");
        home.setBackground(shape(PANEL2,0));
        LinearLayout.LayoutParams homeP=new LinearLayout.LayoutParams(dp(42),dp(43));
        homeP.rightMargin=dp(10);
        first.addView(home,homeP);
        home.setOnClickListener(v->finish());
        TextView name=label("XI STUDIO",23,TEXT,true);
        first.addView(name,new LinearLayout.LayoutParams(0,-2,1));
        TextView indicator=label(busy?"● EJECUTANDO":"● NÚCLEO ACTIVO",10,busy?0xffffc56b:ACCENT,true);
        first.addView(indicator);header.addView(first);
        margin(header,label("ETHER  /  XI MACHINE  /  RVPC2     v0.2",11,MUTED,false),6);
        base.addView(header);
        HorizontalScrollView navScroll=new HorizontalScrollView(this);navScroll.setHorizontalScrollBarEnabled(false);
        LinearLayout nav=row();nav.setPadding(dp(7),dp(8),dp(7),dp(8));
        String[] tabs={"PROGRAMAS","CONSOLA","ARCHIVOS","SISTEMA"};
        for(int i=0;i<tabs.length;i++){
            final int index=i;TextView item=label(tabs[i],12,i==tab?BG:MUTED,true);
            item.setGravity(Gravity.CENTER);item.setPadding(dp(16),dp(12),dp(16),dp(12));
            item.setBackground(shape(i==tab?ACCENT:PANEL,0));
            LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-2,-2);p.setMargins(dp(3),0,dp(3),0);
            nav.addView(item,p);item.setOnClickListener(v->{tab=index;build();if(tab==3)refreshStats();});
        }
        navScroll.addView(nav);base.addView(navScroll);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(false);
        content=column();content.setPadding(dp(18),dp(19),dp(18),dp(30));
        scroll.addView(content);base.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        switch(tab){
            case 0:showEditor();break;
            case 1:showConsole();break;
            case 2:showFiles();break;
            default:showSystem();break;
        }
        TextView footer=label("REVERSE-PC  ·  XI LOCAL  ·  RVPC2",10,MUTED,true);
        footer.setGravity(Gravity.CENTER);footer.setPadding(0,dp(12),0,dp(12));
        base.addView(footer);
    }
    private void fatalScreen(String detail){
        TextView fail=label("REVERSE OS\n\nNo se pudo iniciar el núcleo real:\n"+detail+
                "\n\nLa app no simulará una ejecución falsa.",16,ERROR,true);
        fail.setPadding(dp(22),dp(40),dp(22),dp(22));fail.setBackgroundColor(BG);setContentView(fail);
    }
    private void showEditor(){
        heading("PROGRAMAS XI","Edita instrucciones y ejecútalas con el motor C++ real.");
        LinearLayout sel=card();
        margin(sel,label("ARCHIVO ACTIVO",10,MUTED,true),0);
        margin(sel,label(active,17,ACCENT,true),4);
        margin(content,sel,0);
        gap(content,12);
        EditText editor=new EditText(this);
        editor.setTextSize(13);editor.setTextColor(TEXT);editor.setHintTextColor(MUTED);
        editor.setTypeface(Typeface.MONOSPACE);
        editor.setBackground(shape(PANEL2,0));editor.setGravity(Gravity.TOP|Gravity.START);
        editor.setPadding(dp(13),dp(14),dp(13),dp(14));
        editor.setMinLines(13);editor.setMaxLines(26);
        editor.setSingleLine(false);editor.setHorizontallyScrolling(false);
        editor.setText(draft);
        editor.setInputType(android.text.InputType.TYPE_CLASS_TEXT|
                android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE|
                android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        editor.addTextChangedListener(new TextWatcher(){
            public void beforeTextChanged(CharSequence s,int start,int count,int after){}
            public void onTextChanged(CharSequence s,int start,int before,int count){draft=s.toString();}
            public void afterTextChanged(Editable e){}
        });
        // Keep primary actions above the large editor so Android phones never
        // hide Execute below the keyboard or the fold.
        LinearLayout actions=row();
        flex(actions,button("GUARDAR",false,()->saveDraft()),1);
        flex(actions,button(busy?"EJECUTANDO":"▶ EJECUTAR",true,()->executeProgram(active,draft)),2);
        margin(content,actions,0);
        gap(content,11);
        content.addView(editor,new LinearLayout.LayoutParams(-1,dp(320)));
        LinearLayout tools=row();
        flex(tools,button("NUEVO",false,this::newProgramDialog),1);
        flex(tools,button("ABRIR",false,()->{tab=2;build();}),1);
        flex(tools,button("CONSOLA",false,()->{tab=1;build();}),1);
        margin(content,tools,8);
        if(lastRunMs>=0){
            gap(content,12);
            margin(content,label("Última ejecución · "+lastProgram+" · "+
                String.format(Locale.US,"%.2f ms",lastRunMs),11,MUTED,false),0);
        }
        gap(content,14);
        LinearLayout tips=card();
        margin(tips,label("INSTRUCCIONES DISPONIBLES",12,ACCENT,true),0);
        margin(tips,label("set  mov  add  sub  mul  div  mod  cmp\n"
                +"label  jmp  je  jne  jlt  jle  jgt  jge\n"
                +"load8  store8  print  halt\n"
                +"h  x  s  cx  cz  t  tdg  expect  qstats",12,TEXT,false),8);
        margin(content,tips,0);
    }
    private void showConsole(){
        heading("CONSOLA","Comandos del entorno local y resultados reales de XI.");
        TextView screen=label(terminal.isEmpty()?"REVERSE OS listo.":terminal,12,0xffbcebdc,false);
        screen.setTypeface(Typeface.MONOSPACE);
        screen.setBackground(shape(PANEL,0));
        screen.setPadding(dp(15),dp(15),dp(15),dp(15));
        screen.setTextIsSelectable(true);screen.setMinLines(16);
        content.addView(screen);
        gap(content,12);
        consoleInput=new EditText(this);
        consoleInput.setSingleLine(true);
        consoleInput.setTextSize(14);consoleInput.setTextColor(TEXT);
        consoleInput.setHintTextColor(MUTED);consoleInput.setHint("help · ls · run 01_euclides.xi");
        consoleInput.setBackground(shape(PANEL2,0));consoleInput.setPadding(dp(12),dp(12),dp(12),dp(12));
        content.addView(consoleInput);
        LinearLayout commands=row();
        flex(commands,button("ENVIAR",true,()->{
            String cmd=consoleInput.getText().toString();consoleInput.setText("");
            command(cmd);
        }),2);
        flex(commands,button("LIMPIAR",false,()->{terminal="";build();}),1);
        margin(content,commands,10);
        LinearLayout shortcuts=row();
        flex(shortcuts,button("LISTAR",false,()->command("ls")),1);
        flex(shortcuts,button("ESTADO",false,()->command("status")),1);
        margin(content,shortcuts,8);
    }
    private void showFiles(){
        heading("ARCHIVOS","Programas .xi conservados en el almacenamiento privado.");
        LinearLayout tools=row();
        flex(tools,button("+ NUEVO",true,this::newProgramDialog),1);
        flex(tools,button("IMPORTAR .XI",false,()->startPicker(IMPORT_PROGRAM,true,"text/plain","")),1);
        margin(content,tools,0);
        gap(content,13);
        List<String> names=storage.listPrograms();
        for(String filename:names){
            LinearLayout block=card();
            TextView txt=label(filename,15,filename.equals(active)?ACCENT:TEXT,true);
            block.addView(txt);
            margin(block,label(storage.programBytes(filename)+" bytes  ·  XI SCRIPT",11,MUTED,false),3);
            LinearLayout buttons=row();
            flex(buttons,button("EDITAR",false,()->openProgram(filename)),2);
            flex(buttons,button("▶",true,()->{
                try{executeProgram(filename,storage.readProgram(filename));}
                catch(IOException e){notice(message(e));}
            }),1);
            flex(buttons,button("BORRAR",false,()->deleteDialog(filename)),1);
            margin(block,buttons,12);
            margin(content,block,0);gap(content,11);
        }
        margin(content,label("Sandbox privado · sin acceso a archivos externos sin seleccionarlos.",
                            11,MUTED,false),12);
    }
    private void showSystem(){
        heading("SISTEMA","La interfaz usa Android; los cálculos XI se ejecutan en REVERSE-PC.");
        JSONObject obj;
        try{obj=new JSONObject(cachedStats);}catch(Exception e){obj=new JSONObject();}
        LinearLayout stats=card();
        margin(stats,label("NÚCLEO ETHER / QUANTUM RUNTIME",12,ACCENT,true),0);
        addStat(stats,"Qubits activos",obj.optString("active_qubits","—"));
        addStat(stats,"Componentes",obj.optString("components","—"));
        addStat(stats,"Componente mayor",obj.optString("largest_component","—"));
        addStat(stats,"Reglas Ether",obj.optString("ether_rules","—"));
        addStat(stats,"Payload tableau (B)",obj.optString("tableau_bytes","—"));
        addStat(stats,"Revisión núcleo",obj.optString("core_revision","—"));
        addStat(stats,"Revisión memoria",obj.optString("memory_revision","—"));
        addStat(stats,"Excepciones memoria",obj.optString("memory_overrides","—"));
        addStat(stats,"Enlaces cuánticos",obj.optString("quantum_bindings","—"));
        margin(content,stats,0);
        gap(content,14);
        LinearLayout run=row();
        flex(run,button("ACTUALIZAR",true,this::refreshStats),1);
        flex(run,button("EXPORTAR RVPC2",false,()->startPicker(EXPORT_STATE,false,
                "application/octet-stream","reverse-state.rvp")),2);
        margin(content,run,0);
        LinearLayout second=row();
        flex(second,button("IMPORTAR RVPC",false,()->startPicker(IMPORT_STATE,true,
                "application/octet-stream","")),1);
        flex(second,button("REINICIAR",false,this::resetDialog),1);
        margin(content,second,8);
        gap(content,16);
        LinearLayout about=card();
        margin(about,label("CAPACIDADES ACTUALES",12,ACCENT,true),0);
        margin(about,label("• XI Machine v1: registros, saltos, ciclos y memoria.\n"
            +"• Inverse Ether Memory + estados RVPC2 persistentes.\n"
            +"• Una sola autoridad QuantumRuntime/InfiniteCore.\n"
            +"• Cálculo Clifford+T, programa local y sin red.\n"
            +"• Los registros temporales r0–r15 se reinician en cada ejecución.\n\n"
            +"Límites: esta versión NO es un kernel autónomo, no arranca el teléfono, "
            +"no tiene multitarea de procesos ni controladores propios.",12,TEXT,false),8);
        margin(content,about,0);
    }
    private void addStat(LinearLayout panel,String name,String value){
        LinearLayout line=row();
        TextView key=label(name,12,MUTED,false);
        TextView val=label(value,14,TEXT,true);
        line.addView(key,new LinearLayout.LayoutParams(0,-2,1));
        line.addView(val);
        margin(panel,line,11);
    }
    private void saveDraft(){
        if(storage==null)return;
        try {
            storage.saveProgram(active,draft);
            notice("Guardado: "+active);
        }catch(Exception e){notice(message(e));}
    }
    private void openProgram(String filename){
        try {
            active=filename;draft=storage.readProgram(filename);
            getPreferences(0).edit().putString("last_file",filename).apply();
            tab=0;build();
        }catch(Exception e){notice(message(e));}
    }
    private void newProgramDialog(){
        EditText input=new EditText(this);input.setSingleLine(true);
        input.setHint("mi_programa.xi");
        new AlertDialog.Builder(this).setTitle("Nuevo programa XI").setView(input)
            .setNegativeButton("CANCELAR",null).setPositiveButton("CREAR",(d,which)->{
                try{
                    String name=SessionStore.normalized(input.getText().toString());
                    if(storage.listPrograms().contains(name))throw new IOException("Ya existe "+name);
                    storage.saveProgram(name,"#!reverse-pc-machine-v1\nset r0 1\nprint r0\nhalt\n");
                    openProgram(name);
                }catch(Exception e){notice(message(e));}
            }).show();
    }
    private void deleteDialog(String filename){
        new AlertDialog.Builder(this).setTitle("Eliminar "+filename)
            .setMessage("Se eliminará el archivo del espacio privado de programas.")
            .setNegativeButton("CANCELAR",null)
            .setPositiveButton("ELIMINAR",(d,w)->{
                try {
                    storage.deleteProgram(filename);
                    if(filename.equals(active)){
                        List<String> all=storage.listPrograms();
                        if(all.isEmpty()){storage.saveProgram("nuevo.xi",
                            "#!reverse-pc-machine-v1\nset r0 1\nprint r0\n");active="nuevo.xi";}
                        else active=all.get(0);
                        draft=storage.readProgram(active);
                    }
                    build();
                }catch(Exception e){notice(message(e));}
            }).show();
    }
    private void executeProgram(String name,String source){
        if(busy)return;
        busy=true;build();
        executor.execute(()->{
            long start=SystemClock.elapsedRealtimeNanos();
            try {
                storage.saveProgram(name,source);
                byte[] previous=storage.loadState();
                ResultPacket result=ResultPacket.parse(NativeRuntime.execute(previous,source));
                String nextStats=NativeRuntime.inspect(result.state);
                storage.writeState(result.state); // Commit only after native success and validation.
                long ns=SystemClock.elapsedRealtimeNanos()-start;
                Log.i(TAG,"RUN_OK "+name+" elapsed_ns="+ns);
                runOnUiThread(()->{
                    busy=false;lastProgram=name;lastRunMs=ns/1_000_000.0;
                    cachedStats=nextStats;lastOutput=result.output;
                    appendTerminal("$ run "+name+"\n"+prettify(result.output)+"\n"+
                        String.format(Locale.US,"[OK] %.2f ms · RVPC2 guardado\n\n",lastRunMs));
                    tab=1;build();
                });
            }catch(Throwable e){
                String detail=message(e);Log.e(TAG,"RUN_ERROR "+name,e);
                runOnUiThread(()->{
                    busy=false;
                    appendTerminal("$ run "+name+"\n[ERROR] "+detail+
                       "\nEstado anterior conservado.\n\n");
                    tab=1;build();
                });
            }
        });
    }
    private String prettify(String raw){
        if(raw==null||raw.isEmpty())return "(Programa completado sin salida)";
        StringBuilder out=new StringBuilder();
        String[] lines=raw.split("\n");
        for(String line:lines){
            if(line.isEmpty())continue;
            try {
                JSONObject value=new JSONObject(line);
                if("register".equals(value.optString("type"))){
                    out.append(value.optString("register")).append(" = ")
                       .append(value.optString("value")).append('\n');
                }else{out.append(line).append('\n');}
            }catch(Exception e){out.append(line).append('\n');}
        }
        return out.toString();
    }
    private void appendTerminal(String value){
        terminal+=value;
        if(terminal.length()>50000)terminal=terminal.substring(terminal.length()-40000);
    }
    private void command(String input){
        String cmd=input==null?"":input.trim();
        if(cmd.isEmpty())return;
        if(cmd.equalsIgnoreCase("clear")){terminal="";build();return;}
        appendTerminal("> "+cmd+"\n");
        String[] words=cmd.split("\\s+",2);
        String verb=words[0].toLowerCase(Locale.ROOT);
        String arg=words.length>1?words[1].trim():"";
        try{
            switch(verb){
                case "help":appendTerminal("help · ls · cat archivo.xi · open archivo.xi\n"
                    +"run archivo.xi · status · new · clear · reset\n\n");break;
                case "ls":
                    for(String f:storage.listPrograms())appendTerminal("  "+f+"\n");
                    appendTerminal("\n");break;
                case "cat":appendTerminal(storage.readProgram(arg)+"\n");break;
                case "open":
                case "edit":openProgram(SessionStore.normalized(arg));return;
                case "run":executeProgram(SessionStore.normalized(arg),storage.readProgram(arg));return;
                case "new":newProgramDialog();return;
                case "status":refreshStats();break;
                case "reset":resetDialog();return;
                default:appendTerminal("Comando desconocido: "+verb+". Usa help.\n");
            }
        }catch(Exception e){appendTerminal("[ERROR] "+message(e)+"\n");}
        build();
    }
    private void refreshStats(){
        if(storage==null)return;
        executor.execute(()->{
            try{
                String details=NativeRuntime.inspect(storage.loadState());
                runOnUiThread(()->{
                    cachedStats=details;
                    if(tab==3)build();
                    if(tab==1){appendTerminal(details+"\n");build();}
                });
            }catch(Exception e){runOnUiThread(()->notice(message(e)));}
        });
    }
    private void resetDialog(){
        if(busy)return;
        new AlertDialog.Builder(this).setTitle("Reiniciar núcleo")
            .setMessage("¿Restablecer el estado cuántico y la memoria? Tus programas .xi seguirán guardados.")
            .setNegativeButton("CANCELAR",null)
            .setPositiveButton("REINICIAR",(d,w)->executor.execute(()->{
                try {
                    storage.reset();String info=NativeRuntime.inspect(storage.loadState());
                    runOnUiThread(()->{
                        cachedStats=info;appendTerminal("[RESET] Nuevo RVPC2. Programas intactos.\n");build();
                    });
                }catch(Exception e){runOnUiThread(()->notice(message(e)));}
            })).show();
    }
    private void startPicker(int which,boolean read,String mime,String filename){
        Intent intent=new Intent(read?Intent.ACTION_OPEN_DOCUMENT:Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType(mime);
        if(!read)intent.putExtra(Intent.EXTRA_TITLE,filename);
        startActivityForResult(intent,which);
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(result!=RESULT_OK||data==null||data.getData()==null)return;
        Uri uri=data.getData();
        executor.execute(()->{
            try {
                if(request==EXPORT_STATE){
                    try(OutputStream out=getContentResolver().openOutputStream(uri)){
                        if(out==null)throw new IOException("No se abrió el destino");
                        out.write(storage.loadState());
                    }
                    runOnUiThread(()->notice("Estado RVPC exportado"));
                }else if(request==IMPORT_STATE){
                    byte[] candidate=readBounded(uri,32*1024*1024+29);
                    storage.writeState(candidate);
                    String info=NativeRuntime.inspect(candidate);
                    runOnUiThread(()->{
                        cachedStats=info;appendTerminal("[IMPORT] Estado RVPC restaurado.\n");build();
                    });
                }else if(request==IMPORT_PROGRAM){
                    byte[] dataBytes=readBounded(uri,256*1024);
                    String name="importado_"+System.currentTimeMillis()%100000+".xi";
                    storage.saveProgram(name,new String(dataBytes,StandardCharsets.UTF_8));
                    runOnUiThread(()->openProgram(name));
                }else if(request==EXPORT_PROGRAM){
                    try(OutputStream out=getContentResolver().openOutputStream(uri)){
                        if(out==null)throw new IOException("No se abrió el destino");
                        out.write(draft.getBytes(StandardCharsets.UTF_8));
                    }
                    runOnUiThread(()->notice("Programa exportado"));
                }
            }catch(Exception e){runOnUiThread(()->notice(message(e)));}
        });
    }
    private byte[] readBounded(Uri uri,int limit)throws IOException{
        try(InputStream in=getContentResolver().openInputStream(uri);
            ByteArrayOutputStream out=new ByteArrayOutputStream()){
            if(in==null)throw new IOException("No se abrió el archivo");
            byte[] buf=new byte[8192];int size;
            while((size=in.read(buf))!=-1){
                if(out.size()>limit-size)throw new IOException("Archivo supera el límite");
                out.write(buf,0,size);
            }
            return out.toByteArray();
        }
    }
    private static String message(Throwable e){
        String s=e.getMessage();
        return s==null?e.getClass().getSimpleName():s;
    }
    private void notice(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}
    @Override public void onDestroy(){super.onDestroy();}
}
