package com.reversepc.os;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** Several nonmodal windows inside one Android activity and one XI authority. */
public final class DesktopWindows {
    private final Context context;
    private final FrameLayout area;
    private final LinearLayout tasks;
    private final SharedPreferences prefs;
    private final Runnable changed;
    private final ArrayList<Handle> order=new ArrayList<>();
    private final HashSet<String> desktopHidden=new HashSet<>();
    private Handle focused;
    private int fullWidth,fullHeight;
    private String desktopFocus="";
    public DesktopWindows(Context c,FrameLayout area,LinearLayout tasks,SharedPreferences prefs,Runnable changed){
        context=c;this.area=area;this.tasks=tasks;this.prefs=prefs;this.changed=changed;
        area.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->{
            if(r-l<=0||b-t<=0||(r-l==or-ol&&b-t==ob-ot))return;
            fullWidth=Math.max(fullWidth,r-l);fullHeight=Math.max(fullHeight,b-t);
            for(Handle h:order){if(h.mode!=0)h.rect.snap(h.mode==2?0:h.mode,area.getWidth(),area.getHeight());else h.rect=h.normal.copy();h.place();}
        });
    }
    private int dp(float n){return Math.round(n*context.getResources().getDisplayMetrics().density);}
    private GradientDrawable shape(int color,int edge){GradientDrawable g=new GradientDrawable();g.setColor(color);g.setCornerRadius(dp(9));g.setStroke(dp(1),edge);return g;}
    private TextView text(String value,int size){TextView t=new TextView(context);t.setText(value);t.setTextSize(size);t.setTextColor(0xffe9edf0);t.setGravity(Gravity.CENTER_VERTICAL);return t;}
    public List<Handle> all(){return new ArrayList<>(order);}
    public boolean focusExisting(String key){for(Handle h:order)if(h.key.equals(key)){h.show();return true;}return false;}
    public boolean coversDesktop(){
        boolean left=false,right=false;
        for(Handle h:order)if(!h.minimized){if(h.mode==2)return true;if(h.mode==-1)left=true;if(h.mode==1)right=true;}
        return left&&right;
    }
    public Handle focused(){return focused;}
    public List<String> savedKeys(){
        ArrayList<String> keys=new ArrayList<>();
        try{JSONArray a=new JSONArray(prefs.getString("session","[]"));for(int i=0;i<a.length();i++)keys.add(a.getString(i));}catch(JSONException ignored){}
        return keys;
    }
    public void save(){
        JSONArray session=new JSONArray();SharedPreferences.Editor edit=prefs.edit();
        for(Handle h:order){
            if(h.key.equals("Inicio")||h.key.equals("Buscar")||h.key.equals("Panel del sistema"))continue;
            session.put(h.key);
            try{JSONObject j=new JSONObject();WindowGeometry p=h.normal;
                j.put("x",p.x/(double)Math.max(1,fullWidth));j.put("y",p.y/(double)Math.max(1,fullHeight));
                j.put("w",p.width/(double)Math.max(1,fullWidth));j.put("h",p.height/(double)Math.max(1,fullHeight));
                j.put("mode",h.mode);j.put("minimized",h.minimized);edit.putString("window:"+h.key,j.toString());
            }catch(JSONException ignored){}
        }
        edit.putString("session",session.toString()).putString("focused",focused==null?"":focused.key).apply();
    }
    private void update(){
        tasks.removeAllViews();
        for(Handle h:order){
            TextView task=text(h.title,11);task.setSingleLine(true);task.setEllipsize(android.text.TextUtils.TruncateAt.END);
            task.setPadding(dp(11),0,dp(11),0);task.setGravity(Gravity.CENTER);
            task.setContentDescription("TAREA "+h.title);
            task.setBackground(shape(h==focused?0xff3e5365:0xff263441,h.minimized?0xff394652:0xff7c9eb6));
            task.setOnClickListener(v->{if(h==focused&&!h.minimized)h.hide();else h.show();});
            task.setOnLongClickListener(v->{h.menu(task);return true;});
            LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(dp(98),dp(34));p.setMargins(dp(3),0,dp(3),0);tasks.addView(task,p);
        }
        changed.run();save();
    }
    public Handle open(String key,String title,String subtitle){
        Handle h=new Handle(key,title,subtitle);order.add(h);area.addView(h.shell);h.place();
        if(h.minimized)h.shell.setVisibility(View.GONE);else focus(h);
        update();return h;
    }
    private void focus(Handle h){
        focused=h;h.minimized=false;h.shell.setVisibility(View.VISIBLE);h.shell.bringToFront();
        for(Handle p:order)p.shell.setBackground(shape(0xff202c37,p==h?0xff839caf:0xff455461));
        update();
    }
    public void cycle(){
        if(order.isEmpty())return;int i=focused==null?-1:order.indexOf(focused);order.get((i+1)%order.size()).show();
    }
    public void showDesktop(){
        if(!desktopHidden.isEmpty()){
            Handle last=null;
            for(Handle h:order)if(desktopHidden.contains(h.key)){h.minimized=false;h.shell.setVisibility(View.VISIBLE);}
            for(Handle h:order)if(desktopHidden.contains(h.key)){last=h;if(h.key.equals(desktopFocus))break;}
            desktopHidden.clear();if(last!=null)focus(last);
        }else{
            desktopFocus=focused==null?"":focused.key;
            for(Handle h:order)if(!h.minimized){desktopHidden.add(h.key);h.minimized=true;h.shell.setVisibility(View.GONE);}
            focused=null;
        }
        update();
    }
    public void release(){save();for(Handle h:new ArrayList<>(order))h.dispose();order.clear();}

    public final class Handle {
        public final String key,title;
        public final LinearLayout content;
        private final FrameLayout shell;
        private WindowGeometry rect,normal;
        private boolean minimized,closed;
        private int mode;
        private Consumer<Handle> dismissListener;
        public Runnable saveAction;
        Handle(String key,String title,String subtitle){
            this.key=key;this.title=title;
            int aw=Math.max(dp(400),area.getWidth()),ah=Math.max(dp(220),area.getHeight());
            int w=Math.min(aw,Math.max(dp(410),(int)(aw*.74))),height=Math.min(ah,Math.max(dp(225),(int)(ah*.94)));
            rect=new WindowGeometry(Math.min(dp(150)+order.size()*dp(16),aw-w),Math.min(order.size()*dp(12),ah-height),w,height);
            try{JSONObject j=new JSONObject(prefs.getString("window:"+key,"{}"));
                rect=new WindowGeometry((int)(j.optDouble("x",rect.x/(double)aw)*aw),(int)(j.optDouble("y",rect.y/(double)ah)*ah),
                    (int)(j.optDouble("w",w/(double)aw)*aw),(int)(j.optDouble("h",height/(double)ah)*ah));
                mode=j.optInt("mode",0);if(mode < -1||mode>2)mode=0;minimized=j.optBoolean("minimized",false);
            }catch(JSONException ignored){}
            rect.fit(aw,ah,dp(280),dp(180));normal=rect.copy();if(mode!=0)rect.snap(mode==2?0:mode,aw,ah);
            shell=new FrameLayout(context){@Override public boolean onInterceptTouchEvent(android.view.MotionEvent e){
                if(e.getActionMasked()==android.view.MotionEvent.ACTION_DOWN&&focused!=Handle.this)focus(Handle.this);
                return super.onInterceptTouchEvent(e);
            }};
            shell.setElevation(dp(9));shell.setContentDescription("VENTANA "+title);shell.setBackground(shape(0xff202c37,0xff839caf));
            LinearLayout body=new LinearLayout(context);body.setOrientation(LinearLayout.VERTICAL);body.setPadding(dp(1),dp(1),dp(1),dp(1));
            shell.addView(body,new FrameLayout.LayoutParams(-1,-1));
            LinearLayout bar=new LinearLayout(context);bar.setGravity(Gravity.CENTER_VERTICAL);bar.setPadding(dp(10),0,dp(2),0);bar.setBackgroundColor(0xff2c3b48);
            TextView caption=text(title,13);caption.setTypeface(Typeface.DEFAULT,Typeface.BOLD);caption.setSingleLine(true);caption.setEllipsize(android.text.TextUtils.TruncateAt.END);
            caption.setContentDescription("MOVER "+title);bar.addView(caption,new LinearLayout.LayoutParams(0,-1,1));
            control(bar,"⋮","MENÚ "+title,()->menu(bar));control(bar,"—","MINIMIZAR "+title,this::hide);
            control(bar,"□","MAXIMIZAR "+title,()->snap(mode==2?3:0));control(bar,"×","CERRAR "+title,this::dismiss);
            body.addView(bar,new LinearLayout.LayoutParams(-1,dp(36)));
            content=new LinearLayout(context);content.setOrientation(LinearLayout.VERTICAL);content.setPadding(dp(10),dp(8),dp(10),dp(9));
            body.addView(content,new LinearLayout.LayoutParams(-1,0,1));
            TextView grip=text("◢",16);grip.setGravity(Gravity.BOTTOM|Gravity.RIGHT);grip.setContentDescription("REDIMENSIONAR "+title);
            shell.addView(grip,new FrameLayout.LayoutParams(dp(25),dp(24),Gravity.BOTTOM|Gravity.RIGHT));
            final float[] start=new float[2];final WindowGeometry[] origin=new WindowGeometry[1];final long[] tap={0};
            caption.setOnTouchListener((v,e)->{
                if(e.getActionMasked()==MotionEvent.ACTION_DOWN){focus(this);start[0]=e.getRawX();start[1]=e.getRawY();origin[0]=rect.copy();return true;}
                if(e.getActionMasked()==MotionEvent.ACTION_MOVE){
                    if(mode!=0)return true;rect.x=origin[0].x+Math.round(e.getRawX()-start[0]);rect.y=origin[0].y+Math.round(e.getRawY()-start[1]);place();return true;
                }
                if(e.getActionMasked()==MotionEvent.ACTION_UP){
                    boolean still=Math.abs(e.getRawX()-start[0])+Math.abs(e.getRawY()-start[1])<dp(8);
                    if(still&&android.os.SystemClock.uptimeMillis()-tap[0]<300)snap(mode==2?3:0);
                    else if(!still&&rect.x<=dp(6))snap(-1);
                    else if(!still&&rect.x+rect.width>=area.getWidth()-dp(6))snap(1);
                    tap[0]=still?android.os.SystemClock.uptimeMillis():0;normal=mode==0?rect.copy():normal;save();return true;
                }return true;
            });
            grip.setOnTouchListener((v,e)->{
                if(e.getActionMasked()==MotionEvent.ACTION_DOWN){focus(this);if(mode!=0)snap(3);start[0]=e.getRawX();start[1]=e.getRawY();origin[0]=rect.copy();return true;}
                if(e.getActionMasked()==MotionEvent.ACTION_MOVE){rect.width=origin[0].width+Math.round(e.getRawX()-start[0]);rect.height=origin[0].height+Math.round(e.getRawY()-start[1]);place();return true;}
                if(e.getActionMasked()==MotionEvent.ACTION_UP){normal=rect.copy();save();}return true;
            });
        }
        private void control(LinearLayout bar,String value,String description,Runnable action){TextView b=text(value,19);b.setGravity(Gravity.CENTER);b.setContentDescription(description);b.setOnClickListener(v->action.run());bar.addView(b,new LinearLayout.LayoutParams(dp(33),-1));}
        private void place(){
            rect.fit(Math.max(1,area.getWidth()),Math.max(1,area.getHeight()),dp(mode==0?280:120),dp(150));
            FrameLayout.LayoutParams p=new FrameLayout.LayoutParams(rect.width,rect.height);p.leftMargin=rect.x;p.topMargin=rect.y;shell.setLayoutParams(p);
        }
        public void snap(int side){if(mode==0)normal=rect.copy();if(side==3){rect=normal.copy();mode=0;}else{mode=side==0?2:side;rect.snap(side,area.getWidth(),area.getHeight());}place();focus(this);}
        public boolean isShowing(){return !closed;}
        public boolean isMinimized(){return minimized;}
        public void show(){if(!closed)focus(this);}
        public void hide(){minimized=true;shell.setVisibility(View.GONE);if(focused==this)focused=null;update();}
        public void setOnDismissListener(Consumer<Handle> listener){dismissListener=listener;}
        public void dismiss(){if(closed)return;dispose();order.remove(this);area.removeView(shell);if(focused==this)focused=null;update();}
        private void dispose(){if(closed)return;closed=true;if(dismissListener!=null)dismissListener.accept(this);}
        private void menu(View anchor){PopupMenu m=new PopupMenu(context,anchor);String[] labels={"Restaurar","Ajustar izquierda","Ajustar derecha","Maximizar","Minimizar","Cerrar"};for(String label:labels)m.getMenu().add(label);
            m.setOnMenuItemClickListener(item->{int i=Arrays.asList(labels).indexOf(item.getTitle().toString());if(i==0)snap(3);if(i==1)snap(-1);if(i==2)snap(1);if(i==3)snap(0);if(i==4)hide();if(i==5)dismiss();return true;});m.show();}
    }
}
