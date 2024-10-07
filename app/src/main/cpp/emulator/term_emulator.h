#include <unistd.h>
#include <wchar.h>
#include "term_buffer.h"
#include "term_constants.h"
#include "term_session.h"

typedef struct {
    u_char x;
    int16_t y;
    glyph_style style;
    DECSET_BIT decsetBit;
    TERM_MODE mode;
} term_cursor;

typedef struct {
    char *args;
    int len;
} OSC;

typedef struct {
    int arg[MAX_ESCAPE_PARAMETERS];
    char index: 7;
    u_char state;
    bool dontContinueSequence: 1;
} CSI;

typedef struct {
    int fd;
    pid_t pid;
    term_cursor cursor, saved_state, saved_state_alt;
    term_buffer *screen, main, alt;
    u_char leftMargin, rightMargin;
    short topMargin, bottomMargin;
    CSI csi;
    OSC osc;
    bool tabStop[COLUMNS];
    unsigned int colors[NUM_INDEXED_COLORS];
    uint_fast32_t last_codepoint;
} term_emulator;


#define finishSequence emulator->csi.state=ESC_NONE
#define continue_sequence(new_esc_state) {emulator->csi.state = new_esc_state;emulator->csi.dontContinueSequence=false;}
#define cursor_col emulator->cursor.x
#define cursor_row emulator->cursor.y
#define cursor_style emulator->cursor.style
#define left_margin emulator->leftMargin
#define right_margin emulator->rightMargin
#define top_margin emulator->topMargin
#define bottom_margin emulator->bottomMargin
#define esc_state emulator->csi.state
#define decset_bit emulator->cursor.decsetBit
#define term_mode emulator->cursor.mode
#define about_to_autowrap term_mode.ABOUT_TO_AUTO_WRAP

short mapDecSetBitToInternalBit(const int decsetBit) {
    switch (decsetBit) {
        case 1 :
            return APPLICATION_CURSOR_KEYS;
        case 5 :
            return REVERSE_VIDEO;
        case 6 :
            return ORIGIN_MODE;
        case 7 :
            return AUTOWRAP;
        case 25 :
            return CURSOR_ENABLED;
        case 66 :
            return APPLICATION_KEYPAD;
        case 69 :
            return LEFTRIGHT_MARGIN_MODE;
        case 1000 :
            return MOUSE_TRACKING_PRESS_RELEASE;
        case 1002 :
            return MOUSE_TRACKING_BUTTON_EVENT;
        case 1004 :
            return SEND_FOCUS_EVENTS;
        case 1006 :
            return MOUSE_PROTOCOL_SGR;
        case 2004 :
            return BRACKETED_PASTE_MODE;
        default:
            return -1;
    }
}

void sendMouseEvent(term_emulator *emulator, u_char button, const u_char x, int16_t y, const bool pressed) {
    if (!(MOUSE_LEFT_BUTTON_MOVED == button && decset_bit.MOUSE_TRACKING_BUTTON_EVENT) && decset_bit.MOUSE_PROTOCOL_SGR)
        dprintf(emulator->fd, "\033[<%d;%d;%d%c", button, x, y, (pressed ? 'M' : 'm'));
    else if ((255 - 32 > x) && (255 - 32 > y)) {
        u_char seq[] = {'\033', '[', 'M', (pressed ? button : 3) + 32, x + 32, y + 32};
        write(emulator->fd, seq, 6);
    }
}

void setDefaultTabStop(term_emulator *emulator) {
    for (int i = 0; i < COLUMNS; i++)
        emulator->tabStop[i] = i && 0 == (i & 7);
}


void reset_emulator(term_emulator *emulator) {
    emulator->csi.index = 0;
    esc_state = ESC_NONE;
    emulator->csi.dontContinueSequence = true;
    term_mode = (TERM_MODE) {0, 0, 0, 0, 1};
    top_margin = left_margin = 0;
    bottom_margin = ROWS;
    right_margin = COLUMNS;
    emulator->cursor.style.fg = COLOR_INDEX_FOREGROUND;
    emulator->cursor.style.bg = COLOR_INDEX_BACKGROUND;
    setDefaultTabStop(emulator);
    decset_bit.AUTOWRAP = true;
    decset_bit.CURSOR_ENABLED = true;
    emulator->saved_state = emulator->saved_state_alt = (term_cursor) {0, 0, COLOR_INDEX_FOREGROUND, COLOR_INDEX_BACKGROUND, {0},
                                                                       emulator->cursor.decsetBit, emulator->cursor.mode};
    reset_all_color(emulator->colors);
}


