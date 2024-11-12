//
// Created by Rohit Paul on 03-11-2024.
//

#ifndef NYX_TERM_RENDER_H
#define NYX_TERM_RENDER_H

#include "emulation/term_emulator.h"

void render_term(const Term_emulator *restrict emulator);

void render_term(const Term_emulator *emulator) {
    term_cursor cursor = emulator->cursor;
    Glyph glyph;
    for (int row = 0; row < ROWS; row++) {
        for (int column = 0; column < COLUMNS; column++) {
            glyph = emulator->screen_buff->lines[row][column];
        }
    }
}

#endif //NYX_TERM_RENDER_H
