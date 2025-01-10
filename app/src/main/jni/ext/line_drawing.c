#include <stdbool.h>
#include "core/terminal.h"

//
// Created by Rohit Paul on 30-12-2024.
//

enum {
    G0 = 1 << 13, G1 = 1 << 14, USE_G0 = 1 << 15,
};
#define set(bit, pos) (bit|=(pos))
#define clear(bit, pos) (bit&=~(pos))
#define set_val(bit, pos, val) (val)?set(bit,pos):clear(bit,pos)
#define get(bit, pos) (bit&(pos))
#define TERM_MODE emulator->cursor.decsetBit
#define ESC_STATE (emulator->esc_state&0x7F)
#define CURSOR emulator->cursor
#define CURSOR_GLYPH CURSOR.glyph

static void (*set_main_glyph)(terminal *const restrict);

extern void init_plugin();
void set_char(terminal *restrict emulator);
bool on_get_input(terminal *restrict emulator, unsigned char cp);
bool on_get_input(terminal *restrict const emulator, unsigned char cp) {
    switch (ESC_STATE) {
        case ESC_SELECT_LEFT_PAREN:
            set_val(TERM_MODE, G0, '0' == cp);
            break;
        case ESC_SELECT_RIGHT_PAREN:
            set_val(TERM_MODE, G1, '0' == cp);
            break;
        case ESC_CSI_EXCLAMATION:
        case ESC:
            if (cp == 'c' || cp == 'p') {
                set(TERM_MODE, USE_G0);
                clear(TERM_MODE, G0 | G1);
            }
            return false;
        default:
            switch (cp) {
                case 14:
                    set(TERM_MODE, USE_G0);
                    break;
                case 15:
                    clear(TERM_MODE, USE_G0);
                    break;
            }
    }
    return true;
}

void set_char(terminal *restrict const emulator) {
    static const int table[] = {L'█',    // Solid block '0'
                                L' ',    // Blank '_'
                                L'◆',    // Diamond '`'
                                L'▒',    // Checker board 'a'
                                L'␉',    // Horizontal tab
                                L'␌',    // Form feed
                                L'\r',   // Carriage return
                                L'␊',    // Linefeed
                                L'°',    // Degree
                                L'±',    // Plus-minus
                                L'\n',   // Newline
                                L'␋',    // Vertical tab
                                L'┘',    // Lower right corner
                                L'┐',    // Upper right corner
                                L'┌',    // Upper left corner
                                L'└',    // Lower left corner
                                L'┼',    // Crossing lines
                                L'⎺',    // Horizontal line - scan 1
                                L'⎻',    // Horizontal line - scan 3
                                L'─',    // Horizontal line - scan 5
                                L'⎼',    // Horizontal line - scan 7
                                L'⎽',    // Horizontal line - scan 9
                                L'├',    // T facing rightwards
                                L'┤',    // T facing leftwards
                                L'┴',    // T facing upwards
                                L'┬',    // T facing downwards
                                L'│',    // Vertical line
                                L'≤',    // Less than or equal to
                                L'≥',    // Greater than or equal to
                                L'π',    // Pi
                                L'≠',    // Not equal to
                                L'£',    // UK pound
                                L'·',    // Centered dot
    };
    if ((get(TERM_MODE, USE_G0 | G0) == (USE_G0 | G0)) || (get(TERM_MODE, G1 | USE_G0) == (G1)))
        switch (CURSOR_GLYPH.code) {

        }
    set_main_glyph(emulator);
}

void init_plugin() {

}
