package com.reversepc.os;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/** Optional Android Home entry. The user's system chooser grants this role. */
public final class HomeActivity extends Activity {
    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        startActivity(new Intent(this,getPackageName().endsWith(".workbench")?WorkbenchActivity.class:DesktopActivity.class)
            .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP|Intent.FLAG_ACTIVITY_SINGLE_TOP));
        finish();
    }
}