void doOscSetTextParameters(term_emulator *emulator, const char *const terminator) {
    char value = 0;
    char *text_param;
    for (int osc_index = 0; osc_index < emulator->osc.len; osc_index++) {
        const char b = emulator->osc.args[osc_index];
        if (';' == b) {
            text_param = emulator->osc.args + osc_index + 1;
            break;
        } else if ('0' <= b && '9' >= b)value = value * 10 + (b - '0');
        else {
            finishSequence;
            return;
        }
    }
    size_t len = strlen(text_param);
    bool end_of_input;
    switch (value) {
        case 0:
        case 1:
        case 2:
        case 52:
//                    {
//            //TODO copy to the clipboard base64_decode(strchr(text_param, ';'));
//        }
//            break;
        case 119:
        default:
            finishSequence;
            return;
        case 4: {
            int16_t color_index = -1;
            int16_t parsing_pair_start = -1;
            for (int16_t i = 0;; i++) {
                end_of_input = i == len;
                const char b = text_param[i];
                if (end_of_input || ';' == b) {
                    if (0 > parsing_pair_start) parsing_pair_start = i + 1;
                    else {
                        if (0 > color_index || 255 < color_index) {
                            finishSequence;
                            return;
                        } else {
                            parse_color_to_index(emulator->colors, color_index, text_param + parsing_pair_start, i - parsing_pair_start);
                            color_index = parsing_pair_start = -1;
                        }
                    }
                } else if (0 > parsing_pair_start && ('0' <= b && '9' >= b))
                    color_index = (0 > color_index ? 0 : color_index * 10) + (b - '0');
                else {
                    finishSequence;
                    return;
                }
                if (end_of_input)break;
            }
        }
            break;
        case 10:
        case 11:
        case 12: {
            int16_t special_index = COLOR_INDEX_FOREGROUND + value - 10;
            int16_t last_semi_index = 0;
            for (int16_t char_index = 0;; char_index++) {
                end_of_input = char_index == len;
                if (end_of_input || ';' == text_param[char_index]) {
                    char *color_spec = text_param + last_semi_index;
                    if ('?' == *color_spec) {
                        const unsigned int rgb = emulator->colors[special_index];
                        unsigned int r = (65535 * ((rgb & 0x00FF0000) >> 16)) / 255;
                        unsigned int g = (65535 * ((rgb & 0x0000FF00) >> 8)) / 255;
                        unsigned int b = (65535 * (rgb & 0x000000FF)) / 255;
                        dprintf(emulator->fd, "\033]%d;rgb:%04x/%04x/%04x%s", value, r, g, b, terminator);
                    } else
                        parse_color_to_index(emulator->colors, special_index, color_spec, char_index - last_semi_index);
                    special_index++;
                    if (end_of_input || COLOR_INDEX_CURSOR < special_index || ++char_index >= len) break;
                    last_semi_index = char_index;
                }
            }
        }
            break;

        case 104:
            if (len == 0) reset_all_color(emulator->colors);
            else {
                int16_t last_index = 0;
                for (int16_t char_index = 0;; char_index++) {
                    try:
                    end_of_input = char_index == len;
                    if (end_of_input || ';' == text_param[char_index]) {
                        char char_len = char_index - last_index;
                        int16_t index = 0;
                        char *color_string = text_param + last_index;
                        while (char_len--) {
                            char b = *color_string++;
                            if ('0' <= b && '9' >= b)
                                value = value * 10 + (b - '0');
                            else {
                                goto try;
                            }
                        }
                        reset_color(emulator->colors, index);
                        if (end_of_input) break;
                        last_index = ++char_index;
                    }
                }
            }
            break;
        case 110:
        case 111:
        case 112:
            reset_color(emulator->colors, COLOR_INDEX_FOREGROUND + value - 110);
            break;
    }
}

void scroll_down_one_line(term_emulator *emulator) {
    if (0 != left_margin || right_margin != COLUMNS) {
        blockCopy(emulator->screen, left_margin, top_margin + 1, right_margin - left_margin, bottom_margin - top_margin - 1, left_margin, top_margin);
        blockSet(emulator->screen, left_margin, bottom_margin - 1, right_margin - left_margin, 1, (glyph) {' ', cursor_style});
    } else scroll_down_one_line_buff(emulator->screen, top_margin, left_margin, cursor_style);
}

void do_line_feed(term_emulator *emulator) {
    int16_t new_cursor_row = cursor_row + 1;
    if (cursor_row >= bottom_margin) {
        if (cursor_row != ROWS - 1) {
            cursor_row = new_cursor_row;
            about_to_autowrap = false;
        }
    } else {
        if (new_cursor_row == bottom_margin) {
            scroll_down_one_line(emulator);
            new_cursor_row = bottom_margin - 1;
        }
        cursor_row = new_cursor_row;
        about_to_autowrap = false;
    }
}

