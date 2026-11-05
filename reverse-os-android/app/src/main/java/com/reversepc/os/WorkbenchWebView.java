package com.reversepc.os;

import android.content.Context;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.webkit.WebView;

/** Keep Android's IME inside the PC desktop instead of its fullscreen editor. */
public final class WorkbenchWebView extends WebView {
    public WorkbenchWebView(Context context){super(context);}

    @Override public InputConnection onCreateInputConnection(EditorInfo info){
        InputConnection connection=super.onCreateInputConnection(info);
        if(connection!=null)info.imeOptions|=EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return connection;
    }
}
