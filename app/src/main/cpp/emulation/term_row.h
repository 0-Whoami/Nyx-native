#ifndef TERM_ROW
#define TERM_ROW

#include "term_style.h"

typedef Glyph *restrict Term_row;

void clear_row(Term_row row, int col, glyph_style *s);

void clear_row(Term_row row, int col, glyph_style *s) {
    Glyph g = {.code=' ', .style=*s};
    for (int c = 0; c < col; c++) row[c] = g;
}

#endif //TERM_ROW
