package com.termux;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

public class MainActivity extends Activity implements SurfaceHolder.Callback2 {
    static {
        System.loadLibrary("termux");
    }

    static native void init(Surface surface, AssetManager assetManager);

    static native void destroy();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().takeSurface(this);
//        TextView textView = new TextView(this);
//        setContentView(textView);
//        textView.setTextColor(Color.WHITE);
//        textView.setText("H␋el␉lo\rW␌o\nrl␊d");
//        SurfaceView surfaceView = new SurfaceView(this);
//        setContentView(surfaceView);
//        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
//            @Override
//            public void surfaceCreated(SurfaceHolder holder) {
//                init(holder.getSurface(), getAssets(),null );
//            }
//
//            @Override
//            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
//
//            }
//
//            @Override
//            public void surfaceDestroyed(SurfaceHolder holder) {
//
//            }
//        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        destroy();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

        init(holder.getSurface(), getAssets());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    @Override
    public void surfaceRedrawNeeded(SurfaceHolder holder) {

    }
}