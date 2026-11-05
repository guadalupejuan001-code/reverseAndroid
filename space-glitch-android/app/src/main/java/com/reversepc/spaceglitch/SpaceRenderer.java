package com.reversepc.spaceglitch;

import android.content.Context;
import android.graphics.Bitmap;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Looper;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.charset.StandardCharsets;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Real OpenGL ES 3 renderer with an adaptive-resolution offscreen texture.
 * All four effects are computed procedurally on the GPU, never in a WebView.
 * Image capture is deliberately explicit: the CPU sees pixels only for PNG.
 */
public final class SpaceRenderer implements GLSurfaceView.Renderer {
    public interface Listener {
        void onStatistics(int measuredFps, float internalScale);
        void onScreenshot(Bitmap bitmap);
        void onError(String message);
    }

    private final Context context;
    private final Listener listener;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final FloatBuffer quad = ByteBuffer.allocateDirect(8 * 4)
            .order(ByteOrder.nativeOrder()).asFloatBuffer();

    volatile int mode = 0;
    volatile float glitch = .52f;
    volatile float warp = .98f;
    volatile float speed = 1.15f;
    volatile float cameraX = 0f;
    volatile float cameraY = 0f;
    volatile float zoom = 1f;
    volatile float touchX = .5f;
    volatile float touchY = .5f;
    volatile float pulse = 0f;
    volatile int profile = 0; // 0=AUTO, 1=ECO, 2=ULTRA
    volatile boolean captureRequested = false;

    private int sceneProgram, blitProgram, framebuffer, colorTexture;
    private int width, height, renderWidth, renderHeight;
    private long startNs, lastNs, statsNs;
    private int frameCount;
    private float internalScale = .73f, autoScale = .73f;
    private int slowIntervals, fastIntervals;

    public SpaceRenderer(Context context, Listener listener) {
        this.context = context.getApplicationContext();
        this.listener = listener;
        quad.put(new float[]{-1f,-1f,1f,-1f,-1f,1f,1f,1f}).position(0);
    }

    private String asset(String name) {
        try (InputStream input = context.getAssets().open(name);
             ByteArrayOutputStream bytes = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int count;
            while ((count = input.read(buffer)) != -1) bytes.write(buffer, 0, count);
            return bytes.toString(StandardCharsets.UTF_8.name());
        } catch (IOException e) {
            throw new IllegalStateException("GL shader asset missing: "+name, e);
        }
    }