void emit_codepoint(term_emulator *emulator, uint_fast32_t code_point) {
    emulator->last_codepoint = code_point;
    if (term_mode.USE_G0 ? term_mode.G0 : term_mode.G1) {
        switch (code_point) {
            case '_':
                code_point = L' '; // Blank.
                break;
            case '`':
                code_point = L'◆'; // Diamond.
                break;
            case '0':
                code_point = L'█'; // Solid block;
                break;
            case 'a':
                code_point = L'▒'; // Checker board.
                break;
            case 'b':
                code_point = L'␉'; // Horizontal tab.
                break;
            case 'c':
                code_point = L'␌'; // Form feed.
                break;
            case 'd':
                code_point = L'\r'; // Carriage return.
                break;
            case 'e':
                code_point = L'␊'; // Linefeed.
                break;
            case 'f':
                code_point = L'°'; // Degree.
                break;
            case 'g':
                code_point = L'±'; // Plus-minus.
                break;
            case 'h':
                code_point = L'\n'; // Newline.
                break;
            case 'i':
                code_point = L'␋'; // Vertical tab.
                break;
            case 'j':
                code_point = L'┘'; // Lower right corner.
                break;
            case 'k':
                code_point = L'┐'; // Upper right corner.
                break;
            case 'l':
                code_point = L'┌'; // Upper left corner.
                break;
            case 'm':
                code_point = L'└'; // Left left corner.
                break;
            case 'n':
                code_point = L'┼'; // Crossing lines.
                break;
            case 'o':
                code_point = L'⎺'; // Horizontal line - scan 1.
                break;
            case 'p':
                code_point = L'⎻'; // Horizontal line - scan 3.
                break;
            case 'q':
                code_point = L'─'; // Horizontal line - scan 5.
                break;
            case 'r':
                code_point = L'⎼'; // Horizontal line - scan 7.
                break;
            case 's':
                code_point = L'⎽'; // Horizontal line - scan 9.
                break;
            case 't':
                code_point = L'├'; // T facing rightwards.
                break;
            case 'u':
                code_point = L'┤'; // T facing leftwards.
                break;
            case 'v':
                code_point = L'┴'; // T facing upwards.
                break;
            case 'w':
                code_point = L'┬'; // T facing downwards.
                break;
            case 'x':
                code_point = L'│'; // Vertical line.
                break;
            case 'y':
                code_point = L'≤'; // Less than or equal to.
                break;
            case 'z':
                code_point = L'≥'; // Greater than or equal to.
                break;
            case '{':
                code_point = L'π'; // Pi.
                break;
            case '|':
                code_point = L'≠'; // Not equal to.
                break;
            case '}':
                code_point = L'£'; // UK pound.
                break;
            case '~':
                code_point = L'·'; // Centered dot.
                break;
            default:
                break;
        }
    }
    const bool autowrap = decset_bit.AUTOWRAP;
    const char display_width = wcwidth(code_point);
    const bool cursor_in_last_column = cursor_col == right_margin - 1;
    if (autowrap) {
        if (cursor_in_last_column && ((about_to_autowrap && 1 == display_width) || 2 == display_width)) {
            emulator->screen->lines[cursor_row].text[cursor_col].style.effect.AUTO_WRAPPED = true;
            cursor_col = left_margin;
            if (cursor_row + 1 < bottom_margin)cursor_row++;
            else scroll_down_one_line(emulator);
        }
    } else if (cursor_in_last_column && display_width == 2) return;
    if (term_mode.MODE_INSERT && 0 < display_width) {
        const u_char dest_col = cursor_col + display_width;
        if (dest_col < right_margin)
            blockCopy(emulator->screen, cursor_col, cursor_row, right_margin - dest_col, 1, dest_col, cursor_row);
    }
    emulator->screen->lines[cursor_row].text[cursor_col - (0 >= display_width && about_to_autowrap)] = (glyph) {code_point, cursor_style};
    if (autowrap && 0 < display_width) {
        about_to_autowrap = (cursor_col == (right_margin - display_width));
    }
    cursor_col = MIN(cursor_col + display_width, right_margin - 1);
}

#define save_cursor() if (emulator->screen == &emulator->alt) emulator->saved_state_alt = emulator->cursor; else emulator->saved_state = emulator->cursor;

#define restore_cursor() emulator->cursor = (emulator->screen == &emulator->alt) ? emulator->saved_state_alt : emulator->saved_state;

#define block_clear(sx, sy, w, h) blockSet(emulator->screen, sx, sy, w, h, (glyph) {' ', cursor_style})

#define clear_transcript() blockSet(&emulator->main, 0, 0, COLUMNS, transcript_row, (glyph) {' ', NORMAL})

void set_cursor_position_origin_mode(term_emulator *emulator, char x, short y) {
    bool origin_mode = decset_bit.ORIGIN_MODE;
    const short effective_top_margin = origin_mode ? top_margin : 0;
    const short effective_bottom_margin = origin_mode ? bottom_margin : ROWS;
    const u_char effective_left_margin = origin_mode ? left_margin : 0;
    const u_char effective_right_margin = origin_mode ? right_margin : COLUMNS;
    cursor_col = MAX(effective_left_margin, MIN(effective_left_margin + x, effective_right_margin - 1));;
    cursor_row = MAX(effective_top_margin, MIN(effective_top_margin + y, effective_bottom_margin - 1));;
    about_to_autowrap = false;
}

inline void set_cursor_row_col(term_emulator *emulator, u_char x, short y) {
    cursor_row = LIMIT(y, 0, ROWS - 1);
    cursor_col = LIMIT(x, 0, COLUMNS - 1);
    about_to_autowrap = false;
}

