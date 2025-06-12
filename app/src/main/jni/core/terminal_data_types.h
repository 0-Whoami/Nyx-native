//
// Created by Rohit Paul on 21-12-2024.
//

#ifndef NYX_TERMINAL_DATA_TYPES_H
#define NYX_TERMINAL_DATA_TYPES_H


#include <stdint.h>

typedef int_fast8_t i8;
typedef uint_fast8_t ui8;

typedef struct {
    ui8 effect, bg, fg;
} glyph_style;

typedef union {
    struct {
        glyph_style style;      //Style
        char code;     //Character
    };
    void *ptr;
} Glyph;

#endif //NYX_TERMINAL_DATA_TYPES_H
