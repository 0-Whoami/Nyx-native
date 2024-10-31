#ifndef TERM_BUFFER
#define TERM_BUFFER

#include <stdbool.h>
#include "term_row.h"

typedef struct {
    Term_row *restrict lines;
    int first_row;
    int total_rows;
} Term_buffer;


void init_term_buffer(Term_buffer *restrict buffer, int rows, int columns);

void destroy_term_buffer(Term_buffer *restrict buffer);

void block_copy(Term_buffer *restrict buffer, int sx, int sy, int w, int h, int dx, int dy);

void block_set(Term_buffer *restrict buffer, int sx, int sy, int w, int h, Glyph glyph);

#define internalRow(ex_row) (((ex_row) + buffer->first_row)%buffer->total_rows)

void init_term_buffer(Term_buffer *restrict const buffer, const int rows, const int columns) {
    Glyph glyph = {NORMAL, ' '};
    const size_t size = sizeof(Glyph) * columns;
    buffer->total_rows = rows;
    buffer->first_row = 0;
    buffer->lines = (Term_row *) malloc(rows * sizeof(Term_row));
    for (int i = 0; i < rows; i++) {
        buffer->lines[i] = (Term_row) malloc(size);
        for (int j = 0; j < columns; j++) buffer->lines[i][j] = glyph;
    }
}

void destroy_term_buffer(Term_buffer *restrict const buffer) {
    for (int i = 0; i < buffer->total_rows; i++) free(buffer->lines[i]);
    free((void*)buffer->lines);
}

void block_copy(Term_buffer *restrict const buffer, const int sx, const int sy, const int w, const int h, const int dx, const int dy) {
    if (w) {
        const bool copyingUp = sy > dy;
        for (int y = 0; y < h; y++) {
            const int y2 = copyingUp ? y : h - y - 1;
            Glyph *restrict const src = buffer->lines[internalRow(dy + y2)] + dx;
            Glyph *restrict const dst = buffer->lines[internalRow(sy + y2)] + sx;
            for (int x = 0; x < w; x++) src[x] = dst[x];
        }
    }
}

void block_set(Term_buffer *restrict const buffer, const int sx, const int sy, const int w, const int h, const Glyph glyph) {
    for (int y = 0; y < h; y++) {
        const int row = internalRow(sy + y);
        for (int x = 0; x < w; x++)
            buffer->lines[row][sx + x] = glyph;
    }
}

#endif
