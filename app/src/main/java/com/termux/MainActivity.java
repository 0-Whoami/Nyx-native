package com.termux;

import static android.view.View.TEXT_ALIGNMENT_GRAVITY;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.style.UnderlineSpan;
import android.view.Gravity;
import android.widget.TextView;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity implements GLSurfaceView.Renderer {
    static {
        System.loadLibrary("termux");
    }

    static native void init();

    static native void render();

    static native void destroy();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        GLSurfaceView view = new GLSurfaceView(this);
        view.setEGLContextClientVersion(3);
        view.setRenderer(this);
        TextView tv = new TextView(this);
        tv.setGravity(Gravity.CENTER);
        tv.setTextAlignment(TEXT_ALIGNMENT_GRAVITY);
        SpannableString string = new SpannableString("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijlklmnopqrstuvwxyz01234567890");
        string.setSpan(new UnderlineSpan(), 0, string.length(), 0);
        tv.setText(string);
        tv.setTextColor(Color.WHITE);
        tv.setTypeface(Typeface.MONOSPACE);
        setContentView(tv);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        destroy();
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        render();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {

    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        init();
    }
}