inline void do_esc(term_emulator *emulator, uint_fast32_t codepoint) {
    switch (codepoint) {
        case '#': continue_sequence(ESC_POUND)
            break;
        case '(': continue_sequence(ESC_SELECT_LEFT_PAREN)
            break;
        case ')': continue_sequence(ESC_SELECT_RIGHT_PAREN)
            break;
        case '6':
            if (cursor_col > left_margin)cursor_col--;
            else {
                const int16_t rows = bottom_margin - top_margin;
                blockCopy(emulator->screen, left_margin, top_margin, right_margin - left_margin - 1, rows, left_margin + 1, top_margin);
                blockSet(emulator->screen, right_margin - 1, top_margin, 1, rows, (glyph) {' ', cursor_style.fg, cursor_style.bg});
            }
            break;
        case '7':
            save_cursor()
            break;
        case '8':
            restore_cursor()
            break;
        case '9':
            if (cursor_col < right_margin - 1)cursor_col++;
            else {
                const int16_t rows = bottom_margin - top_margin;
                blockCopy(emulator->screen, left_margin + 1, top_margin, right_margin - left_margin - 1, rows, left_margin, top_margin);
                blockSet(emulator->screen, right_margin - 1, top_margin, 1, rows, (glyph) {' ', cursor_style.fg, cursor_style.bg, 0});
            }
            break;
        case 'c':
            reset_emulator(emulator);
            clear_transcript();
            block_clear(0, 0, COLUMNS, ROWS);
            set_cursor_position_origin_mode(emulator, 0, 0);
            break;
        case 'D':
            do_line_feed(emulator);
            break;
        case 'E':
            cursor_col = decset_bit.ORIGIN_MODE ? left_margin : 0;
            about_to_autowrap = false;
            do_line_feed(emulator);
            break;
        case 'F':
            set_cursor_row_col(emulator, 0, bottom_margin - 1);
            break;
        case 'H':
            emulator->tabStop[cursor_col] = true;
            break;
        case 'M':
            if (cursor_row <= top_margin) {
                blockCopy(emulator->screen, 0, top_margin, COLUMNS, bottom_margin - top_margin - 1, 0, top_margin + 1);
                block_clear(0, top_margin, COLUMNS, 1);
            } else
                cursor_row--;
            break;
        case 'N':
        case '0':
            break;
        case 'P':
            emulator->osc.len = 0;
            free(emulator->osc.args);
            continue_sequence(ESC_P)
            break;
        case '[': continue_sequence(ESC_CSI)
            break;
        case '=':
            decset_bit.APPLICATION_KEYPAD = true;
            break;
        case ']':
            emulator->osc.len = 0;
            free(emulator->osc.args);
            continue_sequence(ESC_OSC)
            break;
        case '>':
            decset_bit.APPLICATION_KEYPAD = false;
            break;
        default:
            finishSequence;
            break;
    }
}

int getArg(term_emulator *emulator, char index, short default_value, bool treat_zero_as_default) {
    short result = emulator->csi.arg[index];
    if (0 > result || (0 == result && treat_zero_as_default)) result = default_value;
    return result;
}

char next_tab_stop(term_emulator *emulator, char num_tabs) {
    for (char i = cursor_col + 1; i < COLUMNS; i++) {
        if (emulator->tabStop[i]) {
            if (0 == --num_tabs) return MIN(i, right_margin);
        }
    }
    return right_margin - 1;
}

void do_set_mode(term_emulator *emulator, bool new_value) {
    const char arg = getArg(emulator, 0, 0, 1);
    switch (arg) {
        case 4:
            term_mode.MODE_INSERT = new_value;
        case 34:
            break;
        default:
            finishSequence;
            break;
    }
}

inline void select_graphic_rendition(term_emulator *emulator) {
    if (emulator->csi.index >= MAX_ESCAPE_PARAMETERS) emulator->csi.index = MAX_ESCAPE_PARAMETERS - 1;
    for (char i = 0; i < emulator->csi.index; i++) {
        char code = emulator->csi.arg[i];
        if (0 > code) {
            if (0 < emulator->csi.index)continue;
            else code = 0;
        }
        switch (code) {
            case 0:
                cursor_style = NORMAL;
                break;
            case 1:
                cursor_style.effect.BOLD = 1;
                break;
            case 2:
                cursor_style.effect.DIM = 1;
                break;
            case 3:
                cursor_style.effect.ITALIC = 1;
                break;
            case 4:
                cursor_style.effect.UNDERLINE = 1;
                break;
            case 5:
                cursor_style.effect.BLINK = 1;
                break;
            case 7:
                cursor_style.effect.INVERSE = 1;
                break;
            case 8:
                cursor_style.effect.INVISIBLE = 1;
                break;
            case 9:
                cursor_style.effect.STRIKETHROUGH = 1;
                break;
            case 22:
                cursor_style.effect.BOLD = 0;
                cursor_style.effect.DIM = 0;
                break;
            case 23:
                cursor_style.effect.ITALIC = 0;
                break;
            case 24:
                cursor_style.effect.UNDERLINE = 0;
                break;
            case 25:
                cursor_style.effect.BLINK = 0;
                break;
            case 27:
                cursor_style.effect.INVERSE = 0;
                break;
            case 28:
                cursor_style.effect.INVISIBLE = 0;
                break;
            case 29:
                cursor_style.effect.STRIKETHROUGH = 0;
                break;
            case 38:
            case 48: {
                if (i + 2 > emulator->csi.index) continue;
                const short first_arg = emulator->csi.arg[i + 1];
                switch (first_arg) {
                    case 2:
                        if (i + 4 <= emulator->csi.index) {
                            short red = emulator->csi.arg[i + 2];
                            short green = emulator->csi.arg[i + 3];
                            short blue = emulator->csi.arg[i + 4];
                            if (0 > red || 0 > green || 0 > blue || 255 < red || 255 < green || 255 < blue)
                                finishSequence;
                            else {
                                unsigned int argb_color = 0xff000000 | (red << 16) | (green << 8) | blue;
                                if (38 == code)cursor_style.fg = argb_color;
                                else
                                    cursor_style.bg = argb_color;
                            }
                            i += 4;
                        }
                        break;
                    case 5: {
                        short color = emulator->csi.arg[i + 2];
                        i += 2;
                        if (0 <= color && NUM_INDEXED_COLORS > color) {
                            if (38 == code) cursor_style.fg = color;
                            else
                                cursor_style.bg = color;
                        }
                    }
                        break;
                    default:
                        finishSequence;
                        break;
                }
            }
                break;
            case 39:
                cursor_style.fg = COLOR_INDEX_FOREGROUND;
                break;
            case 49:
                cursor_style.bg = COLOR_INDEX_BACKGROUND;
                break;
            default:
                if (30 <= code && 37 >= code) cursor_style.fg = code - 30;
                else if (40 <= code && 47 >= code) cursor_style.bg = code - 40;
                else if (90 <= code && 97 >= code) cursor_style.fg = code - 90 + 8;
                else if (100 <= code && 107 >= code) cursor_style.bg = code - 100 + 8;
                break;
        }
    }
}

