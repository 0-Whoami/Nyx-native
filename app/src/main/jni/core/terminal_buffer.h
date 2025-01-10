//
// Created by Rohit Paul on 28-12-2024.
//

#ifndef NYX_TERMINAL_BUFFER_H
#define NYX_TERMINAL_BUFFER_H

#include "terminal.h"

typedef struct {
    void (*allocate_buffers)(terminal *const restrict);

    void (*set_glyph)(terminal *const restrict);

    void (*block_copy)(terminal *restrict const emulator, const i8 sx, const i8 sy, const i8 w, i8 h, const i8 dx, const i8 dy);

    void (*block_set)(terminal *restrict const emulator, const i8 sx, const i8 sy, const i8 w, i8 h);

    void
    (*block_set_attr)(terminal *restrict const emulator, const i8 sx, const i8 sy, const i8 w, const i8 h, bool not_selective,
                      bool dont_keep_attribute);

    void (*set_or_clear_effect)(terminal *restrict const emulator,
                                const ui8 bits,
                                const bool set,
                                const bool reverse,
                                const i8 top,
                                const i8 bottom,
                                const i8 left,
                                const i8 right,
                                const i8 right_margin,
                                const i8 left_margin);

    void (*scroll_down)(terminal *const restrict, i8);

    void (*clear_transcript)(terminal *restrict const);

    void (*set_line_wrap)(terminal *const restrict, i8, bool);

    bool (*get_line_wrap)(terminal *const restrict, i8);

    void (*free_buffers)(terminal *const restrict);
} buffer_operation;

typedef struct {
    Glyph *data;
    ui8 *auto_wrapped_lines;
} Terminal_buffer;

extern buffer_operation BUFF_OP;

#endif //NYX_TERMINAL_BUFFER_H
