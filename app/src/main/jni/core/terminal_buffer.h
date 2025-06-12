//
// Created by Rohit Paul on 28-12-2024.
//

#ifndef NYX_TERMINAL_BUFFER_H
#define NYX_TERMINAL_BUFFER_H

#include "terminal.h"

void allocate_buffers(const terminal *restrict);

void set_glyph(const terminal *restrict);

void block_copy(const terminal *restrict emulator, i8 sx, i8 sy, i8 w, i8 h, i8 dx, i8 dy);

void block_set(const terminal *restrict emulator, i8 sx, i8 sy, i8 w, i8 h);

void
block_set_attribute(const terminal *restrict emulator, i8 sx, i8 sy, i8 w, i8 h, bool not_selective, bool dont_keep_attribute);

void
set_or_clear_effect(const terminal *restrict emulator, ui8 bits, bool set, bool reverse, i8 top, i8 bottom, i8 left, i8 right, i8 right_margin,
                    i8 left_margin);

void scroll_down(terminal *restrict, i8);

void clear_transcript(const terminal *restrict);

void set_line_wrap(const terminal *restrict, i8, bool);

void switch_buffers(terminal *restrict);

bool get_line_wrap(const terminal *restrict, i8);

void free_buffers(const terminal *restrict);

#endif //NYX_TERMINAL_BUFFER_H
