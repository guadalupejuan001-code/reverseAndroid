package com.reversepc.os;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Android transport for the user's original XI-only BBY Q programs.
 * NO semantic learner, predicate inference, lexical parser, or answer generator
 * exists here. This adapter only passes bytes to the same native REVERSE-PC
 * QuantumRuntime and publishes exactly one successful RVPC2 transaction.
 *
 * The OS session and the BBY Q organism are one Ether memory authority.
 * A failure before commit never replaces the user's existing session.rvp.
 */
final class BbyqBridge {
    private static final String TAG="BbyqOS";
    private static final long LANGUAGE=7100000000000000000L;
    private static final String PREFIX="bbyq/";
    interface Progress { void show(String message); }

    static final class Status {
        final int machine, banks, language, words, concepts, facts;
        Status(long[] regs) {
            machine=(int)regs[0];banks=(int)regs[1];language=(int)regs[2];
            words=(int)regs[3];concepts=(int)regs[4];facts=(int)regs[5];
        }
        boolean ready(){return machine==66&&banks==4&&language==76&&words>=1&&facts>=1;}
        String text(){return ready() ? "4 bancos Ether · "+words+" palabras · "+
            concepts+" conceptos · "+facts+" relaciones" :
            "Modelo aún sin instalar: BBY Q se inicializará en Ether";}
    }
    static final class Answer {
        final String text;
        final int status, nextStudy;
        Answer(String text,int status,int nextStudy) {
            this.text=text;this.status=status;this.nextStudy=nextStudy;
        }
    }
    private BbyqBridge(){}

    private static String asset(Context ctx,String path) throws IOException {
        try(InputStream in=ctx.getAssets().open(PREFIX+path)) {
            ByteArrayOutputStream out=new ByteArrayOutputStream();
            byte[] buffer=new byte[8192];
            int len;
            while((len=in.read(buffer))!=-1) {
                if(out.size()+len>256*1024)throw new IOException("Programa XI demasiado grande: "+path);
                out.write(buffer,0,len);
            }
            return out.toString(StandardCharsets.UTF_8.name());
        }
    }

    private static ResultPacket execute(byte[] state,String program) {
        return ResultPacket.parse(NativeRuntime.execute(state,program));
    }
    private static byte[] xi(byte[] state,String code) {
        return execute(state,code).state;
    }
    private static byte[] app(Context ctx,byte[] state,String path) throws IOException {
        return xi(state,asset(ctx,path));
    }
    private static long[] numbers(String jsonl) throws IOException {
        ArrayList<Long> values=new ArrayList<>();
        for(String line:jsonl.split("\\n")) {
            if(line.trim().isEmpty())continue;
            try {
                JSONObject item=new JSONObject(line);
                if("register".equals(item.optString("type")))
                    values.add(item.getLong("value"));
            }catch(Exception error) {
                throw new IOException("Respuesta XI inválida: "+error.getMessage(),error);
            }
        }
        long[] result=new long[values.size()];
        for(int i=0;i<values.size();i++)result[i]=values.get(i);
        return result;
    }
    static Status status(byte[] state) throws IOException {
        String src="#!reverse-pc-machine-v1\n"
            +"load8 r0 7000000000000000000\nprint r0\n"
            +"load8 r1 7000000000000000610\nprint r1\n"
            +"load8 r2 7100000000000000000\nprint r2\n"
            +"load8 r3 7100000000000000016\nprint r3\n"
            +"load8 r4 7100000000000000017\nprint r4\n"
            +"load8 r5 7100000000000000018\nprint r5\n";
        long[] observed=numbers(execute(state,src).output);
        if(observed.length!=6)throw new IOException("BBY Q: lectura de estado incompleta");
        return new Status(observed);
    }

