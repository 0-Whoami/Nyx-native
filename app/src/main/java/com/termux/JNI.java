package com.termux;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;

import java.nio.Buffer;
import java.nio.ByteBuffer;

public final class JNI {
    static {
        System.loadLibrary("termux");
    }

    static void get_text_atlas(int w, final int h) {
        final char[] table = {
                '█',    // Solid block '0'
//                ' ',    // Blank '_'
                '◆',    // Diamond '`'
                '▒',    // Checker board 'a'
                '␉',    // Horizontal tab
                '␌',    // Form feed
                '\r',   // Carriage return
                '␊',    // Linefeed
                '°',    // Degree
                '±',    // Plus-minus
                '\n',   // Newline
                '␋',    // Vertical tab
                '┘',    // Lower right corner
                '┐',    // Upper right corner
                '┌',    // Upper left corner
                '└',    // Lower left corner
                '┼',    // Crossing lines
                '⎺',    // Horizontal line - scan 1
                '⎻',    // Horizontal line - scan 3
                '─',    // Horizontal line - scan 5
                '⎼',    // Horizontal line - scan 7
                '⎽',    // Horizontal line - scan 9
                '├',    // T facing rightwards
                '┤',    // T facing leftwards
                '┴',    // T facing upwards
                '┬',    // T facing downwards
                '│',    // Vertical line
                '≤',    // Less than or equal to
                '≥',    // Greater than or equal to
                'π',    // Pi
                '≠',    // Not equal to
                '£',    // UK pound
                '·',    // Centered dot
        };
        final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Bitmap bitmap = Bitmap.createBitmap(w * 95, h, Bitmap.Config.ALPHA_8);
        final Buffer src = ByteBuffer.allocate(bitmap.getByteCount());
        final Canvas canvas = new Canvas(bitmap);
        int x = 0;
        paint.setTypeface(Typeface.MONOSPACE);
        paint.setTextSize(h);
        final float baseline = -paint.getFontMetrics().ascent;
        final float scaleY = h / (paint.getFontMetrics().descent + baseline);
        for (int codepoint = 0; codepoint < 127; codepoint++) {
            final String string = Character.toString(codepoint < table.length ? table[codepoint] : codepoint);
            final float text_width = paint.measureText(string);
            canvas.save();
            canvas.scale(w / text_width, scaleY);
            canvas.drawText(string, (x++) * text_width, baseline, paint);
            canvas.restore();
        }
        bitmap.copyPixelsToBuffer(src);
        src.rewind();
    }
}