void parse_arg(term_emulator *emulator, char input_byte) {
    if ('0' <= input_byte && '9' >= input_byte) {
        if (emulator->csi.index < MAX_ESCAPE_PARAMETERS) {
            short *old_value = emulator->csi.arg + emulator->csi.index;
            char this_digit = input_byte - '0';
            short value = (0 < *old_value) ? *old_value * 10 + this_digit : this_digit;
            if (9999 < value) value = 9999;
            *old_value = value;
        }
        continue_sequence(emulator->csi.state)
    } else if (';' == input_byte) {
        if (emulator->csi.index < MAX_ESCAPE_PARAMETERS) emulator->csi.index++;
        continue_sequence(emulator->csi.state)
    } else
        finishSequence;
}

inline void do_csi(term_emulator *emulator, uint_fast32_t codepoint) {
    switch (codepoint) {
        case '!': continue_sequence(ESC_CSI_EXCLAMATION)
            break;
        case '"': continue_sequence(ESC_CSI_DOUBLE_QUOTE)
            break;
        case '\'': continue_sequence(ESC_CSI_SINGLE_QUOTE)
            break;
        case '$': continue_sequence(ESC_CSI_DOLLAR)
            break;
        case '*': continue_sequence(ESC_CSI_ARGS_ASTERIX)
            break;
        case '@': {
            about_to_autowrap = false;
            const u_char columns_after_cursor = COLUMNS - cursor_col;
            const u_char spaces_to_insert = MIN(getArg(emulator, 0, 1, true), columns_after_cursor);
            const u_char chars_to_move = columns_after_cursor - spaces_to_insert;
            blockCopy(emulator->screen, cursor_col, cursor_row, chars_to_move, 1, cursor_col + spaces_to_insert, cursor_row);
            block_clear(cursor_col, cursor_row, spaces_to_insert, 1);
        }
            break;
        case 'A':
            cursor_row = MAX(0, cursor_row - getArg(emulator, 0, 1, 1));
            about_to_autowrap = false;
            break;
        case 'B':
            cursor_row = MIN(ROWS - 1, cursor_row + getArg(emulator, 0, 1, 1));
            about_to_autowrap = false;
            break;
        case 'C':
        case 'a':
            cursor_col = MIN(right_margin - 1, cursor_col + getArg(emulator, 0, 1, 1));
            about_to_autowrap = false;
            break;
        case 'D':
            cursor_col = MAX(left_margin, cursor_col - getArg(emulator, 0, 1, 1));
            about_to_autowrap = false;
            break;
        case 'E':
            set_cursor_position_origin_mode(emulator, 0, cursor_row + getArg(emulator, 0, 1, 1));
            break;
        case 'F':
            set_cursor_position_origin_mode(emulator, 0, cursor_row - getArg(emulator, 0, 1, 1));
            break;
        case 'G':
            cursor_col = LIMIT(getArg(emulator, 0, 1, 1), 1, COLUMNS);
            about_to_autowrap = false;
            break;
        case 'H':
        case 'f':
            set_cursor_position_origin_mode(emulator, getArg(emulator, 1, 1, 1) - 1, (short) getArg(emulator, 0, 1, 1));
            break;
        case 'I':
            cursor_col = next_tab_stop(emulator, getArg(emulator, 0, 1, 1));
            about_to_autowrap = false;
            break;
        case 'J': {
            char arg = getArg(emulator, 0, 0, 1);
            switch (arg) {
                case 0:
                    block_clear(cursor_col, cursor_row, COLUMNS - cursor_col, 1);
                    block_clear(0, cursor_row + 1, COLUMNS, ROWS - cursor_row - 1);
                    break;
                case 1:
                    block_clear(0, 0, COLUMNS, cursor_row);
                    block_clear(0, cursor_row, cursor_col + 1, 1);
                    break;
                case 2:
                    block_clear(0, 0, COLUMNS, ROWS);
                    break;
                case 3:
                    clear_transcript();
                    break;
                default:
                    finishSequence;
                    return;

            }
            about_to_autowrap = false;
        }
            break;
        case 'K': {
            char arg = getArg(emulator, 0, 0, 1);
            switch (arg) {
                case 0:
                    block_clear(cursor_col, cursor_row, COLUMNS - cursor_col, 1);
                    break;
                case 1:
                    block_clear(0, cursor_row, cursor_col + 1, 1);
                    break;
                case 2:
                    block_clear(0, cursor_row, COLUMNS, 1);
                    break;
                default:
                    finishSequence;
                    return;
            }
            about_to_autowrap = false;
        }
            break;
        case 'L': {
            short arg = getArg(emulator, 0, 1, 1);
            const short lines_after_cursor = bottom_margin - cursor_row;
            const short lines_to_insert = MIN(arg, lines_after_cursor);
            const short lines_to_move = lines_after_cursor - lines_to_insert;
            blockCopy(emulator->screen, 0, cursor_row, COLUMNS, lines_to_move, 0, cursor_row + lines_to_insert);
            block_clear(0, cursor_row + lines_to_move, COLUMNS, lines_to_insert);
        }
            break;
        case 'M': {
            about_to_autowrap = false;
            short arg = getArg(emulator, 0, 1, 1);
            const short lines_after_cursor = bottom_margin - cursor_row;
            const short lines_to_delete = MIN(arg, lines_after_cursor);
            const short lines_to_move = lines_after_cursor - lines_to_delete;
            blockCopy(emulator->screen, 0, cursor_row + lines_to_delete, COLUMNS, lines_to_move, 0, cursor_row);
            block_clear(0, cursor_row + lines_to_move, COLUMNS, lines_to_delete);
        }
            break;
        case 'P': {
            about_to_autowrap = false;
            char arg = getArg(emulator, 0, 1, 1);
            const char cells_after_cursor = COLUMNS - cursor_col;
            const char cells_to_delete = MIN(arg, cells_after_cursor);
            const char cells_to_move = cells_after_cursor - cells_to_delete;
            blockCopy(emulator->screen, cursor_col + cells_to_delete, cursor_row, cells_to_move, 1, cursor_col, cursor_row);
            block_clear(cursor_col + cells_to_move, cursor_row, cells_to_delete, 1);
        }
            break;
        case 'S': {
            const short lines_to_scroll = getArg(emulator, 0, 1, 1);
            for (short i = 0; i < lines_to_scroll; i++) scroll_down_one_line(emulator);
        }
            break;
        case 'T':
            if (0 == emulator->csi.index) {
                const short lines_to_scroll_arg = getArg(emulator, 0, 1, 1);
                const char lines_between_top_bottom_margin = bottom_margin - top_margin;
                const char lines_to_scroll = MIN(lines_between_top_bottom_margin, lines_to_scroll_arg);
                blockCopy(emulator->screen, 0, top_margin, COLUMNS, lines_between_top_bottom_margin - lines_to_scroll, 0,
                          top_margin + lines_to_scroll);
                block_clear(0, top_margin, COLUMNS, lines_to_scroll);
            } else { finishSequence; }
            break;
        case 'X': {
            about_to_autowrap = false;
            short arg = getArg(emulator, 0, 1, 1);
            blockSet(emulator->screen, cursor_col, cursor_row, MIN(arg, COLUMNS - cursor_col), 1, (glyph) {' ', cursor_style});
        }
            break;
        case 'Z': {
            char number_of_tabs = getArg(emulator, 0, 1, 1);
            char new_col = left_margin;
            for (int i = cursor_col - 1; 0 <= i; i--) {
                if (0 == --number_of_tabs) {
                    new_col = MAX(i, new_col);
                }
            }
            cursor_col = new_col;
        }
            break;
        case '?': continue_sequence(ESC_CSI_QUESTIONMARK)
            break;
        case '>': continue_sequence(ESC_CSI_BIGGERTHAN)
            break;
        case '`':
            set_cursor_position_origin_mode(emulator, getArg(emulator, 0, 1, 1) - 1, cursor_row);
            break;
        case 'b': {
            if (-1 == emulator->last_codepoint) break;
            short num_repeat = getArg(emulator, 0, 1, 1);
            while (0 < num_repeat--)
                emit_codepoint(emulator, emulator->last_codepoint);
        }
            break;
        case 'c':
            if (0 == getArg(emulator, 0, 0, 1)) write(emulator->fd, "\033[?64;1;2;6;9;15;18;21;22c", 26);
            break;
        case 'd': {
            short arg = getArg(emulator, 0, 1, 1);
            cursor_row = LIMIT(arg, 1, ROWS) - 1;
            about_to_autowrap = false;
        }
            break;
        case 'e':
            set_cursor_position_origin_mode(emulator, cursor_col, cursor_row + getArg(emulator, 0, 1, 1));
            break;
        case 'g': {
            char arg = getArg(emulator, 0, 1, 1);
            switch (arg) {
                case 0:
                    emulator->tabStop[cursor_col] = false;
                    break;
                case 3:
                    for (int i = 0; i < COLUMNS; i++)
                        emulator->tabStop[i] = false;
                    break;
            }
        }
            break;
        case 'h':
            do_set_mode(emulator, true);
            break;
        case 'l':
            do_set_mode(emulator, false);
            break;
        case 'm':
            select_graphic_rendition(emulator);
            break;
        case 'n': {
            char arg = emulator->csi.arg[0];
            switch (arg) {
                case 5: {
                    char byte[] = {27, '[', '0', 'n'};
                    write(emulator->fd, byte, 4);
                }
                    break;
                case 6:
                    dprintf(emulator->fd, "\033[%d;%dR", cursor_row + 1, cursor_col + 1);
                    break;
            }
        }
            break;
        case 'r': {
            short arg0 = emulator->csi.arg[0] - 1, arg1 = emulator->csi.arg[1];
            top_margin = LIMIT(arg0, 0, ROWS - 2);
            bottom_margin = LIMIT(arg1, top_margin + 2, ROWS);
            set_cursor_position_origin_mode(emulator, 0, 0);
        }
            break;
        case 's':
            if (decset_bit.LEFTRIGHT_MARGIN_MODE) {
                char arg0 = emulator->csi.arg[0] - 1, arg1 = emulator->csi.arg[1];
                left_margin = LIMIT(arg0, 0, COLUMNS - 2);
                right_margin = LIMIT(arg1, left_margin + 1, COLUMNS);
                set_cursor_position_origin_mode(emulator, 0, 0);
            } else save_cursor()
            break;
        case 't': {
            char arg = emulator->csi.arg[0];
            switch (arg) {
                case 11:
                    write(emulator->fd, "\033[1t", 4);
                    break;
                case 13:
                    write(emulator->fd, "\033[3;0;0t", 8);
                    break;
                case 14:
                    dprintf(emulator->fd, "\033[4;%d;%dt", ROWS * 12, COLUMNS * 12);
                    break;
                case 18:
                    dprintf(emulator->fd, "\033[8;%d;%dt", ROWS, COLUMNS);
                    break;
                case 19:
                    dprintf(emulator->fd, "\033[9;%d;%dt", ROWS, COLUMNS);
                    break;
                case 20:
                    write(emulator->fd, "\033]LIconLabel\033\\", 14);
                    break;
                case 21:
                    write(emulator->fd, "\033]l\033\\", 5);
                    break;
            }
        }
            break;
        case 'u':
            restore_cursor()
            break;
        case ' ': continue_sequence(ESC_CSI_ARGS_SPACE)
            break;
        default:
            parse_arg(emulator, codepoint);
            break;
    }
}

