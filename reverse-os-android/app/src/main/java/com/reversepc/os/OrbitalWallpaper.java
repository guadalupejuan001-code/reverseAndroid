package com.reversepc.os;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.MotionEvent;

/**
 * Native REVERSE orbital wallpaper uses the existing REVERSE-PC Space Glitch V2
 * OpenGL ES 3 render pipeline. Every pixel follows the orbital GLSL rule,
 * presented continuously with measured FPS and bounded adaptive resolution.
 *
 * This layer renders visual motion only. Its measurements are NOT quantum
 * computation measurements or a claim of quantum FPS acceleration.
 */
public final class OrbitalWallpaper extends GLSurfaceView {
    public interface Events {
        void onFps(int fps,float scale);
        void onFailure(String message);
    }

    private final ReverseOrbitalRenderer orbital;
    public OrbitalWallpaper(Context context,Events events) {
        super(context);
        setEGLContextClientVersion(3);
        setEGLConfigChooser(8,8,8,8,16,0);
        setPreserveEGLContextOnPause(true);
        orbital=new ReverseOrbitalRenderer(context,new ReverseOrbitalRenderer.Listener(){
            @Override public void onStatistics(int fps,float scale){
                Log.i("ReverseGPU","REVERSE_GPU_FPS fps="+fps+" scale="+scale);
                events.onFps(fps,scale);
            }
            @Override public void onScreenshot(android.graphics.Bitmap bitmap){
                // Rendering does not consume CPU pixel readback unless explicitly
                // requested by another feature.
            }
            @Override public void onError(String detail){
                Log.e("ReverseGPU","REVERSE_GPU_ERROR "+detail);
                events.onFailure(detail);
            }
        });
        orbital.mode=0;
        orbital.profile=0;
        orbital.speed=.68f;
        orbital.glitch=0f;
        orbital.warp=.34f;
        setRenderer(orbital);
        setRenderMode(RENDERMODE_CONTINUOUSLY);
    }

    public void speed(float value){
        orbital.speed=Math.max(0f,Math.min(2.0f,value));
    }

    public void motionEnabled(boolean enabled){
        orbital.speed=enabled?.68f:0f;
        setRenderMode(enabled?RENDERMODE_CONTINUOUSLY:RENDERMODE_WHEN_DIRTY);
        requestRender();
    }

    public void quality(int mode){
        orbital.profile=Math.max(0,Math.min(2,mode));
    }

    public void pulse(float nx,float ny){
        orbital.pulse(nx,ny);
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if(event.getActionMasked()==MotionEvent.ACTION_DOWN){
            orbital.pulse(event.getX()/Math.max(1,getWidth()),
                          event.getY()/Math.max(1,getHeight()));
            return true;
        }
        return true;
    }
}
