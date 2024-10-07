#include <stdbool.h>
#include "term_style.h"

#define COLUMNS 45
typedef struct {
    glyph text[COLUMNS];
} term_row;

/**
 * Clears the line with provided cursor_style
 * */

void clear_row(term_row *row, glyph_style style) {
    for (int i = 0; i < COLUMNS; i++)
        row->text[i] = (glyph) {' ', style};
}