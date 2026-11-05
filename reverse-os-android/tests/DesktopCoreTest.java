package com.reversepc.os;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.math.BigDecimal;

/** Real filesystem operations and independently specified calculator answers. */
public final class DesktopCoreTest {
    private static int checks;
    interface Checked {void run()throws Exception;}
    static void yes(boolean condition){if(!condition)throw new AssertionError("check "+checks);checks++;}
    static void reject(Checked action)throws Exception{try{action.run();}catch(IOException|IllegalArgumentException ex){checks++;return;}throw new AssertionError("invalid action accepted");}
    static byte[] bytes(String text){return text.getBytes(StandardCharsets.UTF_8);}
    public static void main(String[] args)throws Exception{
        File out=new File(args[0]);out.mkdirs();File privateFiles=new File(out,"files");privateFiles.mkdirs();
        WorkspaceFiles fs=new WorkspaceFiles(privateFiles);
        yes(fs.list("").size()==7);
        fs.mkdir("Documentos/Árbol");fs.write("Documentos/Árbol/Nota de X.txt",bytes("Hola, memoria ñ"),false);
        yes(fs.text("Documentos/Árbol/Nota de X.txt").equals("Hola, memoria ñ"));
        fs.paste("Documentos/Árbol/Nota de X.txt","Descargas",false);
        fs.rename("Descargas/Nota de X.txt","Copia.txt");yes(fs.text("Descargas/Copia.txt").equals("Hola, memoria ñ"));
        reject(()->fs.write("Descargas/Copia.txt",bytes("perdido"),false));yes(fs.text("Descargas/Copia.txt").equals("Hola, memoria ñ"));
        fs.paste("Descargas/Copia.txt","Escritorio",true);yes(fs.list("Descargas").isEmpty());
        String trashed=fs.trash("Escritorio/Copia.txt");yes(fs.list("Papelera").get(0).name.equals("Copia.txt"));
        fs.restore(trashed);yes(fs.text("Escritorio/Copia.txt").equals("Hola, memoria ñ"));
        String folder=fs.trash("Documentos/Árbol");fs.restore(folder);yes(fs.text("Documentos/Árbol/Nota de X.txt").startsWith("Hola"));
        String conflict=fs.trash("Escritorio/Copia.txt");fs.write("Escritorio/Copia.txt",bytes("nuevo"),false);
        reject(()->fs.restore(conflict));yes(fs.text("Escritorio/Copia.txt").equals("nuevo"));yes(fs.text(conflict).equals("Hola, memoria ñ"));
        fs.eraseTrash(conflict);yes(fs.list("Papelera").isEmpty());yes(fs.trashOrigins().isEmpty());
        reject(()->fs.eraseTrash("Escritorio/Copia.txt"));yes(fs.text("Escritorio/Copia.txt").equals("nuevo"));
        reject(()->fs.rename("Documentos","roto"));reject(()->fs.mkdir("Documentos/../afuera"));
        reject(()->fs.write("Descargas/error.txt",new byte[WorkspaceFiles.MAX_FILE+1],false));yes(fs.list("Descargas").isEmpty());
        reject(()->fs.write("Programas/nombre inválido.xi",bytes("halt"),false));
        fs.write("Programas/prueba.xi",bytes("#!reverse-pc-machine-v1\nhalt\n"),false);yes(fs.list("Programas").get(0).name.equals("prueba.xi"));
        Path outside=new File(out,"outside.txt").toPath();Files.write(outside,bytes("afuera"));
        Path link=new File(privateFiles,"workspace/Documentos/enlace.txt").toPath();Files.createSymbolicLink(link,outside.toAbsolutePath());
        reject(()->fs.read("Documentos/enlace.txt"));reject(()->fs.write("Documentos/enlace.txt",bytes("no"),true));yes(new String(Files.readAllBytes(outside),StandardCharsets.UTF_8).equals("afuera"));
        WindowGeometry geometry=new WindowGeometry(-300,900,1500,800);geometry.fit(900,320,280,180);
        yes(geometry.x==0&&geometry.y==0&&geometry.width==900&&geometry.height==320);
        geometry.snap(-1,901,320);yes(geometry.width==450&&geometry.x==0);geometry.snap(1,901,320);yes(geometry.x==450&&geometry.width==451);
        geometry.snap(0,901,320);yes(geometry.width==901&&geometry.height==320);
        CalculatorCompiler compiler=new CalculatorCompiler();
        String[][] cases={{"2.5*(3+1)","10"},{"1071/3","357"},{"1/3","0.333333"},{"-2*3.5","-7"},
            {"10-3*2","4"},{"(10-3)*2","14"},{"0.25+0.75","1"},{"9/2","4.5"},{"-9/2","-4.5"},
            {"0*9999999","0"},{"100000*200000","20000000000"},{"123456789/3","41152263"},{"0.000001+0.000002","0.000003"},
            {"2×(7−3)÷4","2"},{"3,5+2,5","6"},{"-(3+4)*2","-14"},{"4/(2/4)","8"},{"1/0","ERROR"},
            {"9000000000000+9000000000000","ERROR"}};
        StringBuilder expected=new StringBuilder();
        for(int i=0;i<cases.length;i++){
            String source=compiler.compile(cases[i][0]);File file=new File(out,"calc-"+i+".xi");Files.write(file.toPath(),bytes(source));
            expected.append(file.getName()).append('\t').append(cases[i][1].equals("ERROR")?"ERROR":new BigDecimal(cases[i][1]).movePointRight(6).longValueExact()).append('\n');
        }
        for(String invalid:new String[]{"","2+","1e3","set r0 9","()","0.1234567","2;3","1/2 xyz"})reject(()->compiler.compile(invalid));
        Files.write(new File(out,"calculator-expected.tsv").toPath(),bytes(expected.toString()));
        System.out.println("DESKTOP_CORE_PASS checks="+checks+" native_calculator_cases="+cases.length);
    }
}
