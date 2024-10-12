#include <stdlib.h>
#include "term_row.h"

#define ROWS 24
#define transcript_row 200

typedef struct {
    term_row *lines;
    int_fast16_t first_row;
    int_fast16_t total_rows;
} term_buffer;

#define internalRow(ex_row) (((ex_row) + buffer->first_row)%buffer->total_rows)

void init_term_buffer(term_buffer *buffer, const bool main) {
    int_fast16_t rows = main ? transcript_row : ROWS;
    buffer->lines = (term_row *) malloc(rows * sizeof(term_row));
    buffer->total_rows = rows;
    buffer->first_row = 0;
    for (int i = 0; i < rows; i++)
        clear_row(buffer->lines + i, NORMAL);
}



void blockCopy(term_buffer *buffer, const int sx, const int sy, int w, const int h, const int dx, const int dy) {
    if (w) {
        const char copyingUp = sy > dy;
        for (int y = 0; y < h; y++) {
            const int y2 = copyingUp ? y : h - y - 1;
            glyph *src = buffer->lines[internalRow(dy + y2)].text + dx;
            glyph *dst = buffer->lines[internalRow(sy + y2)].text + sx;
            while (w--)
                *src++ = *dst++;
        }
    }
}

static void block_copy_lines_down(term_buffer *buffer, const int_fast16_t internal, int_fast16_t len) {
    if (len) {
        const term_row over_writable = buffer->lines[(internal + len--) % buffer->total_rows];
        while (0 <= --len)
            buffer->lines[(internal + len + 1) % buffer->total_rows] = buffer->lines[(internal + len) % buffer->total_rows];
        buffer->lines[internal % buffer->total_rows] = over_writable;
    }
}

inline void scroll_down_one_line_buff(term_buffer *buffer, int_fast16_t top_margin, int_fast16_t bottom_margin, glyph_style style) {
    block_copy_lines_down(buffer, buffer->first_row, top_margin);
    block_copy_lines_down(buffer, internalRow(bottom_margin), ROWS - bottom_margin);
    buffer->first_row = (buffer->first_row + 1) % buffer->total_rows;
    clear_row(buffer->lines + internalRow(bottom_margin - 1), style);
}

void blockSet(term_buffer *buffer, const int16_t sx, const int16_t sy, const int16_t w, const int16_t h, const glyph glyph) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            buffer->lines[internalRow(sy + y)].text[sx + x] = glyph;
}
