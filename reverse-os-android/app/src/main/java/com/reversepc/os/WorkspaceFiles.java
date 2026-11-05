package com.reversepc.os;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Local desktop documents. The original programs directory stays authoritative
 * for XI scripts, while RVPC2 remains exclusively owned by SessionStore. */
public final class WorkspaceFiles {
    public static final int MAX_FILE=8*1024*1024,MAX_TEXT=256*1024;
    public static final String[] ROOTS={"Escritorio","Documentos","Descargas","Imágenes","Programas","Aplicaciones","Papelera"};
    private final File root,programs,meta;
    public static final class Entry {
        public final String name,path; public final boolean directory;
        public final long bytes,modified;
        Entry(String path,File f){this(path,f,f.getName());}
        Entry(String path,File f,String display){this.path=path;name=display;directory=f.isDirectory();bytes=f.length();modified=f.lastModified();}
    }
    public WorkspaceFiles(File privateFiles)throws IOException{
        root=new File(privateFiles,"workspace").getCanonicalFile();
        programs=new File(privateFiles,"programs").getCanonicalFile();
        meta=new File(privateFiles,"desktop-control").getCanonicalFile();
        for(File f:new File[]{root,programs,meta})if(!f.isDirectory()&&!f.mkdirs())throw new IOException("No se pudo abrir el escritorio");
        for(String name:ROOTS)if(!name.equals("Programas"))new File(root,name).mkdirs();
    }
    public static String name(String value)throws IOException{
        String n=value==null?"":value.trim();
        if(n.isEmpty()||n.length()>100||n.getBytes(StandardCharsets.UTF_8).length>180||n.equals(".")||n.equals("..")||n.startsWith(".")||
           n.contains("/")||n.contains("\\")||n.chars().anyMatch(c->c<32))throw new IOException("Nombre de archivo no válido");
        return n;
    }
    public static String parent(String path){int i=path.lastIndexOf('/');return i<=0?"":path.substring(0,i);}
    public static String join(String path,String child)throws IOException{return path.isEmpty()?name(child):path+"/"+name(child);}
    private File resolve(String path)throws IOException{
        if(path==null||path.isEmpty())return root;
        String[] parts=path.split("/",-1);
        if(!Arrays.asList(ROOTS).contains(parts[0]))throw new IOException("Carpeta fuera del escritorio");
        File base=parts[0].equals("Programas")?programs:root;
        File f=parts[0].equals("Programas")?base:new File(base,parts[0]);
        for(int i=1;i<parts.length;i++)f=new File(f,name(parts[i]));
        for(File p=f;p!=null&&!p.equals(base);p=p.getParentFile())
            if(Files.isSymbolicLink(p.toPath()))throw new IOException("No se admiten enlaces de archivos");
        File canonical=f.getCanonicalFile();
        if(!canonical.equals(base)&&!canonical.getPath().startsWith(base.getPath()+File.separator))throw new IOException("Ruta no válida");
        return canonical;
    }
    public synchronized List<Entry> list(String path)throws IOException{
        List<Entry> result=new ArrayList<>();
        if(path.isEmpty()){
            for(String name:ROOTS)result.add(new Entry(name,resolve(name),name));
        }else{
            File folder=resolve(path);
            if(!folder.isDirectory())throw new IOException("La carpeta no existe");
            File[] all=folder.listFiles();
            if(all!=null)for(File f:all)if(!f.getName().startsWith(".")&&!Files.isSymbolicLink(f.toPath())){
                String display=f.getName();
                if(path.equals("Papelera")){
                    File origin=new File(meta,f.getName()+".origin");
                    if(origin.isFile()){String original=new String(Files.readAllBytes(origin.toPath()),StandardCharsets.UTF_8);display=original.substring(original.lastIndexOf('/')+1);}
                }
                result.add(new Entry(join(path,f.getName()),f,display));
            }
            result.sort(Comparator.comparing((Entry e)->!e.directory).thenComparing(e->e.name.toLowerCase(Locale.ROOT)));
        }
        return result;
    }
    private void mutable(String path)throws IOException{
        if(parent(path).isEmpty())throw new IOException("Las carpetas principales se conservan");
        if(path.startsWith("Papelera/"))throw new IOException("Restaura el archivo antes de modificarlo");
        if(path.startsWith("Programas/")){
            if(!parent(path).equals("Programas")||!path.substring(10).endsWith(".xi")||!path.substring(10).matches("[A-Za-z0-9_][A-Za-z0-9_.-]{0,60}"))
                throw new IOException("Programas acepta archivos XI con nombres ASCII");
        }
    }
    public synchronized void mkdir(String path)throws IOException{
        mutable(path);
        if(path.startsWith("Programas/"))throw new IOException("Guarda los programas en la carpeta Programas");
        File f=resolve(path);
        if(f.exists()||!f.getParentFile().isDirectory()||!f.mkdir())throw new IOException("No se pudo crear la carpeta");
    }
    public synchronized byte[] read(String path)throws IOException{
        File f=resolve(path);
        if(!f.isFile()||f.length()>MAX_FILE)throw new IOException("Archivo no válido o mayor de 8 MiB");
        return Files.readAllBytes(f.toPath());
    }
    public synchronized String text(String path)throws IOException{
        byte[] data=read(path);
        if(data.length>MAX_TEXT)throw new IOException("El editor admite hasta 256 KiB");
        return new String(data,StandardCharsets.UTF_8);
    }
    public synchronized void write(String path,byte[] data,boolean overwrite)throws IOException{
        mutable(path);
        if(data.length>MAX_FILE||(path.startsWith("Programas/")&&data.length>MAX_TEXT))throw new IOException("Archivo demasiado grande");
        File f=resolve(path);
        if(!f.getParentFile().isDirectory()||f.isDirectory()||(!overwrite&&f.exists()))throw new IOException("El destino ya existe o no está disponible");
        File temp=new File(f.getParentFile(),".write-"+UUID.randomUUID());
        try{
            try(FileOutputStream out=new FileOutputStream(temp)){out.write(data);out.getFD().sync();}
            Files.move(temp.toPath(),f.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
        }finally{Files.deleteIfExists(temp.toPath());}
    }
    public synchronized void rename(String path,String newName)throws IOException{
        mutable(path);String dest=join(parent(path),newName);mutable(dest);
        move(path,dest);
    }
    private void move(String path,String dest)throws IOException{
        File from=resolve(path),to=resolve(dest);
        if(!from.exists()||to.exists()||!to.getParentFile().isDirectory()||to.getPath().startsWith(from.getPath()+File.separator))throw new IOException("El destino no está disponible");
        Files.move(from.toPath(),to.toPath(),StandardCopyOption.ATOMIC_MOVE);
    }
    public synchronized void paste(String path,String folder,boolean cut)throws IOException{
        mutable(path);String dest=join(folder,resolve(path).getName());mutable(dest);
        if(cut){move(path,dest);return;}
        File from=resolve(path),to=resolve(dest);
        if(!from.isFile())throw new IOException("Para carpetas utiliza Cortar y Pegar");
        if(from.equals(to))throw new IOException("Elige otra carpeta de destino");
        write(dest,read(path),false);
    }
    public synchronized String trash(String path)throws IOException{
        mutable(path);
        String id=UUID.randomUUID().toString();
        File record=new File(meta,id+".origin");
        Files.write(record.toPath(),path.getBytes(StandardCharsets.UTF_8),StandardOpenOption.CREATE_NEW);
        try{move(path,"Papelera/"+id);}catch(IOException e){record.delete();throw e;}
        return "Papelera/"+id;
    }
    public synchronized void restore(String path)throws IOException{
        if(!parent(path).equals("Papelera"))throw new IOException("Selecciona un archivo de la papelera");
        File record=new File(meta,resolve(path).getName()+".origin");
        if(!record.isFile())throw new IOException("No hay destino original disponible");
        String original=new String(Files.readAllBytes(record.toPath()),StandardCharsets.UTF_8);
        mutable(original);move(path,original);record.delete();
    }
    public synchronized void eraseTrash(String path)throws IOException{
        if(!parent(path).equals("Papelera"))throw new IOException("Solo se eliminan definitivamente elementos de la papelera");
        File target=resolve(path);
        Files.walkFileTree(target.toPath(),new SimpleFileVisitor<Path>(){
            @Override public FileVisitResult visitFile(Path file,java.nio.file.attribute.BasicFileAttributes attributes)throws IOException{Files.delete(file);return FileVisitResult.CONTINUE;}
            @Override public FileVisitResult postVisitDirectory(Path directory,IOException error)throws IOException{if(error!=null)throw error;Files.delete(directory);return FileVisitResult.CONTINUE;}
        });
        Files.deleteIfExists(new File(meta,target.getName()+".origin").toPath());
    }
    public synchronized File image(String path)throws IOException{return resolve(path);}
    public synchronized long modified(String path)throws IOException{return resolve(path).lastModified();}
    public synchronized Map<String,String> trashOrigins()throws IOException{
        Map<String,String> result=new TreeMap<>();File[] records=meta.listFiles((d,n)->n.endsWith(".origin"));
        if(records!=null)for(File f:records)result.put(f.getName().replace(".origin",""),new String(Files.readAllBytes(f.toPath()),StandardCharsets.UTF_8));
        return result;
    }
}
