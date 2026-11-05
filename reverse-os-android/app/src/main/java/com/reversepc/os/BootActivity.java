package com.reversepc.os;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Quiet REVERSE boot. Never touches XI, OpenGL, files or JNI until the
 * desktop has been drawn. The first platform and app frames are both black.
 */
public final class BootActivity extends Activity {
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Runnable next=()->{
        if(isFinishing()||isDestroyed())return;
        try {
            startActivity(new Intent(this,getPackageName().endsWith(".workbench")?WorkbenchActivity.class:DesktopActivity.class));
            overridePendingTransition(android.R.anim.fade_in,android.R.anim.fade_out);
            finish();
        } catch (Throwable failure) {
            Log.e("ReverseBoot","Could not launch desktop",failure);
            TextView error=new TextView(this);
            error.setText("REVERSE\nNo se pudo abrir el escritorio.\n"+
                          failure.getMessage());
            error.setTextColor(0xffffbbbb);
            error.setBackgroundColor(Color.BLACK);
            error.setGravity(Gravity.CENTER);
            setContentView(error);
        }
    };
    private int dp(float a){
        return Math.round(a*getResources().getDisplayMetrics().density);
    }
    private TextView text(String value,int size,int color,boolean bold){
        TextView view=new TextView(this);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setGravity(Gravity.CENTER);
        view.setTypeface(Typeface.create(bold?"sans-serif-medium":"sans-serif",
                                         Typeface.NORMAL));
        return view;
    }
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        getWindow().setStatusBarColor(Color.BLACK);
        getWindow().setNavigationBarColor(Color.BLACK);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);
        if(getPackageName().endsWith(".workbench"))WorkbenchActivity.immersive(this);
        FrameLayout backdrop=new FrameLayout(this);
        backdrop.setBackgroundColor(Color.BLACK);

        LinearLayout center=new LinearLayout(this);
        center.setOrientation(LinearLayout.VERTICAL);
        center.setGravity(Gravity.CENTER);
        TextView welcome=text("welcome....",16,0xffb7bdc5,false);
        center.addView(welcome);
        TextView brand=text("REVERSE.",35,0xfff0eee9,true);
        brand.setLetterSpacing(.11f);
        LinearLayout.LayoutParams title=new LinearLayout.LayoutParams(-1,-2);
        title.topMargin=dp(7);
        center.addView(brand,title);
        View accent=new View(this);
        accent.setBackgroundColor(0xffa7bcd0);
        LinearLayout.LayoutParams rule=new LinearLayout.LayoutParams(dp(54),dp(2));
        rule.topMargin=dp(22);
        center.addView(accent,rule);
        backdrop.addView(center,new FrameLayout.LayoutParams(-1,-2,Gravity.CENTER));

        TextView foot=text("REVERSE OS",10,0xff7e8993,false);
        FrameLayout.LayoutParams footer=new FrameLayout.LayoutParams(-1,dp(36),Gravity.BOTTOM);
        footer.bottomMargin=dp(12);
        backdrop.addView(foot,footer);
        setContentView(backdrop);

        welcome.setAlpha(0f);brand.setAlpha(0f);accent.setAlpha(0f);
        welcome.animate().alpha(1f).setDuration(500).start();
        brand.animate().alpha(1f).setStartDelay(220).setDuration(650).start();
        accent.animate().alpha(1f).setStartDelay(430).setDuration(650).start();
        Log.i("ReverseBoot","WELCOME_SCREEN_SHOWN welcome.... REVERSE.");
        handler.postDelayed(next,2500L);
    }
    @Override protected void onDestroy(){
        handler.removeCallbacks(next);
        super.onDestroy();
    }
}