    private static int shader(int type, String code) {
        int shader = GLES30.glCreateShader(type);
        if (shader == 0) throw new IllegalStateException("glCreateShader failed");
        GLES30.glShaderSource(shader, code);
        GLES30.glCompileShader(shader);
        int[] status = new int[1];
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, status, 0);
        if (status[0] == 0) {
            String log = GLES30.glGetShaderInfoLog(shader);
            GLES30.glDeleteShader(shader);
            throw new IllegalStateException("GL shader compile failure: " + log);
        }
        return shader;
    }

    private static int program(String vertex, String fragment) {
        int vert = shader(GLES30.GL_VERTEX_SHADER, vertex);
        int frag = shader(GLES30.GL_FRAGMENT_SHADER, fragment);
        int program = GLES30.glCreateProgram();
        GLES30.glAttachShader(program, vert);
        GLES30.glAttachShader(program, frag);
        GLES30.glLinkProgram(program);
        int[] status = new int[1];
        GLES30.glGetProgramiv(program, GLES30.GL_LINK_STATUS, status, 0);
        GLES30.glDeleteShader(vert);
        GLES30.glDeleteShader(frag);
        if (status[0] == 0) {
            String log = GLES30.glGetProgramInfoLog(program);
            GLES30.glDeleteProgram(program);
            throw new IllegalStateException("GL program link failure: " + log);
        }
        return program;
    }

    @Override public void onSurfaceCreated(GL10 ignored, EGLConfig config) {
        try {
            sceneProgram = program(asset("space.vert"), asset("space.frag"));
            blitProgram = program(asset("space.vert"), asset("blit.frag"));
            framebuffer = 0;
            colorTexture = 0;
            renderWidth = renderHeight = 0;
            GLES30.glDisable(GLES30.GL_DEPTH_TEST);
            GLES30.glDisable(GLES30.GL_BLEND);
            GLES30.glClearColor(.01f, .01f, .05f, 1f);
            startNs = lastNs = statsNs = System.nanoTime();
            frameCount = 0;
        } catch (RuntimeException e) {
            report(e.getMessage());
            throw e;
        }
    }

    @Override public void onSurfaceChanged(GL10 ignored, int w, int h) {
        width = Math.max(1, w);
        height = Math.max(1, h);
        renderWidth = renderHeight = 0;  // Force FBO allocation on next draw.
    }

    private static void uniform1(int program, String key, float value) {
        int index = GLES30.glGetUniformLocation(program, key);
        if (index >= 0) GLES30.glUniform1f(index, value);
    }

    private static void uniform2(int program, String key, float x, float y) {
        int index = GLES30.glGetUniformLocation(program, key);
        if (index >= 0) GLES30.glUniform2f(index, x, y);
    }

    private void drawQuad() {
        quad.position(0);
        GLES30.glEnableVertexAttribArray(0);
        GLES30.glVertexAttribPointer(0, 2, GLES30.GL_FLOAT, false, 0, quad);
        GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0, 4);
        GLES30.glDisableVertexAttribArray(0);
    }

    private void resizeFramebuffer(int newWidth, int newHeight) {
        if (newWidth == renderWidth && newHeight == renderHeight && framebuffer != 0)
            return;
        if (colorTexture != 0) GLES30.glDeleteTextures(1, new int[]{colorTexture}, 0);
        if (framebuffer != 0) GLES30.glDeleteFramebuffers(1, new int[]{framebuffer}, 0);
        renderWidth = newWidth;
        renderHeight = newHeight;
        int[] ids = new int[1];
        GLES30.glGenTextures(1, ids, 0);
        colorTexture = ids[0];
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, colorTexture);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE);
        GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_RGBA8,
                renderWidth, renderHeight, 0, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, null);
        GLES30.glGenFramebuffers(1, ids, 0);
        framebuffer = ids[0];
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, framebuffer);
        GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER, GLES30.GL_COLOR_ATTACHMENT0,
                GLES30.GL_TEXTURE_2D, colorTexture, 0);
        int status = GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER);
        if (status != GLES30.GL_FRAMEBUFFER_COMPLETE)
            throw new IllegalStateException("GL framebuffer error " + Integer.toHexString(status));
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);
    }

    private float desiredScale() {
        float scale = profile == 1 ? .52f : profile == 2 ? 1f : autoScale;
        // Keep GPU allocations bounded on budget phones, even in ULTRA mode.
        float maxPixels = profile == 2 ? 2300000f : 1550000f;
        float cap = (float)Math.sqrt(maxPixels / Math.max(1f, (float)width * height));
        return Math.max(.36f, Math.min(scale, cap));
    }

    @Override public void onDrawFrame(GL10 ignored) {
        if (sceneProgram == 0 || width == 0 || height == 0) return;
        long now = System.nanoTime();
        float seconds = (now - startNs) / 1_000_000_000f;
        float delta = Math.min(.1f, (now - lastNs) / 1_000_000_000f);
        lastNs = now;
        internalScale = desiredScale();
        int rw = Math.max(2, Math.round(width * internalScale));
        int rh = Math.max(2, Math.round(height * internalScale));

        try {
            resizeFramebuffer(rw, rh);
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, framebuffer);
            GLES30.glViewport(0, 0, renderWidth, renderHeight);
            GLES30.glUseProgram(sceneProgram);
            uniform2(sceneProgram,"uResolution",renderWidth,renderHeight);
            uniform1(sceneProgram,"uTime",seconds);
            uniform1(sceneProgram,"uMode",mode);
            uniform1(sceneProgram,"uGlitch",glitch);
            uniform1(sceneProgram,"uWarp",warp);
            uniform1(sceneProgram,"uSpeed",speed);
            uniform2(sceneProgram,"uCamera",cameraX,cameraY);
            uniform2(sceneProgram,"uTouch",touchX,touchY);
            uniform1(sceneProgram,"uPulse",pulse);
            uniform1(sceneProgram,"uZoom",zoom);
            drawQuad();

            if (captureRequested) {
                captureRequested = false;
                capturePixels();
            }

            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);
            GLES30.glViewport(0, 0, width, height);
            GLES30.glUseProgram(blitProgram);
            GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, colorTexture);
            int sampler = GLES30.glGetUniformLocation(blitProgram,"uTexture");
            if (sampler >= 0) GLES30.glUniform1i(sampler, 0);
            drawQuad();

            pulse = Math.max(0f, pulse - delta * 1.24f);
            frameCount++;
            if (now - statsNs >= 1_000_000_000L) {
                float interval = (now - statsNs) / 1_000_000_000f;
                final int fps = Math.round(frameCount / interval);
                final float scale = internalScale;
                ui.post(() -> listener.onStatistics(fps,scale));
                if (profile == 0) {
                    if (fps < 29) { slowIntervals++; fastIntervals=0; }
                    else if (fps > 53) { fastIntervals++; slowIntervals=0; }
                    else { slowIntervals=0; fastIntervals=0; }
                    if (slowIntervals >= 2) {
                        autoScale = Math.max(.43f, autoScale - .07f);
                        slowIntervals = 0;
                    } else if (fastIntervals >= 4) {
                        autoScale = Math.min(.82f, autoScale + .035f);
                        fastIntervals = 0;
                    }
                }
                frameCount = 0;
                statsNs = now;
            }
        } catch (RuntimeException error) {
            report(error.getMessage());
            // Do not spin on a broken GL context at full speed.
            throw error;
        }
    }

    public void pulse(float nx, float ny) {
        touchX=Math.max(0f,Math.min(1f,nx));
        touchY=1f-Math.max(0f,Math.min(1f,ny));
        pulse=1f;
    }

    private void capturePixels() {
        // Read only when requested. GPU RGB image is stored bottom-up; the
        // background worker creates top-down ARGB and releases the GL thread.
        int w = renderWidth, h = renderHeight;
        if (w <= 0 || h <= 0 || (long)w*h > 2500000L) {
            report("Captura demasiado grande");
            return;
        }
        ByteBuffer pixels = ByteBuffer.allocateDirect(w*h*4);
        GLES30.glReadPixels(0,0,w,h,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,pixels);
        int status=GLES30.glGetError();
        if(status!=GLES30.GL_NO_ERROR) {
            report("No se pudo leer la imagen GL: " + Integer.toHexString(status));
            return;
        }
        Thread encode = new Thread(() -> {
            try {
                int[] argb = new int[w*h];
                for (int y=0;y<h;y++) {
                    int sourceY=h-1-y;
                    for (int x=0;x<w;x++) {
                        int pos=(sourceY*w+x)*4;
                        int red=pixels.get(pos)&255;
                        int green=pixels.get(pos+1)&255;
                        int blue=pixels.get(pos+2)&255;
                        argb[y*w+x]=0xff000000|(red<<16)|(green<<8)|blue;
                    }
                }
                Bitmap bitmap=Bitmap.createBitmap(argb,w,h,Bitmap.Config.ARGB_8888);
                ui.post(() -> listener.onScreenshot(bitmap));
            } catch (Throwable e) {
                report("Captura: "+e.getMessage());
            }
        },"Space-Glitch-PNG-Encode");
        encode.setDaemon(true);
        encode.start();
    }

    private void report(String message) {
        final String safe=message==null?"Error OpenGL":message;
        ui.post(() -> listener.onError(safe));
    }
}
