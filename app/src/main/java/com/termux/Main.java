package com.termux;

import android.app.Activity;
import android.os.Bundle;

public final class Main extends Activity {
    static {
        System.loadLibrary("termux");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        long t = System.currentTimeMillis();
        print();
        System.out.println("JNI : " + (System.currentTimeMillis() - t));
        t = System.currentTimeMillis();
        for (long x = 0; x <= 1e10; ++x) ;
        System.out.println("Java : " + (System.currentTimeMillis() - t));
    }

    native void print();
}
