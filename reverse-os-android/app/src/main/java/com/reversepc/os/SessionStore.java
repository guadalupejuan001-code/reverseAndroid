package com.reversepc.os;
import android.content.Context;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class SessionStore {
    private static final int MAX_PROGRAM=256*1024;
    private final File programs;
    private final AtomicFile state;
    SessionStore(Context context) throws IOException {
        programs=new File(context.getFilesDir(),"programs");
        if(!programs.isDirectory()&&!programs.mkdirs())
            throw new IOException("Could not create program folder");
        state=new AtomicFile(new File(context.getFilesDir(),"session.rvp"));
        if(!state.getBaseFile().exists())writeState(NativeRuntime.initialize());
        samples();
    }
    private static void atomic(AtomicFile target,byte[] content)throws IOException{
        FileOutputStream out=null;
        try {
            out=target.startWrite();
            out.write(content);
            target.finishWrite(out);
        } catch(IOException|RuntimeException e){
            if(out!=null)target.failWrite(out);
            throw e;
        }
    }
    public synchronized byte[] loadState()throws IOException{return state.readFully();}
    public synchronized void writeState(byte[] data)throws IOException{
        if(data==null||data.length>32*1024*1024+29)throw new IOException("Invalid state size");
        // Import validation must occur before any persistent mutation.
        NativeRuntime.inspect(data);
        atomic(state,data);
    }
    public synchronized void reset()throws IOException{writeState(NativeRuntime.initialize());}
    public static String normalized(String name){
        String n=name==null?"":name.trim();
        if(!n.matches("[A-Za-z0-9_][A-Za-z0-9_.-]{0,60}"))
            throw new IllegalArgumentException("Nombre: usa letras ASCII, números, guiones o _");
        if(n.equals(".")||n.equals(".."))throw new IllegalArgumentException("Nombre inválido");
        return n.endsWith(".xi")?n:n+".xi";
    }
    private File file(String name)throws IOException{
        File f=new File(programs,normalized(name));
        if(!f.getCanonicalFile().getParentFile().equals(programs.getCanonicalFile()))
            throw new IOException("Program path escapes sandbox");
        return f;
    }
    public synchronized void saveProgram(String name,String contents)throws IOException{
        byte[] bytes=contents.getBytes(StandardCharsets.UTF_8);
        if(bytes.length>MAX_PROGRAM)throw new IOException("Programa supera 256 KiB");
        atomic(new AtomicFile(file(name)),bytes);
    }
    public synchronized String readProgram(String name)throws IOException{
        byte[] bytes=new AtomicFile(file(name)).readFully();
        if(bytes.length>MAX_PROGRAM)throw new IOException("Programa supera 256 KiB");
        return new String(bytes,StandardCharsets.UTF_8);
    }
    public synchronized void deleteProgram(String name)throws IOException{
        File f=file(name);
        if(!f.exists()||!f.delete())throw new IOException("No pude eliminar "+name);
    }
    public synchronized List<String> listPrograms(){
        File[] files=programs.listFiles((dir,name)->name.endsWith(".xi")&&
                name.matches("[A-Za-z0-9_][A-Za-z0-9_.-]{0,60}"));
        List<String> names=new ArrayList<>();
        if(files!=null)for(File f:files)if(f.isFile())names.add(f.getName());
        names.sort(String.CASE_INSENSITIVE_ORDER);
        return names;
    }
    public long programBytes(String name){
        try{return file(name).length();}catch(IOException e){return -1;}
    }
    private void sample(String name,String code)throws IOException{
        if(!file(name).exists())saveProgram(name,code);
    }
    private void samples()throws IOException{
        sample("01_euclides.xi",
            "#!reverse-pc-machine-v1\n"
           +"# Euclides con registros de 64 bits, sin oraculo de respuestas.\n"
           +"set r0 1071\nset r1 462\nlabel ciclo\ncmp r1 0\nje listo\n"
           +"mov r2 r0\nmod r2 r1\nmov r0 r1\nmov r1 r2\njmp ciclo\n"
           +"label listo\nstore8 42 r0\nprint r0\nmem_read 42\nhalt\n");
        sample("02_fibonacci.xi",
            "#!reverse-pc-machine-v1\n"
           +"# 12 iteraciones; resultado 144 en byte de memoria 80.\n"
           +"set r0 0\nset r1 1\nset r2 0\n"
           +"label ciclo\nmov r3 r0\nadd r3 r1\nmov r0 r1\nmov r1 r3\n"
           +"add r2 1\ncmp r2 12\njlt ciclo\n"
           +"store8 80 r0\nprint r0\nmem_read 80\nhalt\n");
        sample("03_ghz128.xi",ghz());
        sample("04_ether_espacio.xi",
            "#!reverse-pc-machine-v1\n"
           +"# Regla Ether global; consulta sin materializar direcciones intermedias.\n"
           +"h_all\nprobz 9999999999999999999999999999999999999999999999999999999999999999\n"
           +"qstats\nhalt\n");
    }
    private String ghz(){
        return "#!reverse-pc-machine-v1\n"
           +"# GHZ de 128 qubits: q200 a q327. Puertas reales con direcciones calculadas.\n"
           +"h 200\nset r0 200\n"
           +"label expandir\nmov r1 r0\nadd r1 1\ncx $r0 $r1\n"
           +"mov r0 r1\ncmp r0 327\njlt expandir\n"
           +"t 327\nexpect Z:200 Z:327\nqstats\nhalt\n";
    }
}
