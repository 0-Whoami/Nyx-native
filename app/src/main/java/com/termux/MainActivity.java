package com.termux;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MainActivity extends Activity {
    static {
        System.loadLibrary("termux");
    }

    static native void init(Surface surface, AssetManager assetManager, Bitmap atlus);

    static native void destroy();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        SurfaceView surfaceView = new SurfaceView(this);
        setContentView(surfaceView);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Bitmap bitmap = Bitmap.createBitmap(10, 12, Bitmap.Config.ALPHA_8);
                init(holder.getSurface(), getAssets(),null );
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {

            }
        });

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        destroy();
    }
}