#ifndef TERM_BUFFER
#define TERM_BUFFER

#include <stdbool.h>
#include "term_style.h"

typedef Glyph *Term_row;

typedef struct {
    Term_row *lines;
    int history_rows;
} Term_buffer;


void init_term_buffer(Term_buffer *restrict buffer, int rows, int columns);

void block_copy(Term_buffer *restrict buffer, int sx, int sy, int w, int h, int dx, int dy);

void block_set(Term_buffer *restrict buffer, int sx, int sy, int w, int h, Glyph glyph);

#define internal_row_buff(buffer, ex_row) (buffer->history_rows+ex_row)
#define internal_row(ex_row) internal_row_buff(buffer,ex_row)

void init_term_buffer(Term_buffer *restrict const buffer, const int rows, const int columns) {
    Glyph glyph = {.style=NORMAL, .code= ' '};
    const size_t size = sizeof(Glyph) * columns;
    buffer->history_rows = 0;
    buffer->lines = (Term_row *) malloc(rows * sizeof(Term_row));
    for (int i = 0; i < rows; i++) {
        buffer->lines[i] = (Term_row) malloc(size);
        for (int j = 0; j < columns; j++) buffer->lines[i][j] = glyph;
    }
}

void block_copy(Term_buffer *restrict const buffer, const int sx, const int sy, const int w, const int h, const int dx, const int dy) {
    if (w) {
        int y2;
        Glyph *restrict src, *restrict dst;
        const bool copyingUp = sy > dy;
        for (int y = 0; y < h; y++) {
            y2 = copyingUp ? y : h - 1 - y;
            src = buffer->lines[internal_row(dy + y2)] + dx;
            dst = buffer->lines[internal_row(sy + y2)] + sx;
            for (int x = 0; x < w; x++) src[x] = dst[x];
        }
    }
}

void block_set(Term_buffer *restrict const buffer, const int sx, const int sy, const int w, int h, const Glyph glyph) {
    Glyph *restrict ptr;
    for (int y = 0; y < h; y++) {
        ptr = buffer->lines[internal_row(sy)] + sx;
        for (int x = 0; x < w; x++) ptr[x] = glyph;
    }
}

#endif