    private static void report(Progress callback,String message) {
        if(callback!=null)callback.show(message);
    }
    static Status install(Context ctx,SessionStore store,Progress update) throws IOException {
        // Stage every operation in memory, publish once after ALL original
        // XI learning, jury, lexical and proof programs succeed.
        final byte[] before=store.loadState();
        byte[] work=before;
        Status initial=status(work);
        if(initial.ready()){
            work=app(ctx,work,"e008/audit-organism.xi");
            work=app(ctx,work,"language/audit.xi");
            installProgramEntries(ctx,store);
            return status(work);
        }
        if(initial.machine==0&&initial.banks==0) {
            report(update,"Creando los cuatro conocimientos originales…");
            work=app(ctx,work,"e006/birth.xi");
            work=app(ctx,work,"e006/seed-42.xi");
            for(String task:new String[]{"xor","xnor","and","or"}) {
                if(!"xor".equals(task))work=app(ctx,work,"e008/open-shadow.xi");
                for(int epoch=0;epoch<12;epoch++){
                    work=app(ctx,work,"xor".equals(task)?
                        "e006/feed-xor.xi":"e008/train-"+task+".xi");
                    work=app(ctx,work,"e006/learn.xi");
                }
                work=app(ctx,work,"xor".equals(task)?
                    "e006/validate-feed-xor.xi":"e008/admit-feed-"+task+".xi");
                ResultPacket admitted=execute(work,asset(ctx,"e008/admit.xi"));
                long[] results=numbers(admitted.output);
                if(results.length<3||results[2]!=1)
                    throw new IOException("BBY Q: hipótesis "+task+" no superó el jurado");
                work=admitted.state;
                work=app(ctx,work,"e008/bank-store.xi");
                report(update,"Conocimiento XI validado: "+task.toUpperCase(Locale.ROOT));
            }
        }else if(initial.machine!=66||initial.banks!=4) {
            throw new IOException("Hay un estado BBY Q/Ether parcial o incompatible; "+
                "no lo sobrescribí. Conserva tu RVPC2 y revisa XI Studio.");
        }
        work=app(ctx,work,"e008/audit-organism.xi");
        Status installed=status(work);
        if(installed.language==0) {
            report(update,"Aprendiendo vocabulario y relaciones originales…");
            work=app(ctx,work,"language/birth.xi");
            String curriculum=asset(ctx,"curriculum-v1.txt");
            for(String raw:curriculum.split("\\n")) {
                String line=raw.trim();
                if(line.isEmpty()||line.startsWith("#"))continue;
                String[] parts=line.split("\\s+");
                if(parts.length==5&&"word".equals(parts[0])) {
                    work=learnWord(ctx,work,parts[1],
                        Integer.parseInt(parts[2]),Integer.parseInt(parts[3]),
                        Integer.parseInt(parts[4]));
                }else if(parts.length==5&&"fact".equals(parts[0])) {
                    work=learnFact(ctx,work,parts[1],parts[2],parts[3],
                        "1".equals(parts[4]));
                    work=learnFact(ctx,work,parts[1],parts[2],parts[3],
                        "1".equals(parts[4]));
                }else throw new IOException("Lección XI inválida: "+line);
            }
            work=app(ctx,work,"language/calibrate.xi");
        }else if(installed.language!=76) {
            throw new IOException("Firma lingüística desconocida: no alteré tu memoria");
        }
        work=app(ctx,work,"language/audit.xi");
        Status completed=status(work);
        if(!completed.ready())throw new IOException("BBY Q no completó sus verificaciones");
        // Validate that the exact trained relation actually produces new text.
        AnswerCheck smoke=answerInMemory(ctx,work,"cuarto contiene llave?");
        if(!"cuarto contiene llave (via caja).".equals(smoke.answer.text))
            throw new IOException("BBY Q: su conocimiento no superó la prueba de diálogo");
        work=app(ctx,smoke.state,"e008/audit-organism.xi");
        work=app(ctx,work,"language/audit.xi");
        // ONE atomic publication protects pre-existing desktop XI programs,
        // Ether memory and even any qubits from intermediate boot failures.
        store.writeState(work);
        installProgramEntries(ctx,store);
        Log.i(TAG,"BBYQ_INSTALL_OK words="+completed.words+" facts="+completed.facts);
        report(update,"BBY Q instalado en el mismo núcleo Ether.");
        return status(store.loadState());
    }

