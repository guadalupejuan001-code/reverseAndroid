package com.reversepc.os;

import android.content.Context;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import org.json.JSONArray;
import org.json.JSONObject;

/** Typed observation/action transport. No words, prompts or intent are parsed here. */
final class BbyqInterface {
    static final long A=8200000000000000000L,M=8300000000000000000L;
    static final String LEGACY_SHA="2e189e11a9ceda2d4dfc2299e61892929dc696411d595b8e3c138c8134c2802f";
    private static String bundledIdentity;
    private BbyqInterface(){}
    static String identity(byte[] bytes)throws Exception{
        StringBuilder result=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(bytes))result.append(String.format(java.util.Locale.ROOT,"%02x",b&255));return result.toString();
    }
    static byte[] asset(Context ctx,String path)throws Exception{
        try(InputStream in=ctx.getAssets().open(path)){return WorkbenchPrograms.read(in);}
    }
    static boolean trusted(Context ctx,byte[] image)throws Exception{
        if(bundledIdentity==null){
            byte[] bundled=asset(ctx,"native-apps/BBY-Q.rxe");
            JSONObject info=new JSONObject(new String(NativeRuntime.inspectRxe(bundled),StandardCharsets.UTF_8));
            bundledIdentity="E025-Chain-1".equals(info.optString("app_version"))&&"org.reverse.bbyq".equals(info.optString("app_id"))?identity(bundled):"";
        }
        String hash=identity(image);
        if(bundledIdentity.equals(hash))return true;
        JSONArray compatible=new JSONObject(new String(asset(ctx,"bbyq-interface/manifest.json"),StandardCharsets.UTF_8)).optJSONArray("compatible_w4u_sha256s");
        if(compatible!=null)for(int i=0;i<compatible.length();i++)if(hash.equals(compatible.getString(i)))return true;
        return false;
    }
    // Recovered from the delivered 2.5.0 host. Work only on candidate bytes;
    // publication is handled by WorkbenchPrograms after all lessons succeed.
    static byte[] extendChain(Context ctx,byte[] state)throws Exception{
        ResultPacket marker=ResultPacket.parse(NativeRuntime.execute(state,"#!reverse-pc-machine-v1\nload8 r0 8600000000000000000\nprint r0\nhalt\n"));
        int schema=-1;
        for(String row:marker.output.split("\\n"))if(row.startsWith("{"))schema=new JSONObject(row).getInt("value");
        if(schema==25)return state;
        if(schema!=0)throw new IOException("La nueva región de memoria ya contiene otro estado");
        byte[] raw=asset(ctx,"bbyq-interface/upgrade-chain.json");
        JSONObject manifest=new JSONObject(new String(asset(ctx,"bbyq-interface/manifest.json"),StandardCharsets.UTF_8));
        if(!identity(raw).equals(manifest.getJSONObject("chain").getString("curriculum_sha256")))throw new IOException("El currículo no coincide con esta actualización");
        JSONObject curriculum=new JSONObject(new String(raw,StandardCharsets.UTF_8));
        if(curriculum.getInt("format")!=1||!"E025".equals(curriculum.getString("profile")))throw new IOException("Formato de currículo desconocido");
        JSONArray initializers=curriculum.getJSONArray("initializers");
        for(int i=0;i<initializers.length();i++){
            String file=initializers.getString(i);
            if(!file.matches("chain/initialize-[a-z]+\\.xi"))throw new IOException("Inicializador no válido");
            state=ResultPacket.parse(NativeRuntime.execute(state,new String(asset(ctx,"bbyq-interface/"+file),StandardCharsets.UTF_8))).state;
        }
        JSONObject modules=curriculum.getJSONObject("modules");
        java.util.Map<String,String> cache=new java.util.HashMap<>();
        JSONArray lessons=curriculum.getJSONArray("lessons");
        for(int i=0;i<lessons.length();i++){
            JSONObject lesson=lessons.getJSONObject(i);String source;
            if(lesson.has("raw"))source="#!reverse-pc-machine-v1\n"+lesson.getString("raw");
            else{
                boolean dialogue=lesson.optBoolean("dialogue",false);
                String mode=(dialogue?"dialogue-":"")+lesson.getString("mode");
                if(!mode.matches("[a-z][a-z0-9-]*"))throw new IOException("Módulo de enseñanza no válido");
                String body=cache.get(mode);
                if(body==null){
                    JSONObject module=modules.getJSONObject(mode);String file=module.getString("file");
                    if(!file.equals("chain/"+mode+".xi"))throw new IOException("Ruta del módulo no válida");
                    byte[] bytes=asset(ctx,"bbyq-interface/"+file);
                    if(!identity(bytes).equals(module.getString("sha256")))throw new IOException("El módulo no coincide con el currículo");
                    body=new String(bytes,StandardCharsets.UTF_8);body=body.substring(body.indexOf('\n')+1);cache.put(mode,body);
                }
                byte[] text=lesson.optString("text","").getBytes(StandardCharsets.UTF_8);
                if(text.length>255)throw new IOException("Lección fuera del contrato de entrada");
                long mailbox=dialogue?7300000000000000000L:7400000000000000000L;
                StringBuilder program=new StringBuilder("#!reverse-pc-machine-v1\n");
                if(text.length>0){
                    program.append(put(mailbox+16,1)).append(put(mailbox+17,text.length));
                    if(!dialogue)program.append(put(mailbox+3,lesson.getInt("language")));
                    for(int j=0;j<text.length;j++)program.append(put(mailbox+128+j,text[j]&255));
                }
                JSONArray fields=lesson.getJSONArray("fields");
                for(int j=0;j<fields.length();j++){
                    JSONArray field=fields.getJSONArray(j);int value=field.getInt(1);
                    if(value<0||value>255)throw new IOException("Campo de enseñanza fuera de rango");
                    // Android JSON.getLong(String) converts through double and
                    // rounds these 64-bit XI addresses. Parse the decimal text.
                    program.append(put(Long.parseLong(field.getString(0)),value));
                }
                source=program.append(body).toString();
            }
            ResultPacket result=ResultPacket.parse(NativeRuntime.execute(state,source));
            if(lesson.has("expected")&&!lesson.getString("expected").equals(speech(result.output)))throw new IOException("La enseñanza entra en conflicto con el vocabulario de esta sesión");
            state=result.state;
        }
        return state;
    }
    private static String speech(String output)throws Exception{
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        for(String row:output.split("\\n"))if(row.startsWith("{")){
            JSONObject item=new JSONObject(row);String register=item.optString("register");
            if(!"r14".equals(register)&&!"r9".equals(register))continue;
            int b=item.getInt("value");if(b<0||b>255||bytes.size()>=4096)throw new IOException("Salida XI no válida");bytes.write(b);
        }
        return StandardCharsets.UTF_8.newDecoder().decode(java.nio.ByteBuffer.wrap(bytes.toByteArray())).toString();
    }
    private static String put(long address,int value){return "store8 "+address+" "+value+"\n";}
    static byte[] observe(byte[] state,JSONObject observation)throws Exception{
        if(observation==null)return state;
        int active=observation.getInt("active"),count=observation.getInt("count"),space=observation.getInt("space"),minCount=observation.getInt("minimizedCount");
        JSONArray present=observation.getJSONArray("present"),minimized=observation.getJSONArray("minimized"),maximized=observation.getJSONArray("maximized"),programs=observation.getJSONArray("programs");
        String view=observation.getString("view");
        if(active<0||active>31||count<0||count>128||present.length()!=32||minimized.length()!=32||maximized.length()!=32||view.length()>65536||space<0||space>1||minCount<0||minCount>count||programs.length()>16)throw new IOException("Observación del escritorio inválida");
        StringBuilder source=new StringBuilder("#!reverse-pc-machine-v1\n");
        source.append(put(A+16,1)).append(put(A+17,active)).append(put(A+18,count)).append(put(M+710,space)).append(put(M+711,minCount));
        for(int i=0;i<32;i++){
            int n=present.getInt(i),min=minimized.getInt(i),max=maximized.getInt(i);
            if(n<0||n>128||min<0||min>n||max<0||max>n)throw new IOException("Cantidad de ventanas inválida");
            source.append(put(A+32+i,n)).append(put(M+640+i,min)).append(put(M+672+i,max)).append(put(M+512+i,0));
        }
        for(int i=0;i<programs.length();i++){
            JSONObject app=programs.getJSONObject(i);int target=app.getInt("target"),cap=app.getInt("capability");
            if(target!=i+16||cap<1||cap>2)throw new IOException("Catálogo de programas inválido");
            source.append(put(M+512+target,cap));
        }
        return ResultPacket.parse(NativeRuntime.execute(state,source.toString())).state;
    }
    static JSONObject action(byte[] state)throws Exception{
        String source="#!reverse-pc-machine-v1\nload8 r0 "+(A+64)+"\nprint r0\nload8 r1 "+(A+65)+"\nprint r1\n";
        ResultPacket read=ResultPacket.parse(NativeRuntime.execute(state,source));int[] values=new int[2];int i=0;
        for(String row:read.output.split("\\n"))if(row.startsWith("{")){JSONObject item=new JSONObject(row);if("register".equals(item.optString("type"))){if(i>=2)throw new IOException("Respuesta de control inválida");values[i++]=item.getInt("value");}}
        if(i!=2)throw new IOException("Respuesta de control incompleta");
        int op=values[0],target=values[1];if(op==0)return null;
        boolean valid=(op==10&&target>=16&&target<=31)||(op==1&&((target>=1&&target<=10)||(target>=16&&target<=31)))||(op>=2&&op<=6&&((target>=1&&target<=10)||target==15||(target>=16&&target<=31)))||((op==7||op==8)&&target==0)||(op==9&&(target==13||target==14));
        if(!valid)throw new IOException("El programa devolvió una acción fuera del contrato");
        return new JSONObject().put("op",op).put("target",target);
    }
    static JSONObject trace(byte[] state)throws Exception{
        int[] offsets={16,17,18,19,20,32,33,34};StringBuilder source=new StringBuilder("#!reverse-pc-machine-v1\n");
        for(int offset:offsets)source.append("load8 r0 ").append(M+offset).append("\nprint r0\n");
        ResultPacket read=ResultPacket.parse(NativeRuntime.execute(state,source.toString()));int[] values=new int[8];int n=0;
        for(String row:read.output.split("\\n"))if(row.startsWith("{")){JSONObject item=new JSONObject(row);if("register".equals(item.optString("type"))){if(n>=8)throw new IOException("Traza inválida");values[n++]=item.getInt("value");}}
        if(n!=8)throw new IOException("Traza incompleta");
        return new JSONObject().put("engine","W4u").put("version","0.1").put("status",values[0]).put("goal",values[1]).put("target",values[2]).put("steps",values[3]).put("completed",values[4]).put("planner",values[5]).put("explorer",values[6]).put("verifier",values[7]);
    }
    static byte[] consume(Context ctx,byte[] state)throws Exception{
        return ResultPacket.parse(NativeRuntime.execute(state,new String(asset(ctx,"bbyq-interface/consume.xi"),StandardCharsets.UTF_8))).state;
    }
    static ResultPacket feedback(Context ctx,byte[] state,int language,boolean success)throws Exception{
        String body=new String(asset(ctx,"bbyq-interface/feedback.xi"),StandardCharsets.UTF_8);
        body=body.substring(body.indexOf('\n')+1);
        String source="#!reverse-pc-machine-v1\n"+put(7400000000000000003L,language)+put(M+80,success?1:0)+body;
        ResultPacket result=ResultPacket.parse(NativeRuntime.execute(state,source));ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        for(String row:result.output.split("\\n"))if(row.startsWith("{")){
            JSONObject item=new JSONObject(row);if(!"r14".equals(item.optString("register")))continue;
            int b=item.getInt("value");if(b<0||b>255||bytes.size()>=4096)throw new IOException("Confirmación XI inválida");bytes.write(b);
        }
        if(bytes.size()==0)throw new IOException("Falta la confirmación XI");
        String text=StandardCharsets.UTF_8.newDecoder().decode(java.nio.ByteBuffer.wrap(bytes.toByteArray())).toString();
        return ResultPacket.parse(ResultPacket.encode(text,result.state));
    }
}
