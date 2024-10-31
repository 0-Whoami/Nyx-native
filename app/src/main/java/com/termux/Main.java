package com.termux;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public final class Main extends Activity {
    static {
        System.loadLibrary("termux");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        SurfaceView view = new SurfaceView(this);
        setContentView(view);
        view.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                init(holder.getSurface());
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {

            }
        });
    }
    public native void init(Surface view);
    public native void load_asset_manager(AssetManager assetManager);
}
