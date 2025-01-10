//
// Created by Rohit Paul on 28-12-2024.
//

#include <string.h>
#include "terminal_buffer.h"


#define get(bit, pos) (bit&(pos))
#define DECSET_BIT emulator->cursor.decsetBit
#define SCREEN_BUFF ((Terminal_buffer*)emulator->screen_buff)
#define COLUMNS emulator->columns
#define CURSOR emulator->cursor
#define CURSOR_COL CURSOR.x
#define CURSOR_ROW CURSOR.y
#define CURSOR_GLYPH CURSOR.glyph
#define LEFT_MARGIN emulator->leftMargin
#define RIGHT_MARGIN emulator->rightMargin
#define TOP_MARGIN emulator->topMargin
#define BOTTOM_MARGIN emulator->bottomMargin

#define get_glyph_internal(x, y) (SCREEN_BUFF->data + ((y * COLUMNS) + x))
#define get_glyph_external(x, y) get_glyph_internal(x,(y))
#define block_clear(sx, sy, w, h) CURSOR_GLYPH.code=' ';BUFF_OP.block_set(emulator, sx, sy, w, h)

static void set_glyph(terminal *restrict const emulator) {
    *get_glyph_external(CURSOR_COL, CURSOR_ROW) = CURSOR_GLYPH;
}

static void block_copy(terminal *restrict const emulator, const i8 sx, const i8 sy, const i8 w, i8 h, const i8 dx, const i8 dy) {
    if (w > 0) {
        i8 x = (i8) (sy < dy);
        const i8 copyingUp = (i8) (COLUMNS * (1 - 2 * x));
        Glyph *restrict src = get_glyph_external(sx, sy + (x * h)), *restrict dst = get_glyph_external(dx, dy + (x * h));
        while (--h > 0) {
            for (x = 0; x < w; x++) dst[x] = src[x];
            src += copyingUp;
            dst += copyingUp;
        }
    }
}

static void block_set(terminal *restrict const emulator, const i8 sx, const i8 sy, const i8 w, const i8 h) {
    Glyph *restrict ptr = get_glyph_external(sx, sy);
    for (i8 y = 0; y < h; y++) {
        for (i8 x = 0; x < w; x++) ptr[x] = CURSOR_GLYPH;
        ptr += COLUMNS;
    }
}

static void
block_set_attribute(terminal *restrict const emulator, const i8 sx, const i8 sy, const i8 w, const i8 h, bool not_selective,
                    bool dont_keep_attribute) {
    Glyph *restrict ptr = get_glyph_external(sx, sy);
    for (i8 y = 0; y < h; y++) {
        for (i8 x = 0; x < w; x++) {
            if (not_selective || (get(ptr[x].style.effect, PROTECTED) == 0)) {
                ptr[x].code = CURSOR_GLYPH.code;
                if (dont_keep_attribute) ptr[x].style = CURSOR_GLYPH.style;
            }
        }
        ptr += COLUMNS;
    }
}

static void
set_or_clear_effect(terminal *restrict const emulator, const ui8 bits, const bool set, const bool reverse, const i8 top, const i8 bottom,
                    const i8 left, const i8 right, const i8 right_margin,
                    const i8 left_margin) {
    Glyph *restrict line = get_glyph_external(0, top);
    bool rectangle = get(DECSET_BIT, RECTANGULAR_CHANGE_ATTRIBUTE);
    i8 x = left, end_of_line;
    for (i8 y = top; y < bottom; y++) {
        end_of_line = (i8) ((rectangle || y + 1 == bottom) ? right : right_margin);
        for (; x < end_of_line; x++)
            if (reverse)
                line[x].style.effect = (line[x].style.effect & ~bits) || (bits & ~line[x].style.effect);
            else if (set)
                line[x].style.effect |= bits;
            else line[x].style.effect &= ~bits;
        x = (i8) (rectangle ? left : left_margin);
        line += COLUMNS;
    }
}

static void scroll_down(terminal *restrict const emulator, const i8 n) {
    block_copy(emulator, LEFT_MARGIN, TOP_MARGIN + n, RIGHT_MARGIN - LEFT_MARGIN, BOTTOM_MARGIN - TOP_MARGIN - n, LEFT_MARGIN, TOP_MARGIN);
    block_clear(LEFT_MARGIN, BOTTOM_MARGIN - n, RIGHT_MARGIN - LEFT_MARGIN, n);
}

static void clear_transcript(terminal *restrict const emulator) {
    memset(emulator->main, (int) NORMAL.ptr, (size_t) (COLUMNS * emulator->rows));
}

static void set_line_wrap(terminal *const restrict emulator, i8 row, bool set) {
    if (set) SCREEN_BUFF->auto_wrapped_lines[row / 8] |= (1 << (row & 7));
    else
        SCREEN_BUFF->auto_wrapped_lines[row / 8] &= ~(1 << (row & 7));
}

static bool get_line_wrap(terminal *const restrict emulator, i8 row) {
    return SCREEN_BUFF->auto_wrapped_lines[row / 8] & (1 << (row & 7));
}

buffer_operation BUFF_OP = {
        .allocate_buffers=0,
        .block_set_attr=block_set_attribute,
        .block_set=block_set,
        .block_copy=block_copy,
        .scroll_down=scroll_down,
        .clear_transcript=clear_transcript,
        .set_glyph=set_glyph,
        .set_or_clear_effect=set_or_clear_effect,
        .set_line_wrap=set_line_wrap,
        .get_line_wrap=get_line_wrap,
        .free_buffers=0
};
