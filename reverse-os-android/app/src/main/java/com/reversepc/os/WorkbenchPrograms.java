package com.reversepc.os;

import android.content.Context;
import android.util.Log;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Locale;

/** RXE catalog and transactions for the current desktop. Call on RuntimeExecutor. */
final class WorkbenchPrograms {
    static final String BBYQ="Aplicaciones/BBY-Q.rxe";
    private final Context context;
    private final WorkspaceFiles files;
    private byte[] retiredHello;

    WorkbenchPrograms(Context context)throws Exception{
        this.context=context.getApplicationContext();files=new WorkspaceFiles(context.getFilesDir());
        // Use the same executable paths and private memories as DesktopActivity.
        // Never replace a user's executable or restore a deliberately removed app.
        android.content.SharedPreferences prefs=context.getSharedPreferences("workbench-programs",0);
        if(!prefs.getBoolean("seeded",false)){
            android.content.SharedPreferences previous=context.getSharedPreferences("desktop-v1",0);
            if(!exists(BBYQ)&&!previous.getBoolean("bbyq-rxe-e016-seeded",false))try(InputStream in=context.getAssets().open("native-apps/BBY-Q.rxe")){
                byte[] image=read(in);info(image);files.write(BBYQ,image,false);
            }
            prefs.edit().putBoolean("seeded",true).apply();
        }
        upgradeBbyq();
    }
    private void upgradeBbyq()throws Exception{
        byte[] bundled=BbyqInterface.asset(context,"native-apps/BBY-Q.rxe");
        if(!"E025-Chain-1".equals(info(bundled).optString("app_version")))return;
        if(!exists(BBYQ))return; // A deliberately removed application stays removed.
        byte[] previous=files.read(BBYQ);
        if(identity(previous).equals(identity(bundled)))return;
        JSONObject manifest=new JSONObject(new String(BbyqInterface.asset(context,"bbyq-interface/manifest.json"),StandardCharsets.UTF_8));
        String previousHash=identity(previous);
        boolean legacy=BbyqInterface.LEGACY_SHA.equals(previousHash);
        boolean oldW4u=manifest.getString("legacy_w4u_sha256").equals(previousHash);
        boolean oldSocial=manifest.getString("legacy_social_sha256").equals(previousHash);
        boolean knownControl=manifest.getString("legacy_control_sha256").equals(previousHash);
        JSONArray identities=manifest.optJSONArray("legacy_control_sha256s");
        if(identities!=null)for(int i=0;i<identities.length();i++)knownControl|=identities.getString(i).equals(identity(previous));
        if(!legacy&&!knownControl&&!oldW4u&&!oldSocial)return; // Keep custom executables.
        try{
            new RxeAppStore(context,previous).withLockedSnapshot(previous,old->{
                if(old.pending!=null)throw new IOException("La acción pendiente debe confirmarse antes de actualizar BBY Q");
                byte[] candidate=old.state;
                if(!oldSocial){
                    String migration=legacy?"upgrade-e016.rxe":oldW4u?"upgrade-w4u.rxe":"upgrade-control.rxe";
                    candidate=ResultPacket.parse(NativeRuntime.executeRxe(candidate,BbyqInterface.asset(context,"bbyq-interface/"+migration))).state;
                }
                candidate=ResultPacket.parse(NativeRuntime.executeRxe(candidate,BbyqInterface.asset(context,"bbyq-interface/upgrade-chain-ui.rxe"))).state;
                candidate=BbyqInterface.extendChain(context,candidate);
                new RxeAppStore(context,bundled).migrate(bundled,old,candidate);
                if(!identity(files.read(BBYQ)).equals(previousHash))throw new IOException("El ejecutable cambió durante la actualización");
                // Keep the old memory as a rollback copy and publish only after
                // the candidate state and full transcript are durable.
                files.write(BBYQ,bundled,true);
                Log.i("ReverseWorkbench","BBYQ_CHAIN_MIGRATED messages="+old.history.length());
            });
        }catch(Exception conflict){Log.w("ReverseWorkbench","BBYQ_CHAIN_RETAINED_PREVIOUS",conflict);}
    }
    private boolean exists(String path)throws Exception{
        for(WorkspaceFiles.Entry entry:files.list("Aplicaciones"))if(entry.path.equals(path))return true;
        return false;
    }
    static byte[] read(InputStream input)throws IOException{
        if(input==null)throw new IOException("No se pudo leer el archivo");
        ByteArrayOutputStream output=new ByteArrayOutputStream();byte[] buffer=new byte[8192];int count;
        while((count=input.read(buffer))!=-1){if(output.size()+count>WorkspaceFiles.MAX_FILE)throw new IOException("El ejecutable supera 8 MiB");output.write(buffer,0,count);}
        return output.toByteArray();
    }
    private static String path(String value)throws Exception{
        if(value==null||!WorkspaceFiles.parent(value).equals("Aplicaciones")||!value.toLowerCase(Locale.ROOT).endsWith(".rxe"))throw new IOException("Selecciona un ejecutable de Aplicaciones");
        WorkspaceFiles.name(value.substring("Aplicaciones/".length()));return value;
    }
    private static JSONObject info(byte[] image)throws Exception{
        return new JSONObject(new String(NativeRuntime.inspectRxe(image),StandardCharsets.UTF_8));
    }
    private static String identity(byte[] image)throws Exception{
        StringBuilder hex=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(image))hex.append(String.format(Locale.ROOT,"%02x",b&255));return hex.toString();
    }
    private byte[] checked(JSONObject request)throws Exception{
        byte[] image=files.read(path(request.getString("path")));
        if(!identity(image).equals(request.getString("identity")))throw new IOException("El ejecutable cambió. Vuelve a abrirlo antes de continuar");
        return image;
    }
    private boolean stockDemo(String path,byte[] image)throws Exception{
        if(!"Aplicaciones/Hola-REVERSE.rxe".equals(path))return false;
        if(retiredHello==null)retiredHello=NativeRuntime.compileRxe("#!reverse-pc-machine-v1\nset r0 6\nmul r0 7\nprint r0\nhalt\n".getBytes(StandardCharsets.UTF_8),"Hola REVERSE".getBytes(StandardCharsets.UTF_8));
        return java.util.Arrays.equals(retiredHello,image);
    }
    JSONObject catalog()throws Exception{
        JSONArray apps=new JSONArray();int rejected=0;
        for(WorkspaceFiles.Entry entry:files.list("Aplicaciones"))if(!entry.directory&&entry.path.toLowerCase(Locale.ROOT).endsWith(".rxe")){
            try{byte[] image=files.read(entry.path);if(stockDemo(entry.path,image))continue;apps.put(new JSONObject().put("path",entry.path).put("metadata",info(image)).put("bytes",image.length).put("identity",identity(image)));}
            catch(Exception invalid){rejected++;Log.w("ReverseWorkbench","RXE_CATALOG_REJECT "+entry.path,invalid);}
        }
        return new JSONObject().put("ok",true).put("apps",apps).put("rejected",rejected);
    }
    private JSONObject snapshot(String path,byte[] image,JSONObject metadata)throws Exception{
        JSONObject result=new JSONObject().put("ok",true).put("path",path).put("identity",identity(image)).put("metadata",metadata);
        if("chat".equals(metadata.optString("ui"))){
            RxeAppStore.Saved saved=new RxeAppStore(context,image).load(image);
            result.put("history",saved.history).put("language",saved.language);
            result.put("desktop_control",BbyqInterface.trusted(context,image));
            if(BbyqInterface.trusted(context,image))result.put("agents",BbyqInterface.trace(saved.state));
        }else result.put("output",context.getSharedPreferences("desktop-v1",0).getString("rxe-output:"+path,""));
        return result;
    }
    JSONObject open(String selected)throws Exception{
        String selectedPath=path(selected);byte[] image=files.read(selectedPath);
        return snapshot(selectedPath,image,info(image));
    }
    private JSONObject observation(JSONObject request)throws Exception{
        JSONObject desktop=request.optJSONObject("desktop");if(desktop==null)return null;
        JSONArray programs=desktop.getJSONArray("programs");if(programs.length()>16)throw new IOException("Catálogo demasiado grande");
        // Revalidate every frontend binding against actual native RXE metadata.
        for(int i=0;i<programs.length();i++){
            JSONObject app=programs.getJSONObject(i);byte[] image=checked(app);JSONObject metadata=info(image);
            int capability="chat".equals(metadata.optString("ui"))?1:2;
            if(app.getInt("target")!=16+i||app.getInt("capability")!=capability||"org.reverse.bbyq".equals(metadata.optString("app_id")))throw new IOException("Capacidad de programa inválida");
        }
        return desktop;
    }
    JSONObject ask(JSONObject request)throws Exception{
        byte[] image=checked(request);JSONObject metadata=info(image);
        if(!"chat".equals(metadata.optString("ui")))throw new IOException("Este ejecutable no ofrece una conversación");
        String text=request.getString("text").trim();int language=request.getInt("language");
        if(text.isEmpty()||text.getBytes(StandardCharsets.UTF_8).length>255||(language!=1&&language!=2))throw new IOException("Escribe un mensaje de hasta 255 bytes y elige un idioma");
        RxeAppStore app=new RxeAppStore(context,image);
        JSONObject observation=BbyqInterface.trusted(context,image)?observation(request):null;
        RxeAppStore.Saved saved=app.ask(image,text,language,observation);
        Log.i("ReverseWorkbench","WORKBENCH_APP_TURN_OK "+app.identity);
        JSONObject result=new JSONObject().put("ok",true).put("path",request.getString("path")).put("identity",app.identity).put("metadata",metadata).put("history",saved.history).put("language",saved.language).put("desktop_control",BbyqInterface.trusted(context,image));
        // Only a freshly committed ask may dispatch an action. open/catalog and
        // restoration never return this field and therefore never replay it.
        if(saved.pending!=null)result.put("control",saved.pending);
        if(BbyqInterface.trusted(context,image))result.put("agents",BbyqInterface.trace(saved.state));
        return result;
    }
    JSONObject acknowledge(JSONObject request)throws Exception{
        byte[] image=checked(request);
        if(!BbyqInterface.trusted(context,image))throw new IOException("Este ejecutable no controla el escritorio");
        RxeAppStore.Saved saved=new RxeAppStore(context,image).acknowledge(image,request.getString("ticket"),request.getBoolean("success"),observation(request));
        JSONObject result=snapshot(request.getString("path"),image,info(image));
        if(saved.pending!=null)result.put("control",saved.pending);
        return result;
    }
    JSONObject run(JSONObject request,SessionStore session)throws Exception{
        byte[] image=checked(request);
        if("chat".equals(info(image).optString("ui")))throw new IOException("Abre la conversación de esta aplicación");
        ResultPacket result=ResultPacket.parse(NativeRuntime.executeRxe(session.loadState(),image));
        session.writeState(result.state);
        String output=result.output.length()>48000?result.output.substring(0,48000)+"\n[Salida visible recortada]":result.output;
        context.getSharedPreferences("desktop-v1",0).edit().putString("rxe-output:"+request.getString("path"),output).apply();
        Log.i("ReverseWorkbench","WORKBENCH_RXE_RUN_OK "+request.getString("path"));
        return new JSONObject().put("ok",true).put("output",output);
    }
    JSONObject compile(JSONObject request)throws Exception{
        String source=request.getString("source");byte[] bytes=source.getBytes(StandardCharsets.UTF_8);
        if(bytes.length>WorkspaceFiles.MAX_TEXT)throw new IOException("El programa supera 256 KiB");
        String name=WorkspaceFiles.name(request.getString("name"));if(!name.toLowerCase(Locale.ROOT).endsWith(".rxe"))name+=".rxe";
        String target=path("Aplicaciones/"+name);
        byte[] image=NativeRuntime.compileRxe(bytes,name.substring(0,name.length()-4).getBytes(StandardCharsets.UTF_8));
        files.write(target,image,false);return new JSONObject().put("ok",true).put("path",target);
    }
    JSONObject importImage(byte[] image)throws Exception{
        JSONObject metadata=info(image);String title=metadata.getString("title").replaceAll("[\\\\/\\p{Cntrl}]","_").trim();
        if(title.isEmpty()||title.startsWith("."))title="Programa";
        // UTF-8 names must fit WorkspaceFiles' byte limit even with a copy suffix.
        while(title.length()>60||title.getBytes(StandardCharsets.UTF_8).length>140)title=title.substring(0,title.length()-1);
        String target="Aplicaciones/"+title+".rxe";int suffix=2;
        while(exists(target)){if(suffix>1000)throw new IOException("Hay demasiadas copias de esta aplicación");target="Aplicaciones/"+title+"-"+(suffix++)+".rxe";}
        files.write(target,image,false);return new JSONObject().put("ok",true).put("path",target);
    }
    byte[] exportImage(JSONObject request,boolean memory)throws Exception{
        byte[] image=checked(request);info(image);
        if(!memory)return image;
        if(!"chat".equals(info(image).optString("ui")))throw new IOException("La aplicación no ofrece una memoria privada");
        return new RxeAppStore(context,image).load(image).state;
    }
}