void process_codepoint(term_emulator *emulator, uint_fast32_t codepoint) {
    switch (codepoint) {
        case 0:
            break;
        case 7:
            if (esc_state == ESC_OSC) doOscSetTextParameters(emulator, "\007");
            break;
        case 8:
            if (left_margin == cursor_col) {
                const int16_t prev_row = cursor_row - 1;
                text_effect *effect_bit = &emulator->screen->lines[prev_row].text[right_margin - 1].style.effect;
                if (0 <= prev_row && effect_bit->AUTO_WRAPPED) {
                    effect_bit->AUTO_WRAPPED = false;
                    set_cursor_row_col(emulator, prev_row, right_margin - 1);
                }
            } else {
                cursor_col--;
                about_to_autowrap = false;
            }
            break;
        case 9: {
            u_char i = cursor_col + 1;
            cursor_col = (i < COLUMNS) ? MIN(i, right_margin) : right_margin - 1;
        }
            break;
        case 10:
        case 11:
        case 12:
            do_line_feed(emulator);
            break;
        case 13:
            cursor_col = left_margin;
            about_to_autowrap = 0;
            break;
        case 14:
            term_mode.USE_G0 = true;
            break;
        case 15:
            term_mode.USE_G0 = false;
            break;
        case 24:
        case 26:
            if (ESC_NONE != esc_state) {
                finishSequence;
                emit_codepoint(emulator, 127);
            }
            break;
        case 27:
            if (ESC_OSC == esc_state) continue_sequence(ESC_OSC_ESC)
            else {
                esc_state = ESC;
                emulator->csi.index = 0;
                for (int i = 0; i < MAX_ESCAPE_PARAMETERS; i++)
                    emulator->csi.arg[i] = -1;
            }
            break;
        default: {
            emulator->csi.dontContinueSequence = true;
            switch (esc_state) {
                case ESC_NONE:
                    if (32 <= codepoint)emit_codepoint(emulator, codepoint);
                    break;
                case ESC:
                    do_esc(emulator, codepoint);
                    break;
                case ESC_POUND:
                    if ('8' == codepoint) blockSet(emulator->screen, 0, 0, COLUMNS, ROWS, (glyph) {'E', cursor_style});
                    else
                        finishSequence;
                    break;
                case ESC_SELECT_LEFT_PAREN:
                    term_mode.G0 = '0' == codepoint;
                    break;
                case ESC_SELECT_RIGHT_PAREN:
                    term_mode.G1 = '0' == codepoint;
                    break;
                case ESC_CSI:
                    do_csi(emulator, codepoint);
                    break;
                case ESC_CSI_EXCLAMATION:
                    if ('p' == codepoint) reset_emulator(emulator);
                    else
                        finishSequence;
                    break;
                case ESC_CSI_QUESTIONMARK:
                    switch (codepoint) {
                        case 'c':
                            write(emulator->fd, "\033[>41;320;0c", 12);
                            break;
                        case 'm':
                            break;
                        default:
                            parse_arg(emulator, codepoint);
                    }
                    break;
                case ESC_CSI_DOLLAR: {
                    bool origin_mode = decset_bit.ORIGIN_MODE;
                    const short effective_top_margin = origin_mode ? top_margin : 0;
                    const short effective_bottom_margin = origin_mode ? bottom_margin : ROWS;
                    const u_char effective_left_margin = origin_mode ? left_margin : 0;
                    const u_char effective_right_margin = origin_mode ? right_margin : COLUMNS;
                    switch (codepoint) {
                        case 'v': {
                            short arg0 = getArg(emulator, 0, 1, 1) - 1 + effective_top_margin;
                            short arg1 = getArg(emulator, 1, 1, 1) - 1 + effective_left_margin;
                            short arg2 = getArg(emulator, 2, ROWS, 1) + effective_top_margin;
                            short arg3 = getArg(emulator, 3, COLUMNS, 1) + effective_left_margin;
                            short arg5 = getArg(emulator, 5, 1, 1) - 1 + effective_top_margin;
                            short arg6 = getArg(emulator, 6, 1, 1) - 1 + effective_left_margin;
                            short top_source = MIN(arg0, ROWS);
                            short left_source = MIN(arg1, COLUMNS);
                            short bottom_source = LIMIT(arg2, top_source, ROWS);
                            short right_source = LIMIT(arg3, left_source, COLUMNS);
                            short dest_top = MIN(arg5, ROWS);
                            short dest_left = MIN(arg6, COLUMNS);
                            short height_to_copy = MIN(ROWS - dest_top, bottom_source - top_source);
                            short width_to_copy = MIN(COLUMNS - dest_left, right_source - left_source);
                            blockCopy(emulator->screen, left_source, top_source, width_to_copy, height_to_copy, dest_left, dest_top);
                        }
                            break;
                        case '{':
                        case 'x':
                        case 'z': {
                            const bool erase = 'x' != codepoint, selective = '{' == codepoint, keep_attributes = erase && selective;
                            char arg_index = 0;
                            int fill_char = erase ? ' ' : getArg(emulator, arg_index++, -1, 1);
                            if ((32 <= fill_char && 126 >= fill_char) || (160 <= fill_char && 255 >= fill_char)) {
                                short top = getArg(emulator, arg_index++, 1, 1) + effective_top_margin;
                                top = MIN(top, effective_bottom_margin + 1);
                                short left = getArg(emulator, arg_index++, 1, true) + effective_top_margin;
                                left = MIN(left, effective_right_margin + 1);
                                short bottom = getArg(emulator, arg_index++, ROWS, true) + effective_top_margin;
                                bottom = MIN(bottom, effective_bottom_margin);
                                short right = getArg(emulator, arg_index, COLUMNS, true) + effective_left_margin;
                                right = MIN(right, effective_right_margin);
                                for (int row = top - 1; row < bottom; row++) {
                                    for (int col = left - 1; col < right; col++) {
                                        glyph *glyph1 =
                                                emulator->screen->lines[(row + emulator->screen->first_row) % emulator->screen->total_rows].text +
                                                col;
                                        if (!selective || glyph1->style.effect.PROTECTED == 0) {
                                            glyph1->code = fill_char;
                                            if (!keep_attributes) glyph1->style = cursor_style;
                                        }
                                    }
                                }
                            }
                        }
                            break;
                        case 'r':
                        case 't': {
                            const bool reverse = 't' == codepoint;
                            short arg0 = getArg(emulator, 0, 1, 1) - 1, arg1 = getArg(emulator, 1, 1, 1) - 1, arg2 =
                                    getArg(emulator, 2, ROWS, 1) + 1, arg3 = getArg(emulator, 3, COLUMNS, 1) + 1;
                            short top = MIN(arg0, effective_bottom_margin) + effective_top_margin, left =
                                    MIN(arg1, effective_right_margin) + effective_left_margin, bottom =
                                    MIN(arg2, effective_bottom_margin - 1) + effective_top_margin, right =
                                    MIN(arg3, effective_right_margin - 1) + effective_left_margin;
                            if (4 <= emulator->csi.index) {
                                if (emulator->csi.index >= MAX_ESCAPE_PARAMETERS)
                                    emulator->csi.index = MAX_ESCAPE_PARAMETERS - 1;
                                for (char i = 4; i < emulator->csi.index; i++) {
                                    bool set_or_clear = true;
                                    text_effect effect;
                                    short arg = emulator->csi.arg[i];
                                    switch (arg) {
                                        case 0:
                                            if (!reverse)set_or_clear = false;
                                            effect.BOLD = effect.UNDERLINE = effect.BLINK = effect.INVERSE = 1;
                                            break;
                                        case 1:
                                            effect.BOLD = 1;
                                            break;
                                        case 4:
                                            effect.UNDERLINE = 1;
                                            break;
                                        case 5:
                                            effect.BLINK = 1;
                                            break;
                                        case 7:
                                            effect.INVERSE = 1;
                                            break;
                                        case 22:
                                            set_or_clear = false;
                                            effect.BOLD = 1;
                                            break;
                                        case 24:
                                            set_or_clear = false;
                                            effect.UNDERLINE = 1;
                                            break;
                                        case 25:
                                            set_or_clear = false;
                                            effect.BLINK = 1;
                                            break;
                                        case 27:
                                            set_or_clear = false;
                                            effect.INVERSE = 1;
                                            break;
                                        default:
                                            effect = (text_effect) {0};
                                            break;
                                    }
                                    if (!reverse || set_or_clear) {
                                        for (int y = top; y < bottom; y++) {
                                            const term_row line = emulator->screen->lines[(emulator->screen->first_row + y) %
                                                                                          emulator->screen->total_rows];
                                            short x = decset_bit.RECTANGULAR_CHANGEATTRIBUTE || y == top ? left : effective_left_margin;
                                            const short end_of_line =
                                                    decset_bit.RECTANGULAR_CHANGEATTRIBUTE || y + 1 == bottom ? right : effective_right_margin;
                                            for (; x < end_of_line; x++) {
                                                
                                            }
                                        }
                                    }
                                }
                            }
                        }
                            break;
                        default:
                            finishSequence;
                            break;
                    }
                }
                    break;
            }
        }

    }
}

void init_term_emulator(term_emulator *emulator) {
    init_term_buffer(&emulator->main, transcript_row);
    init_term_buffer(&emulator->alt, ROWS);
    emulator->screen = &emulator->main;
    reset_emulator(emulator);
//TODO : START pseudo terminal and read it
}