    private static void installProgramEntries(Context ctx,SessionStore store)throws IOException {
        List<String> available=store.listPrograms();
        for(String name:new String[]{"readline","ask","reflect","speak",
                                     "ground","relate","calibrate","audit"}) {
            String installed="BBYQ_"+name+".xi";
            if(!available.contains(installed))
                store.saveProgram(installed,asset(ctx,"language/"+name+".xi"));
        }
    }
    private static String word(String source) {
        if(source==null||!source.matches("[a-z]{1,8}"))
            throw new IllegalArgumentException("BBY Q admite palabras ASCII a-z de 1 a 8 letras");
        return source;
    }
    private static String put(long address,int value) {
        return "store8 "+address+" "+value+"\n";
    }
    private static String nativeWord(long address,String token) {
        token=word(token);
        StringBuilder s=new StringBuilder(put(address,token.length()));
        for(int i=0;i<token.length();i++)s.append(put(address+1+i,token.charAt(i)));
        return s.toString();
    }
    private static String prefix(){return "#!reverse-pc-machine-v1\n";}
    private static byte[] learnWord(Context ctx,byte[] state,String token,int s0,int s1,int s2)
        throws IOException {
        if(s0<0||s0>3||s1<0||s1>3||s2<0||s2>3)
            throw new IllegalArgumentException("Cada sensor debe estar entre 0 y 3");
        String src=prefix()+put(LANGUAGE+125,1)+nativeWord(LANGUAGE+100,token)
            +put(LANGUAGE+120,s0)+put(LANGUAGE+121,s1)+put(LANGUAGE+122,s2);
        return app(ctx,xi(state,src),"language/ground.xi");
    }
    private static byte[] learnFact(Context ctx,byte[] state,String subject,
                                    String predicate,String object,boolean trueFact)
        throws IOException {
        String src=prefix()+put(LANGUAGE+175,2)+nativeWord(LANGUAGE+140,subject)
            +nativeWord(LANGUAGE+150,predicate)+nativeWord(LANGUAGE+160,object)
            +put(LANGUAGE+170,trueFact?1:0);
        return app(ctx,xi(state,src),"language/relate.xi");
    }
    static Status teachWord(Context ctx,SessionStore store,String token,int s0,int s1,int s2)
        throws IOException {
        byte[] start=store.loadState();
        if(!status(start).ready())throw new IOException("Instala primero BBY Q");
        byte[] working=learnWord(ctx,start,token,s0,s1,s2);
        working=app(ctx,working,"language/audit.xi");
        store.writeState(working);
        Log.i(TAG,"BBYQ_TEACH_WORD_OK");
        return status(working);
    }
    static Status teachFact(Context ctx,SessionStore store,String s,String p,String o,
                            boolean observed) throws IOException {
        byte[] start=store.loadState();
        if(!status(start).ready())throw new IOException("Instala primero BBY Q");
        byte[] working=start;
        // Two independent observational confirmations for a direct fact.
        working=learnFact(ctx,working,s,p,o,observed);
        working=learnFact(ctx,working,s,p,o,observed);
        working=app(ctx,working,"language/calibrate.xi");
        working=app(ctx,working,"language/audit.xi");
        store.writeState(working);
        Log.i(TAG,"BBYQ_TEACH_RELATION_OK");
        return status(working);
    }
    private static final class AnswerCheck {
        final byte[] state;
        final Answer answer;
        AnswerCheck(byte[] state,Answer answer){this.state=state;this.answer=answer;}
    }
    private static AnswerCheck answerInMemory(Context ctx,byte[] before,String question)
        throws IOException {
        if(question==null||question.isEmpty())throw new IOException("Introduce una pregunta");
        byte[] bytes=question.getBytes(StandardCharsets.US_ASCII);
        if(bytes.length>32)throw new IOException("Esta versión admite hasta 32 caracteres");
        for(int i=0;i<question.length();i++) {
            char c=question.charAt(i);
            if(c>127||c<32)throw new IOException("Solo texto ASCII simple en E009");
        }
        StringBuilder src=new StringBuilder(prefix());
        src.append(put(LANGUAGE+175,4)).append(put(LANGUAGE+400,bytes.length));
        for(int i=0;i<bytes.length;i++)
            src.append(put(LANGUAGE+401+i,bytes[i]&255));
        byte[] working=xi(before,src.toString());
        working=app(ctx,working,"language/readline.xi"); // Grammar is XI, not Java.
        ResultPacket proof=execute(working,asset(ctx,"language/ask.xi"));
        long[] sem=numbers(proof.output);
        if(sem.length<1)throw new IOException("No hubo salida de inferencia XI");
        working=proof.state;
        ResultPacket meta=execute(working,asset(ctx,"language/reflect.xi"));
        long[] thoughts=numbers(meta.output);
        if(thoughts.length<1)throw new IOException("No hubo decisión de aprendizaje XI");
        ResultPacket speech=execute(meta.state,asset(ctx,"language/speak.xi"));
        StringBuilder language=new StringBuilder();
        for(long n:numbers(speech.output)) {
            if(n<32||n>126)throw new IOException("Salida ASCII corrupta: "+n);
            language.append((char)n);
        }
        byte[] examined=app(ctx,speech.state,"language/audit.xi");
        return new AnswerCheck(examined,new Answer(language.toString(),(int)sem[0],
                                                   (int)thoughts[0]));
    }
    static Answer ask(Context ctx,SessionStore store,String question) throws IOException {
        byte[] snapshot=store.loadState();
        if(!status(snapshot).ready())throw new IOException("BBY Q no está inicializado");
        long started=SystemClock.elapsedRealtimeNanos();
        AnswerCheck result=answerInMemory(ctx,snapshot,question);
        store.writeState(result.state);
        Log.i(TAG,"BBYQ_ANSWER_OK query="+question+" result="+result.answer.text+
            " ns="+(SystemClock.elapsedRealtimeNanos()-started));
        return result.answer;
    }
}
