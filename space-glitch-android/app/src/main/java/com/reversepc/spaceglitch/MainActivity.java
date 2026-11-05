package com.reversepc.spaceglitch;

import android.app.Activity;
import android.content.ContentValues;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** Standalone offline Android GLSurfaceView app, zero WebView/Internet. */
public final class MainActivity extends Activity implements SpaceRenderer.Listener {
    private static final int CYAN=Color.rgb(100,244,255);
    private static final int PURPLE=Color.rgb(204,115,255);
    private static final int WHITE=Color.rgb(230,244,255);
    private static final int MUTED=Color.rgb(151,183,207);

    private GLSurfaceView canvas;
    private SpaceRenderer renderer;
    private LinearLayout controls;
    private TextView performance;
    private TextView modeCaption;
    private Button[] modes,qualities;
    private Button qualityButton, captureButton;
    private boolean closed=false;
    private float lastX,lastY,lastPinch;
    private float dragged=0f;
    private int quality=0;

    private int dp(float v){
        return Math.round(v*getResources().getDisplayMetrics().density);
    }

    private GradientDrawable shape(int fill,int radius,int stroke){
        GradientDrawable d=new GradientDrawable();
        d.setColor(fill);
        d.setCornerRadius(dp(radius));
        if(stroke!=0)d.setStroke(dp(1),stroke);
        return d;
    }

    private TextView text(String value,int sp,int color,boolean bold){
        TextView result=new TextView(this);
        result.setText(value);
        result.setTextColor(color);
        result.setTextSize(sp);
        if(bold)result.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        result.setIncludeFontPadding(false);
        return result;
    }

    private Button button(String title){
        Button b=new Button(this);
        b.setText(title);
        b.setAllCaps(false);
        b.setTextSize(11);
        b.setPadding(dp(2),0,dp(2),0);
        b.setTextColor(WHITE);
        b.setMinWidth(0);b.setMinimumWidth(0);
        b.setMinHeight(0);b.setMinimumHeight(0);
        b.setBackground(shape(0xff142239,9,0xff30435e));
        return b;
    }

    private static void select(Button b,boolean active){
        b.setTextColor(active?0xff021018:0xffc4daed);
        GradientDrawable d=new GradientDrawable();
        d.setColor(active?0xff64f4ff:0xff17243a);
        d.setCornerRadius(18f);
        d.setStroke(1,active?0xffa0ffff:0xff3b526c);
        b.setBackground(d);
    }

    @Override protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        immersive();

        FrameLayout screen=new FrameLayout(this);
        screen.setBackgroundColor(0xff00020a);
        canvas=new GLSurfaceView(this);
        canvas.setEGLContextClientVersion(3);
        canvas.setEGLConfigChooser(8,8,8,8,16,0);
        canvas.setPreserveEGLContextOnPause(true);
        renderer=new SpaceRenderer(this,this);
        canvas.setRenderer(renderer);
        canvas.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        canvas.setContentDescription("space-glitch-gpu-canvas");
        screen.addView(canvas,new FrameLayout.LayoutParams(-1,-1));
        installGestureControls();

        LinearLayout top=new LinearLayout(this);
        top.setOrientation(LinearLayout.VERTICAL);
        top.setBackground(shape(0xd8091024,14,0xff354e77));
        top.setPadding(dp(14),dp(12),dp(14),dp(12));
        FrameLayout.LayoutParams tlp=new FrameLayout.LayoutParams(-1,-2,Gravity.TOP);
        tlp.setMargins(dp(10),dp(15),dp(10),0);
        screen.addView(top,tlp);
        LinearLayout topRow=new LinearLayout(this);
        topRow.setGravity(Gravity.CENTER_VERTICAL);
        top.addView(topRow);
        LinearLayout logo=new LinearLayout(this);
        logo.setOrientation(LinearLayout.VERTICAL);
        topRow.addView(logo,new LinearLayout.LayoutParams(0,-2,1f));
        TextView name=text("SPACE GLITCH / V2",19,CYAN,true);
        name.setLetterSpacing(.12f);
        logo.addView(name);
        logo.addView(text("EMULADOR VISUAL • GPU PROCEDURAL",9,MUTED,false));
        Button toggle=button("OCULTAR PANEL");
        toggle.setTextSize(10);
        topRow.addView(toggle,new LinearLayout.LayoutParams(dp(118),dp(42)));
        performance=text("GPU INICIANDO…",11,WHITE,true);
        performance.setContentDescription("space-glitch-fps");
        LinearLayout.LayoutParams plp=new LinearLayout.LayoutParams(-1,-2);
        plp.topMargin=dp(7);
        top.addView(performance,plp);

        controls=new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setBackground(shape(0xee080f23,18,0xff3e5676));
        controls.setPadding(dp(12),dp(10),dp(12),dp(12));
        FrameLayout.LayoutParams bottom=new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM);
        bottom.setMargins(dp(10),0,dp(10),dp(12));
        screen.addView(controls,bottom);
        toggle.setOnClickListener(v->{
            boolean visible=controls.getVisibility()==View.VISIBLE;
            controls.setVisibility(visible?View.GONE:View.VISIBLE);
            toggle.setText(visible?"MOSTRAR PANEL":"OCULTAR PANEL");
        });

        modeCaption=text("01 / PORTAL DE SINGULARIDAD",12,CYAN,true);
        controls.addView(modeCaption);
        LinearLayout modeRow=new LinearLayout(this);
        modeRow.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams modeParams=new LinearLayout.LayoutParams(-1,dp(43));
        modeParams.topMargin=dp(9);
        controls.addView(modeRow,modeParams);
        String[] labels={"PORTAL","FRACTURA","ÉTER","ESPEJO"};
        String[] captions={"01 / PORTAL DE SINGULARIDAD","02 / FRACTURA DIGITAL",
                "03 / CAMPO DE ÉTER","04 / SIMETRÍA DE ESPEJO"};
        modes=new Button[4];
        for(int i=0;i<4;i++){
            final int idx=i;
            Button b=button(labels[i]);
            modes[i]=b;
            LinearLayout.LayoutParams bp=new LinearLayout.LayoutParams(0,-1,1f);
            if(i>0)bp.leftMargin=dp(5);
            modeRow.addView(b,bp);
            select(b,i==0);
            b.setOnClickListener(v->{
                renderer.mode=idx;
                modeCaption.setText(captions[idx]);
                for(int j=0;j<modes.length;j++)select(modes[j],j==idx);
                renderer.pulse(.5f,.5f);
            });
        }
        createSlider("INTENSIDAD GLITCH",52,progress->renderer.glitch=progress/100f);
        createSlider("DISTORSIÓN ESPACIAL",57,progress->renderer.warp=.15f+1.85f*progress/100f);
        createSlider("VELOCIDAD",47,progress->renderer.speed=.2f+3.3f*progress/100f);

        LinearLayout actions=new LinearLayout(this);
        actions.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams ap=new LinearLayout.LayoutParams(-1,dp(47));
        ap.topMargin=dp(9);
        controls.addView(actions,ap);
        Button pulse=button("PULSO");
        actions.addView(pulse,new LinearLayout.LayoutParams(0,-1,1f));
        pulse.setOnClickListener(v->renderer.pulse(.5f,.5f));
        qualityButton=button("CALIDAD AUTO");
        LinearLayout.LayoutParams qp=new LinearLayout.LayoutParams(0,-1,1.45f);
        qp.leftMargin=dp(6);
        actions.addView(qualityButton,qp);
        qualityButton.setOnClickListener(v->{
            quality=(quality+1)%3;
            renderer.profile=quality;
            qualityButton.setText(quality==0?"CALIDAD AUTO":quality==1?"CALIDAD ECO":"CALIDAD ULTRA");
        });
        captureButton=button("GUARDAR PNG");
        LinearLayout.LayoutParams cp=new LinearLayout.LayoutParams(0,-1,1.45f);
        cp.leftMargin=dp(6);
        actions.addView(captureButton,cp);
        captureButton.setOnClickListener(v->{
            captureButton.setEnabled(false);
            captureButton.setText("CAPTURANDO…");
            renderer.captureRequested=true;
        });
        TextView legend=text("TOCAR = ONDA   •   ARRASTRAR = CÁMARA   •   PELLIZCAR = ZOOM",9,MUTED,false);
        legend.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);
        lp.topMargin=dp(8);
        controls.addView(legend,lp);
        TextView note=text("Efectos digitales · no alteran la realidad física",9,0xff8192b0,false);
        note.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams np=new LinearLayout.LayoutParams(-1,-2);
        np.topMargin=dp(5);
        controls.addView(note,np);
        setContentView(screen);
    }

    private interface Adjust { void changed(int progress); }
    private void createSlider(String label,int initial,Adjust adjust){
        LinearLayout row=new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams rp=new LinearLayout.LayoutParams(-1,dp(36));
        rp.topMargin=dp(3);
        controls.addView(row,rp);
        TextView txt=text(label,10,WHITE,true);
        row.addView(txt,new LinearLayout.LayoutParams(dp(156),-2));
        SeekBar slider=new SeekBar(this);
        slider.setMax(100);
        slider.setProgress(initial);
        slider.setProgressTintList(android.content.res.ColorStateList.valueOf(CYAN));
        slider.setThumbTintList(android.content.res.ColorStateList.valueOf(CYAN));
        row.addView(slider,new LinearLayout.LayoutParams(0,dp(38),1f));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            @Override public void onProgressChanged(SeekBar s,int p,boolean fromUser){adjust.changed(p);}
            @Override public void onStartTrackingTouch(SeekBar s){}
            @Override public void onStopTrackingTouch(SeekBar s){}
        });
        adjust.changed(initial);
    }

    private void installGestureControls(){
        canvas.setOnTouchListener((view,event)->{
            if(closed)return false;
            switch(event.getActionMasked()){
                case MotionEvent.ACTION_DOWN:
                    lastX=event.getX();
                    lastY=event.getY();
                    dragged=0;
                    lastPinch=0;
                    return true;
                case MotionEvent.ACTION_POINTER_DOWN:
                    if(event.getPointerCount()>1)lastPinch=separation(event);
                    return true;
                case MotionEvent.ACTION_MOVE:
                    if(event.getPointerCount()>1){
                        float span=separation(event);
                        if(lastPinch>10f && span>10f)
                            renderer.zoom=Math.max(.48f,Math.min(2.2f,renderer.zoom*span/lastPinch));
                        lastPinch=span;
                    }else{
                        float dx=event.getX()-lastX,dy=event.getY()-lastY;
                        dragged+=Math.abs(dx)+Math.abs(dy);
                        renderer.cameraX=Math.max(-1.4f,Math.min(1.4f,
                                renderer.cameraX+dx/Math.max(1f,canvas.getWidth())*1.6f));
                        renderer.cameraY=Math.max(-1.2f,Math.min(1.2f,
                                renderer.cameraY-dy/Math.max(1f,canvas.getHeight())*1.6f));
                        lastX=event.getX();lastY=event.getY();
                    }
                    return true;
                case MotionEvent.ACTION_POINTER_UP:
                    lastPinch=0;dragged=1000f;return true;
                case MotionEvent.ACTION_UP:
                    if(dragged<dp(12))
                        renderer.pulse(event.getX()/Math.max(1f,canvas.getWidth()),
                                event.getY()/Math.max(1f,canvas.getHeight()));
                    return true;
                default:return true;
            }
        });
    }

    private static float separation(MotionEvent e){
        if(e.getPointerCount()<2)return 0;
        float dx=e.getX(0)-e.getX(1),dy=e.getY(0)-e.getY(1);
        return (float)Math.hypot(dx,dy);
    }

    private void immersive(){
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE|
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|
                View.SYSTEM_UI_FLAG_FULLSCREEN|
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    @Override public void onStatistics(int fps,float scale){
        if(closed)return;
        performance.setText(String.format(Locale.US,
                "GPU %d FPS • RESOLUCIÓN %.0f%% • GL ES 3.0",fps,100*scale));
    }

    @Override public void onScreenshot(Bitmap bitmap) {
        if(closed){bitmap.recycle();return;}
        Thread saver=new Thread(()->{
            Uri result=null;
            try{
                String name="SPACE_GLITCH_V2_"+new SimpleDateFormat(
                        "yyyyMMdd_HHmmss",Locale.US).format(new Date())+".png";
                ContentValues values=new ContentValues();
                values.put(MediaStore.MediaColumns.DISPLAY_NAME,name);
                values.put(MediaStore.MediaColumns.MIME_TYPE,"image/png");
                values.put(MediaStore.MediaColumns.RELATIVE_PATH,
                        Environment.DIRECTORY_PICTURES+"/SpaceGlitch");
                values.put(MediaStore.MediaColumns.IS_PENDING,1);
                result=getContentResolver().insert(
                        MediaStore.Images.Media.EXTERNAL_CONTENT_URI,values);
                if(result==null)throw new IllegalStateException("No se creó el archivo PNG");
                try(OutputStream stream=getContentResolver().openOutputStream(result)){
                    if(stream==null || !bitmap.compress(Bitmap.CompressFormat.PNG,100,stream))
                        throw new IllegalStateException("No se pudo guardar PNG");
                }
                ContentValues done=new ContentValues();
                done.put(MediaStore.MediaColumns.IS_PENDING,0);
                getContentResolver().update(result,done,null,null);
                runOnUiThread(()->{
                    if(!closed)Toast.makeText(this,"PNG en Imágenes/SpaceGlitch",Toast.LENGTH_LONG).show();
                });
            } catch(Exception ex) {
                if(result!=null)getContentResolver().delete(result,null,null);
                final String error=ex.getMessage();
                runOnUiThread(()->{
                    if(!closed)Toast.makeText(this,"Error PNG: "+error,Toast.LENGTH_LONG).show();
                });
            } finally {
                bitmap.recycle();
                runOnUiThread(()->{
                    if(!closed){captureButton.setEnabled(true);captureButton.setText("GUARDAR PNG");}
                });
            }
        },"Space-Glitch-MediaStore");
        saver.setDaemon(true);
        saver.start();
    }

    @Override public void onError(String message){
        if(!closed){
            performance.setText("GPU ERROR: "+message);
            Toast.makeText(this,message,Toast.LENGTH_LONG).show();
            captureButton.setEnabled(true);
            captureButton.setText("GUARDAR PNG");
        }
    }

    @Override protected void onPause(){
        if(canvas!=null)canvas.onPause();
        super.onPause();
    }
    @Override protected void onResume(){
        super.onResume();
        immersive();
        if(canvas!=null)canvas.onResume();
    }
    @Override protected void onDestroy(){
        closed=true;
        super.onDestroy();
    }
}
