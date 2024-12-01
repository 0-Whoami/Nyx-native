//
// Created by Rohit Paul on 13-11-2024.
//

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <bits/termios_inlines.h>
#include "core/terminal.h"

typedef Glyph *Term_row;

typedef struct {
    Term_row *lines;
    int history_rows;
} T_buff;

#define ASCII_MAX      0b01111111 //0xxxxxxx
#define UTF_2_BYTE_MAX 0b11011111 //110xxxxx
#define UTF_3_BYTE_MAX 0b11101111 //1110xxxx
#define UTF_4_BYTE_MAX 0b11110111 //11110xxx

#define UNICODE_REPLACEMENT_CHARACTER 0xFFFD
#define MAX_ESCAPE_PARAMETERS 16
#define MAX_OSC_STRING_LENGTH 2048
#define BUFFER_SIZE 2048

#define set(bit, pos) (bit|=(pos))
#define clear(bit, pos) (bit&=~(pos))
#define get(bit, pos) (bit&(pos))
#define set_val(bit, pos, val) (val)?set(bit,pos):clear(bit,pos)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, low, high) (x >= low && x <= high)
#define LIMIT(x, a, b) (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define CELI(a, b) ((a+b-1)/b)

typedef enum {
    BLOCK = 0, C_UNDERLINE = 1, BAR = 2
} CURSOR_SHAPE;

enum EscapeState {
    ESC_NONE = 0,
    ESC,
    ESC_POUND,
    ESC_SELECT_LEFT_PAREN,
    ESC_SELECT_RIGHT_PAREN,
    ESC_CSI,
    ESC_CSI_QUESTIONMARK,
    ESC_CSI_DOLLAR,
    ESC_PERCENT,
    ESC_OSC,
    ESC_OSC_ESC,
    ESC_CSI_BIGGERTHAN,
    ESC_P,
    ESC_CSI_QUESTIONMARK_ARG_DOLLAR,
    ESC_CSI_ARGS_SPACE,
    ESC_CSI_ARGS_ASTERIX,
    ESC_CSI_DOUBLE_QUOTE,
    ESC_CSI_SINGLE_QUOTE,
    ESC_CSI_EXCLAMATION,
    ESC_APC,
    ESC_APC_ESCAPE
};
enum {
    APPLICATION_CURSOR_KEYS = 1 << 0,
    REVERSE_VIDEO = 1 << 1,
    ORIGIN_MODE = 1 << 2,
    AUTOWRAP = 1 << 3,
    CURSOR_ENABLED = 1 << 4,
    APPLICATION_KEYPAD = 1 << 5,
    LEFTRIGHT_MARGIN_MODE = 1 << 6,
    MOUSE_TRACKING_PRESS_RELEASE = 1 << 7,
    MOUSE_TRACKING_BUTTON_EVENT = 1 << 8,
    SEND_FOCUS_EVENTS = 1 << 9,
    MOUSE_PROTOCOL_SGR = 1 << 10,
    BRACKETED_PASTE_MODE = 1 << 11,
    RECTANGULAR_CHANGE_ATTRIBUTE = 1 << 12,
};
enum {
    G0 = 1 << 13, G1 = 1 << 14, USE_G0 = 1 << 15,
};

typedef struct {
    glyph_style style;
    i8 x, y;
    uint16_t decsetBit;
} term_cursor;

struct terminal_emulator {
    int fd;
    pid_t pid;
    color colors[NUM_INDEXED_COLORS];
    term_cursor cursor, saved_state, saved_state_alt;
    T_buff *screen_buff;
    T_buff main, alt;
    ui8 *tabStop;
    int last_codepoint;
    i8 row, column;
    i8 leftMargin, rightMargin, topMargin, bottomMargin;
    char osc_args[MAX_OSC_STRING_LENGTH + 4];
    int16_t osc_len;
    short csi_args[MAX_ESCAPE_PARAMETERS];
    ui8 esc_state; //MSB is do_not_continue_sequence and 5 lsb is state
    i8 csi_index;
    u_short aShort; //1st msb is abt to autowrap 2nd msb is mode_insert 13 lsb are saved decset
    ui8 cursor_shape;
};

#define TAB_STOP emulator->tabStop

#define COLORS emulator->colors

#define CURSOR emulator->cursor
#define CURSOR_COL CURSOR.x
#define CURSOR_ROW CURSOR.y
#define CURSOR_STYLE CURSOR.style
#define CURSOR_SHAPE emulator->cursor_shape

#define DECSET_BIT emulator->cursor.decsetBit
#define TERM_MODE DECSET_BIT

#define SAVED_STATE emulator->saved_state
#define SAVED_STATE_ALT emulator->saved_state_alt

#define ESC_STATE (emulator->esc_state&0x7F)
#define set_esc_state(state) (emulator->esc_state=((emulator->esc_state&1<<7)|state))
#define CSI_ARG emulator->csi_args
#define CSI_INDEX emulator->csi_index
#define DO_NOT_CONTINUE_SEQUENCE get(emulator->esc_state,1<<7)
#define SET_DO_NOT_CONTINUE_SEQUENCE set(emulator->esc_state,1<<7)

#define MAIN_BUFF emulator->main
#define ALT_BUFF emulator->alt
#define SCREEN_BUFF emulator->screen_buff

#define FD emulator->fd
#define PID emulator->pid

#define LAST_CODEPOINT emulator->last_codepoint

#define LEFT_MARGIN emulator->leftMargin
#define RIGHT_MARGIN emulator->rightMargin
#define TOP_MARGIN emulator->topMargin
#define BOTTOM_MARGIN emulator->bottomMargin

#define OSC_ARG emulator->osc_args
#define OSC_LEN emulator->osc_len

#define ABOUT_TO_AUTOWRAP get(emulator->aShort,1<<15)
#define SET_ABOUT_TO_AUTOWRAP_VAL(val) set_val(emulator->aShort,1<<15,val)
#define CLEAR_ABOUT_TO_AUTOWRAP clear(emulator->aShort,1<<15)
#define MODE_INSERT get(emulator->aShort,1<<14)
#define CLEAR_MODE_INSERT clear(emulator->aShort,1<<14)
#define SET_MODE_INSERT_VAL(val) set_val(emulator->aShort,1<<14,val)

#define SAVED_DECSET_BIT emulator->aShort
#define ROWS emulator->row
#define COLUMNS emulator->column

static const glyph_style NORMAL = {.fg={.index=COLOR_INDEX_FOREGROUND}, .bg={.index=COLOR_INDEX_BACKGROUND}};

static int16_t TRANSCRIPT_ROWS = 200;

static struct render_data *renderer;

static void (*copy_base64_to_clipboard_hooked)(char *);

static size_t
(*get_code_from_term_cap)(char **write_onto, const char *string, size_t len, bool cursorApp, bool keypadApp);

//Non dependant
#define is_unassigned_or_surrogate(codepoint) (((codepoint) >= 0xD800 && (codepoint) <= 0xDFFF) || ((codepoint) >= 0xF0000 && (codepoint) <= 0xFFFFD) || ((codepoint) >= 0x100000 && (codepoint) <= 0x10FFFD))
#define decode_utf(buffer, codepoint, remaining_buf_len, byte_len, remaining_utf_len) { \
    byte_len = 1;                                                                                                       \
    codepoint = buffer[write_len];                                                                                      \
                                                                                                                        \
    if(codepoint > ASCII_MAX) {                                                                                         \
        byte_len = ((codepoint & 0b11100000) == 0b11000000) ? 2 : ((codepoint & 0b11110000) == 0b11100000) ? 3 : 4;     \
        if(codepoint > UTF_4_BYTE_MAX) codepoint = UNICODE_REPLACEMENT_CHARACTER;                                       \
        else if(byte_len > remaining_buf_len) {                                                                         \
            remaining_utf_len = ( ui8) (remaining_buf_len);                                                          \
            memcpy(buffer, buffer + write_len, remaining_utf_len);                                                      \
            break;                                                                                                      \
        } else {                                                                                                        \
            codepoint = (codepoint & (0xFF >> byte_len)) << ((byte_len - 1) * 6);                                       \
            for(int i = 1; i < byte_len; i++) codepoint |= (buffer[write_len + i] & 0x3F) << ((byte_len - i - 1) * 6);  \
            if(is_unassigned_or_surrogate(codepoint)) codepoint=UNICODE_REPLACEMENT_CHARACTER;                                                                             \
        }                                                                                                               \
    }                                                                                                                   \
}

static ui8 append_codepoint(const int codepoint, char *const buffer) {

    if (codepoint <= ASCII_MAX) {
        // 1-byte sequence: U+0000 to U+007F
        buffer[0] = (char) codepoint;
        buffer[1] = '\0';
        return 1;
    } else if (codepoint <= UTF_2_BYTE_MAX) {
        // 2-byte sequence: U+0080 to U+07FF
        buffer[0] = 0xC0 | ((codepoint >> 6) & 0x1F);
        buffer[1] = 0x80 | (codepoint & 0x3F);
        buffer[2] = '\0';
        return 2;
    } else if (codepoint <= UTF_3_BYTE_MAX) {
        // 3-byte sequence: U+0800 to U+FFFF
        buffer[0] = 0xE0 | ((codepoint >> 12) & 0x0F);
        buffer[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[2] = 0x80 | (codepoint & 0x3F);
        buffer[3] = '\0';
        return 3;
    } else if (codepoint <= UTF_4_BYTE_MAX) {
        // 4-byte sequence: U+10000 to U+10FFFF
        buffer[0] = 0xF0 | ((codepoint >> 18) & 0x07);
        buffer[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        buffer[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[3] = 0x80 | (codepoint & 0x3F);
        buffer[4] = '\0';
        return 4;
    }
    // Invalid codepoint, return 0
    return 0;
}

static ui8 str_to_hex_int8(const char *restrict const string, bool *err, i8 len) {
    char c;
    ui8 result = 0;
    for (int8_t i = 0; i < len; i++) {
        c = string[i];
        switch (c) {
            case '0'...'9':
                c -= '0';
                break;
            case 'a'...'f':
                c -= ('a' - 10);
                break;
            case 'A'...'F':
                c -= ('A' - 10);
                break;
            default:
                if (err)
                    *err = true;
                return 0;
        }
        result = result * 16 + c;
    }
    return result;
}

static void parse_color_to_index(color *colors, const char *color_string, const i8 len) {
    bool skip_between = 0;
    i8 chars_for_colors, component_length;
    ui8 col[3];
    bool err = false;
    double mult;
    if ('#' == color_string[0]) color_string++;
    else if (strncmp(color_string, "rgb:", 4) == 0) {
        color_string += 4;
        skip_between = 1;
    } else return;
    chars_for_colors = (i8) (len - 2 * skip_between);
    if (0 != chars_for_colors % 3) return;
    component_length = (i8) (chars_for_colors / 3);
    {
        double temp_num = (component_length >> 2) - 1;
        mult = 255 / temp_num * temp_num;
    }
    for (int i = 0; i < 3; i++) {
        col[i] = (ui8) (str_to_hex_int8(color_string, &err, component_length) * mult);
        if (err) return;
        color_string += skip_between + component_length;
    }
    *colors = (color) {col[0], col[1], col[2]};
}

static int16_t map_decset_bit(int16_t bit) {
    switch (bit) {
        case 1:
            return APPLICATION_CURSOR_KEYS;
        case 5:
            return REVERSE_VIDEO;
        case 6:
            return ORIGIN_MODE;
        case 7:
            return AUTOWRAP;
        case 25:
            return CURSOR_ENABLED;
        case 66:
            return APPLICATION_KEYPAD;
        case 69:
            return LEFTRIGHT_MARGIN_MODE;
        case 1000:
            return MOUSE_TRACKING_PRESS_RELEASE;
        case 1002:
            return MOUSE_TRACKING_BUTTON_EVENT;
        case 1004:
            return SEND_FOCUS_EVENTS;
        case 1006:
            return MOUSE_PROTOCOL_SGR;
        case 2004:
            return BRACKETED_PASTE_MODE;
        default:
            return -1;
    }
}


#define internal_row_buff(buffer, ex_row) (buffer->history_rows+ex_row)
#define internal_row(ex_row) internal_row_buff(buffer,ex_row)

#define is_alt_buff_active() (SCREEN_BUFF==&ALT_BUFF)
#define finish_sequence() set_esc_state(ESC_NONE)
#define continue_sequence(new_esc_state) (emulator->esc_state = new_esc_state)
#define reset_color(index) COLORS[index] = DEFAULT_COLORSCHEME[index];
#define reset_all_color() memcpy(COLORS, DEFAULT_COLORSCHEME, sizeof(int_least32_t) * NUM_INDEXED_COLORS);
#define set_char(codepoint, x, y)  SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF,y)][x] = (Glyph) {.code=codepoint, .style= CURSOR_STYLE}
#define restore_cursor() emulator->cursor = (is_alt_buff_active()) ? SAVED_STATE_ALT :SAVED_STATE;
#define block_clear(sx, sy, w, h) block_set(SCREEN_BUFF, sx, sy, w, h, (Glyph) { CURSOR_STYLE,' '})
#define clear_transcript() block_set(&MAIN_BUFF, 0, 0, COLUMNS, TRANSCRIPT_ROWS, (Glyph) {.code=' ',.style= NORMAL});MAIN_BUFF.history_rows=0;

static void
block_copy(T_buff *restrict const buffer, const i8 sx, const i8 sy, const i8 w, const i8 h, const i8 dx, const i8 dy) {
    if (w) {
        i8 y2;
        Glyph *restrict src, *restrict dst;
        const bool copyingUp = sy > dy;
        for (i8 y = 0; y < h; y++) {
            y2 = (i8) (copyingUp ? y : h - 1 - y);
            src = buffer->lines[internal_row(dy + y2)] + dx;
            dst = buffer->lines[internal_row(sy + y2)] + sx;
            for (i8 x = 0; x < w; x++) src[x] = dst[x];
        }
    }
}

static void
block_set(T_buff *restrict const buffer, const i8 sx, const i8 sy, const i8 w, const int16_t h, const Glyph glyph) {
    Glyph *restrict ptr;
    for (int16_t y = 0; y < h; y++) {
        ptr = buffer->lines[internal_row(sy)] + sx;
        for (int8_t x = 0; x < w; x++) ptr[x] = glyph;
    }
}

static void reset_emulator(terminal *restrict const emulator) {
    CSI_INDEX = 0;
    finish_sequence();
    SET_DO_NOT_CONTINUE_SEQUENCE;
    set(TERM_MODE, USE_G0);
    clear(TERM_MODE, G0 | G1);
    CLEAR_MODE_INSERT;
    TOP_MARGIN = LEFT_MARGIN = 0;
    BOTTOM_MARGIN = ROWS;
    RIGHT_MARGIN = COLUMNS;
    CURSOR_STYLE.fg.index = COLOR_INDEX_FOREGROUND;
    CURSOR_STYLE.bg.index = COLOR_INDEX_BACKGROUND;
    {
        i8 temp = CELI(COLUMNS, 8);
        for (i8 i = 0; i < temp; i++) TAB_STOP[i] = 1;//Setting default tab stop
    }
    set(DECSET_BIT, AUTOWRAP | CURSOR_ENABLED);
    SAVED_STATE = SAVED_STATE_ALT = (term_cursor) {.style=NORMAL, .decsetBit= emulator->cursor.decsetBit};
    reset_all_color()
}

static void save_cursor(terminal *restrict const emulator) {
    if (is_alt_buff_active())
        SAVED_STATE_ALT = emulator->cursor;
    else
        SAVED_STATE = emulator->cursor;
}

static void do_osc_set_text_parameters(terminal *restrict const emulator, const char *const terminator) {
    i8 value = 0;
    char *text_param = NULL;
    int16_t len = 0;
    bool end_of_input;

    for (uint16_t osc_index = 0; osc_index < OSC_LEN; osc_index++) {
        const char b = OSC_ARG[osc_index];
        switch (b) {
            case ';':
                text_param = OSC_ARG + osc_index + 1;
                len = (int16_t) (OSC_LEN - (osc_index + 1));
                goto exit_loop;
            case '0'...'9':
                value = (i8) (value * 10 + (b - '0'));
                break;
            default:
                finish_sequence();
                return;
        }
    }
    exit_loop:;

    switch (value) {
        case 0:
        case 1:
        case 2:
        case 119:
            break;
        case 4: {
            int16_t color_index = -1;
            int16_t parsing_pair_start = -1;
            char b;
            for (int16_t i = 0;; i++) {
                end_of_input = i == len;
                b = text_param[i];
                if (end_of_input || ';' == b) {
                    if (0 > parsing_pair_start) parsing_pair_start = (int16_t) (i + 1);
                    else if (BETWEEN(color_index, 0, 255)) {
                        parse_color_to_index(emulator->colors + color_index, text_param + parsing_pair_start,
                                             (i8) (i - parsing_pair_start));
                        color_index = parsing_pair_start = -1;
                    } else {
                        finish_sequence();
                        return;
                    }

                } else if (0 > parsing_pair_start && BETWEEN(b, '0', '9'))
                    color_index = (int16_t) ((0 < color_index ? 0 : color_index * 10) + (b - '0'));
                else {
                    finish_sequence();
                    return;
                }
                if (end_of_input)break;
            }
        }
            break;
        case 10:
        case 11:
        case 12: {
            ui8 special_index = (ui8) (COLOR_INDEX_FOREGROUND + value - 10);
            int16_t last_semi_index = 0;
            for (int16_t char_index = 0;; char_index++) {
                end_of_input = char_index == len;
                if (end_of_input || ';' == text_param[char_index]) {
                    char *color_spec = text_param + last_semi_index;
                    if ('?' == *color_spec) {
                        const color rgb = emulator->colors[special_index];
                        dprintf(FD, "\033]%d;rgb:%04x/%04x/%04x%s", value, rgb.r, rgb.g, rgb.b, terminator);
                    } else
                        parse_color_to_index(emulator->colors + special_index, color_spec,
                                             ((i8) (char_index - last_semi_index)));
                    special_index++;
                    if (end_of_input || COLOR_INDEX_CURSOR < special_index || ++char_index >= len) break;
                    last_semi_index = char_index;
                }
            }
        }
            break;
        case 52:
            if (copy_base64_to_clipboard_hooked)copy_base64_to_clipboard_hooked(strchr(text_param, ';'));
            break;

        case 104:
            if (len == 0) reset_all_color()
            else {
                int16_t last_index = 0;
                for (int16_t char_index = 0;; char_index++) {
                    end_of_input = char_index == len;
                    if (end_of_input || ';' == text_param[char_index]) {
                        int16_t char_len = (int16_t) (char_index - last_index);
                        ui8 color_to_reset = 0;
                        char *color_string = text_param + last_index, b;
                        while (--char_len >= 0) {
                            b = *color_string++;
                            if (BETWEEN(b, '0', '9'))
                                color_to_reset = color_to_reset * 10 + (b - '0');
                            else
                                goto try;

                        }
                        reset_color(color_to_reset)
                        if (end_of_input) break;
                        last_index = ++char_index;
                        try:;
                    }
                }
            }
            break;
        case 110:
        case 111:
        case 112:
            reset_color(COLOR_INDEX_FOREGROUND + value - 110)
            break;
    }
    finish_sequence();
}

static void scroll_down(terminal *restrict const emulator, const int16_t n) {
    if (0 != LEFT_MARGIN || RIGHT_MARGIN != COLUMNS) {
        block_copy(SCREEN_BUFF, LEFT_MARGIN, TOP_MARGIN + 1, RIGHT_MARGIN - LEFT_MARGIN, BOTTOM_MARGIN - TOP_MARGIN - 1,
                   LEFT_MARGIN, TOP_MARGIN);
        block_set(SCREEN_BUFF, LEFT_MARGIN, BOTTOM_MARGIN - 1, RIGHT_MARGIN - LEFT_MARGIN, 1,
                  (Glyph) {.code=' ', .style= CURSOR_STYLE});
    } else {
        int16_t total_row = is_alt_buff_active() ? ROWS : TRANSCRIPT_ROWS;
        Term_row temp_rows[n], temp;
        Glyph fill = {.code=' ', .style=CURSOR_STYLE};
        memcpy(temp_rows, SCREEN_BUFF->lines, (size_t) n * sizeof(Term_row));
        memcpy(SCREEN_BUFF->lines, SCREEN_BUFF->lines + n, ((size_t) (total_row - n)) * sizeof(Term_row));
        for (int16_t y = 0; y < n; y++) {
            temp = temp_rows[y];
            SCREEN_BUFF->lines[total_row - 1 - y] = temp;
            for (i8 x = 0; x < COLUMNS; x++) temp[x] = fill;
        }
        if (SCREEN_BUFF->history_rows + n - 1 < total_row - ROWS) SCREEN_BUFF->history_rows += n;
    }
}

static void do_line_feed(terminal *restrict const emulator) {
    i8 new_cursor_row = CURSOR_ROW + 1;
    if (CURSOR_ROW >= BOTTOM_MARGIN) {
        if (CURSOR_ROW != ROWS - 1) {
            CURSOR_ROW = new_cursor_row;
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
    } else {
        if (new_cursor_row == BOTTOM_MARGIN) {
            scroll_down(emulator, 1);
            new_cursor_row = BOTTOM_MARGIN - 1;
        }
        CURSOR_ROW = new_cursor_row;
        CLEAR_ABOUT_TO_AUTOWRAP;
    }
}

static void emit_codepoint(terminal *restrict const emulator, int codepoint) {
    static const int table[] = {L' ',    // Blank '_'
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

    const i8 display_width = (int8_t) wcwidth((wchar_t) codepoint);
    const bool cursor_in_last_column = CURSOR_COL == RIGHT_MARGIN - 1;

    LAST_CODEPOINT = codepoint;
    if (get(TERM_MODE, USE_G0) ? get(TERM_MODE, G0) : get(TERM_MODE, G1)) {
        switch (codepoint) {
            case '0':
                codepoint = L'█';    // Solid block '0'
                break;
            case '_' ...'~':
                codepoint = table[codepoint - '_'];
        }
    }

    if (get(DECSET_BIT, AUTOWRAP)) {
        if (cursor_in_last_column && ((ABOUT_TO_AUTOWRAP && 1 == display_width) || 2 == display_width)) {
            set(SCREEN_BUFF->lines[CURSOR_ROW][CURSOR_COL].style.effect, AUTO_WRAPPED);
            CURSOR_COL = LEFT_MARGIN;
            if (CURSOR_ROW + 1 < BOTTOM_MARGIN)CURSOR_ROW++;
            else scroll_down(emulator, 1);
        }
    } else if (cursor_in_last_column && display_width == 2) return;
    if (MODE_INSERT && 0 < display_width) {
        const i8 dest_col = CURSOR_COL + display_width;
        if (dest_col < RIGHT_MARGIN)
            block_copy(SCREEN_BUFF, CURSOR_COL, CURSOR_ROW, RIGHT_MARGIN - dest_col, 1, dest_col, CURSOR_ROW);
    }

    set_char(codepoint, CURSOR_COL - (0 >= display_width && ABOUT_TO_AUTOWRAP), CURSOR_ROW);

    if (get(DECSET_BIT, AUTOWRAP) && 0 < display_width)
        SET_ABOUT_TO_AUTOWRAP_VAL(CURSOR_COL == (RIGHT_MARGIN - display_width));

    CURSOR_COL = MIN(CURSOR_COL + display_width, RIGHT_MARGIN - 1);
}

static void set_cursor_position_origin_mode(terminal *restrict const emulator, const i8 x, const i8 y) {
    bool origin_mode = get(DECSET_BIT, ORIGIN_MODE);
    const i8 effective_top_margin = (i8) (origin_mode ? TOP_MARGIN : 0);
    const i8 effective_bottom_margin = (i8) ((origin_mode ? BOTTOM_MARGIN : ROWS) - 1);
    const i8 effective_left_margin = (i8) (origin_mode ? LEFT_MARGIN : 0);
    const i8 effective_right_margin = (i8) ((origin_mode ? RIGHT_MARGIN : COLUMNS) - 1);
    const i8 temp_col = (i8) (effective_left_margin + x), temp_row = (i8) (effective_top_margin + y);
    CURSOR_COL = LIMIT(temp_col, effective_left_margin, effective_right_margin);
    CURSOR_ROW = LIMIT(temp_row, effective_top_margin, effective_bottom_margin);
    CLEAR_ABOUT_TO_AUTOWRAP;
}

static void set_cursor_row_col(terminal *restrict const emulator, const i8 x, const i8 y) {
    CURSOR_ROW = LIMIT(y, 0, ROWS - 1);
    CURSOR_COL = LIMIT(x, 0, COLUMNS - 1);
    CLEAR_ABOUT_TO_AUTOWRAP;
}

static int16_t
getArg(terminal *restrict const emulator, const i8 index, const i8 default_value, const bool treat_zero_as_default) {
    int16_t result = CSI_ARG[index];
    if (0 > result || (0 == result && treat_zero_as_default))
        result = (int16_t) default_value;
    return result;
}

static inline i8 next_tab_stop(terminal *restrict const emulator, i8 num_tabs) {
    for (i8 i = CURSOR_COL + 1; i < COLUMNS; i++)
        if (TAB_STOP[i / 8] & (1 << (i & 7)) && (0 == --num_tabs))
            return MIN(i, RIGHT_MARGIN);

    return RIGHT_MARGIN - 1;
}

static void parse_arg(terminal *restrict const emulator, const char input_byte) {
    switch (input_byte) {
        case '0'...'9':
            if (CSI_INDEX < MAX_ESCAPE_PARAMETERS) {
                int16_t old_value = CSI_ARG[CSI_INDEX];
                const i8 this_digit = (i8) (input_byte - '0');
                old_value = (int16_t) ((0 < old_value) ? (old_value * 10) + this_digit : this_digit);
                if (old_value > 9999)old_value = 9999;
                CSI_ARG[CSI_INDEX] = old_value;
            }
            continue_sequence(ESC_STATE);
            break;
        case ';':
            if (CSI_INDEX < MAX_ESCAPE_PARAMETERS) CSI_INDEX++;
            continue_sequence(ESC_STATE);
            break;
        default:
            finish_sequence();
    }
}

static void do_dec_set_or_reset(terminal *restrict const emulator, const bool setting, const int16_t external_bit) {

    {
        int16_t internal = map_decset_bit(external_bit);
        switch (internal) {
            case -1:
                break;
            case MOUSE_TRACKING_PRESS_RELEASE:
            case MOUSE_TRACKING_BUTTON_EVENT:
                if (setting)
                    clear(DECSET_BIT, MOUSE_TRACKING_PRESS_RELEASE | MOUSE_TRACKING_BUTTON_EVENT);
                        __attribute__((fallthrough));
            default:
                set_val(DECSET_BIT, internal, setting);
        }
    }
    switch (external_bit) {
        case 1 :
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 12:
        case 25:
        case 40:
        case 45:
        case 66:
        case 1000:
        case 1001:
        case 1002:
        case 1003:
        case 1004:
        case 1005:
        case 1006:
        case 1015:
        case 1034:
        case 2004:
            break;
        case 3: {
            LEFT_MARGIN = TOP_MARGIN = 0;
            BOTTOM_MARGIN = ROWS;
            RIGHT_MARGIN = COLUMNS;
            clear(DECSET_BIT, LEFTRIGHT_MARGIN_MODE);
            block_clear(0, 0, COLUMNS, ROWS);
            CURSOR_ROW = CURSOR_COL = 0;
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 6 :
            if (setting)
                set_cursor_position_origin_mode(emulator, 0, 0);
            break;
        case 69 :
            if (!setting) {
                LEFT_MARGIN = 0;
                RIGHT_MARGIN = COLUMNS;
            }
            break;
        case 1048:
            if (setting) save_cursor(emulator);
            else
                restore_cursor()
            break;
        case 47:
        case 1047:
        case 1049: {
            T_buff *new_screen = setting ? &ALT_BUFF : &MAIN_BUFF;
            if (new_screen != SCREEN_BUFF) {
                if (setting) save_cursor(emulator);
                else
                    restore_cursor()
                if (new_screen == &ALT_BUFF)
                    block_set(new_screen, 0, 0, COLUMNS, ROWS, (Glyph) {.code=' ', .style= CURSOR_STYLE});
            }
        }
            break;
        default:
            finish_sequence();
    }
}

static void inline select_graphic_radiation(terminal *restrict const emulator) {
    if (CSI_INDEX >= MAX_ESCAPE_PARAMETERS)
        CSI_INDEX = MAX_ESCAPE_PARAMETERS - 1;
    for (int i = 0; i < CSI_INDEX; i++) {
        int16_t code = CSI_ARG[i];
        if (0 > code) {
            if (0 < CSI_INDEX)continue;
            else code = 0;
        }
        switch (code) {
            case 0:
                CURSOR_STYLE = NORMAL;
                break;
            case 1 ...9:
                set(CURSOR_STYLE.effect, 1 << (code - (1 + (code > 6))));
                break;
            case 22:
                clear(CURSOR_STYLE.effect, BOLD | DIM);
                break;
            case 23 ...29:
                clear(CURSOR_STYLE.effect, 1 << (code - (21 + (code > 26))));
                break;
            case 30 ... 37:
                CURSOR_STYLE.fg.index = (ui8) (code - 30);
                break;
            case 38:
            case 48: {
                const i8 first_arg = (i8) CSI_ARG[i + 1];
                if (i + 2 > CSI_INDEX) continue;
                switch (first_arg) {
                    case 2:
                        if (i + 4 <= CSI_INDEX) {
                            const color c = {.r=(ui8) CSI_ARG[i + 2], .g=(ui8) CSI_ARG[i + 3], .b=(ui8) CSI_ARG[i + 4]};
                            if (code == 38) {
                                CURSOR_STYLE.fg.color = c;
                                set(CURSOR_STYLE.effect, TRUE_COLOR_FG);
                            } else {
                                CURSOR_STYLE.bg.color = c;
                                set(CURSOR_STYLE.effect, TRUE_COLOR_BG);
                            }

                            i += 4;
                        }
                        break;
                    case 5: {
                        int16_t color = CSI_ARG[i + 2];
                        i += 2;
                        if (BETWEEN(color, 0, NUM_INDEXED_COLORS)) {
                            (code == 38) ? (CURSOR_STYLE.fg.index = (ui8) color)
                                         : (CURSOR_STYLE.bg.index = (ui8) color);
                        }
                    }
                        break;
                    default:
                        finish_sequence();
                }
            }
                break;
            case 39:
                CURSOR_STYLE.fg.index = COLOR_INDEX_FOREGROUND;
                break;
            case 40 ... 47:
                CURSOR_STYLE.bg.index = (ui8) (code - 40);
                break;
            case 49:
                CURSOR_STYLE.bg.index = COLOR_INDEX_BACKGROUND;
                break;
            case 90 ... 97:
                CURSOR_STYLE.fg.index = (ui8) (code - 90 + 8);
                break;
            case 100 ... 107:
                CURSOR_STYLE.bg.index = (ui8) (code - 100 + 8);
        }
    }
}

static inline void do_esc(terminal *restrict const emulator, const int codepoint) {
    switch (codepoint) {
        case '#':
            continue_sequence(ESC_POUND);
            break;
        case '(':
            continue_sequence(ESC_SELECT_LEFT_PAREN);
            break;
        case ')':
            continue_sequence(ESC_SELECT_RIGHT_PAREN);
            break;
        case '6':
            if (CURSOR_COL > LEFT_MARGIN)CURSOR_COL--;
            else {
                const i8 rows = BOTTOM_MARGIN - TOP_MARGIN;
                block_copy(SCREEN_BUFF, LEFT_MARGIN, TOP_MARGIN, RIGHT_MARGIN - LEFT_MARGIN - 1, rows, LEFT_MARGIN + 1,
                           TOP_MARGIN);
                block_set(SCREEN_BUFF, RIGHT_MARGIN - 1, TOP_MARGIN, 1, rows,
                          (Glyph) {.code=' ', .style={CURSOR_STYLE.fg, CURSOR_STYLE.bg, 0}});
            }
            break;
        case '7':
            save_cursor(emulator);
            break;
        case '8':
            restore_cursor()
            break;
        case '9':
            if (CURSOR_COL < RIGHT_MARGIN - 1)CURSOR_COL++;
            else {
                const i8 rows = BOTTOM_MARGIN - TOP_MARGIN;
                block_copy(SCREEN_BUFF, LEFT_MARGIN + 1, TOP_MARGIN, RIGHT_MARGIN - LEFT_MARGIN - 1, rows, LEFT_MARGIN,
                           TOP_MARGIN);
                block_set(SCREEN_BUFF, RIGHT_MARGIN - 1, TOP_MARGIN, 1, rows,
                          (Glyph) {.code=' ', .style= {CURSOR_STYLE.fg, CURSOR_STYLE.bg, 0}});
            }
            break;
        case 'c':
            reset_emulator(emulator);
            clear_transcript()
            block_clear(0, 0, COLUMNS, ROWS);
            set_cursor_position_origin_mode(emulator, 0, 0);
            break;
        case 'D':
            do_line_feed(emulator);
            break;
        case 'E':
            CURSOR_COL = get(DECSET_BIT, ORIGIN_MODE) ? LEFT_MARGIN : 0;
            CLEAR_ABOUT_TO_AUTOWRAP;
            do_line_feed(emulator);
            break;
        case 'F':
            set_cursor_row_col(emulator, 0, BOTTOM_MARGIN - 1);
            break;
        case 'H':
            TAB_STOP[CURSOR_COL / 8] |= true << (CURSOR_COL & 7);
            break;
        case 'M':
            if (CURSOR_ROW <= TOP_MARGIN) {
                block_copy(SCREEN_BUFF, 0, TOP_MARGIN, COLUMNS, BOTTOM_MARGIN - TOP_MARGIN - 1, 0, TOP_MARGIN + 1);
                block_clear(0, TOP_MARGIN, COLUMNS, 1);
            } else
                CURSOR_ROW--;
            break;
        case 'N':
        case '0':
            break;
        case 'P':
            OSC_LEN = 0;
            continue_sequence(ESC_P);
            break;
        case '[':
            continue_sequence(ESC_CSI);
            break;
        case '=':
            set(DECSET_BIT, APPLICATION_KEYPAD);
            break;
        case ']':
            OSC_LEN = 0;
            continue_sequence(ESC_OSC);
            break;
        case '>':
            clear(DECSET_BIT, APPLICATION_KEYPAD);
            break;
        default:
            finish_sequence();
    }
}

static inline void do_esc_csi(terminal *restrict const emulator, const int codepoint) {
    switch (codepoint) {
        case '!':
            continue_sequence(ESC_CSI_EXCLAMATION);
            break;
        case '"':
            continue_sequence(ESC_CSI_DOUBLE_QUOTE);
            break;
        case '\'':
            continue_sequence(ESC_CSI_SINGLE_QUOTE);
            break;
        case '$':
            continue_sequence(ESC_CSI_DOLLAR);
            break;
        case '*':
            continue_sequence(ESC_CSI_ARGS_ASTERIX);
            break;
        case '@': {
            const i8 columns_after_cursor = (i8) (COLUMNS - CURSOR_COL);
            const i8 arg = (i8) getArg(emulator, 0, 1, true);
            const i8 spaces_to_insert = MIN(arg, columns_after_cursor);
            const i8 chars_to_move = (i8) (columns_after_cursor - spaces_to_insert);
            CLEAR_ABOUT_TO_AUTOWRAP;
            block_copy(SCREEN_BUFF, CURSOR_COL, CURSOR_ROW, chars_to_move, 1, CURSOR_COL + spaces_to_insert,
                       CURSOR_ROW);
            block_clear(CURSOR_COL, CURSOR_ROW, spaces_to_insert, 1);
        }
            break;
        case 'A': {
            const i8 row = (i8) (CURSOR_ROW - getArg(emulator, 0, 1, 1));
            CURSOR_ROW = MAX(0, row);
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'B': {
            const i8 row = (i8) (CURSOR_ROW + getArg(emulator, 0, 1, 1));
            CURSOR_ROW = MIN(ROWS - 1, row);
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'C':
        case 'a': {
            const i8 col = (i8) (CURSOR_COL + getArg(emulator, 0, 1, 1));
            CURSOR_COL = MIN(RIGHT_MARGIN - 1, col);
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'D': {
            const i8 col = (i8) (CURSOR_COL - getArg(emulator, 0, 1, 1));
            CURSOR_COL = MAX(LEFT_MARGIN, col);
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'E':
            set_cursor_position_origin_mode(emulator, 0, (i8) (CURSOR_ROW + getArg(emulator, 0, 1, 1)));
            break;
        case 'F':
            set_cursor_position_origin_mode(emulator, 0, (i8) (CURSOR_ROW - getArg(emulator, 0, 1, 1)));
            break;
        case 'G': {
            const i8 arg = (i8) CSI_ARG[0];
            CURSOR_COL = LIMIT(arg, 1, COLUMNS);
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'H':
        case 'f':
            set_cursor_position_origin_mode(emulator, (i8) (getArg(emulator, 1, 1, 1) - 1),
                                            (i8) getArg(emulator, 0, 1, 1));
            break;
        case 'I':
            CURSOR_COL = next_tab_stop(emulator, (i8) getArg(emulator, 0, 1, 1));
            CLEAR_ABOUT_TO_AUTOWRAP;
            break;
        case 'J': {
            const i8 arg = (i8) getArg(emulator, 0, 0, 1);
            switch (arg) {
                case 0:
                    block_clear(CURSOR_COL, CURSOR_ROW, COLUMNS - CURSOR_COL, 1);
                    block_clear(0, CURSOR_ROW + 1, COLUMNS, ROWS - CURSOR_ROW - 1);
                    break;
                case 1:
                    block_clear(0, 0, COLUMNS, CURSOR_ROW);
                    block_clear(0, CURSOR_ROW, CURSOR_COL + 1, 1);
                    break;
                case 2:
                    block_clear(0, 0, COLUMNS, ROWS);
                    break;
                case 3:
                clear_transcript()
                    break;
                default:
                    finish_sequence();
                    return;

            }
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'K': {
            const i8 arg = (i8) getArg(emulator, 0, 0, 1);
            switch (arg) {
                case 0:
                    block_clear(CURSOR_COL, CURSOR_ROW, COLUMNS - CURSOR_COL, 1);
                    break;
                case 1:
                    block_clear(0, CURSOR_ROW, CURSOR_COL + 1, 1);
                    break;
                case 2:
                    block_clear(0, CURSOR_ROW, COLUMNS, 1);
                    break;
                default:
                    finish_sequence();
                    return;
            }
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'L': {
            const i8 arg = (i8) getArg(emulator, 0, 1, 1);
            const i8 lines_after_cursor = BOTTOM_MARGIN - CURSOR_ROW;
            const i8 lines_to_insert = MIN(arg, lines_after_cursor);
            const i8 lines_to_move = (i8) (lines_after_cursor - lines_to_insert);
            block_copy(SCREEN_BUFF, 0, CURSOR_ROW, COLUMNS, lines_to_move, 0, CURSOR_ROW + lines_to_insert);
            block_clear(0, CURSOR_ROW + lines_to_move, COLUMNS, lines_to_insert);
        }
            break;
        case 'M': {
            const i8 arg = (i8) getArg(emulator, 0, 1, 1);
            const i8 lines_after_cursor = BOTTOM_MARGIN - CURSOR_ROW;
            const i8 lines_to_delete = MIN(arg, lines_after_cursor);
            const i8 lines_to_move = (i8) (lines_after_cursor - lines_to_delete);
            CLEAR_ABOUT_TO_AUTOWRAP;
            block_copy(SCREEN_BUFF, 0, CURSOR_ROW + lines_to_delete, COLUMNS, lines_to_move, 0, CURSOR_ROW);
            block_clear(0, CURSOR_ROW + lines_to_move, COLUMNS, lines_to_delete);
        }
            break;
        case 'P': {
            const i8 arg = (i8) getArg(emulator, 0, 1, 1);
            const i8 cells_after_cursor = (i8) (COLUMNS - CURSOR_COL);
            const i8 cells_to_delete = MIN(arg, cells_after_cursor);
            const i8 cells_to_move = (i8) (cells_after_cursor - cells_to_delete);
            CLEAR_ABOUT_TO_AUTOWRAP;
            block_copy(SCREEN_BUFF, CURSOR_COL + cells_to_delete, CURSOR_ROW, cells_to_move, 1, CURSOR_COL, CURSOR_ROW);
            block_clear(CURSOR_COL + cells_to_move, CURSOR_ROW, cells_to_delete, 1);
        }
            break;
        case 'S':
            scroll_down(emulator, getArg(emulator, 0, 1, 1));
            break;
        case 'T':
            if (0 == CSI_INDEX) {
                const i8 lines_to_scroll_arg = (i8) getArg(emulator, 0, 1, 1);
                const i8 lines_between_top_bottom_margin = BOTTOM_MARGIN - TOP_MARGIN;
                const i8 lines_to_scroll = MIN(lines_between_top_bottom_margin, lines_to_scroll_arg);
                block_copy(SCREEN_BUFF, 0, TOP_MARGIN, COLUMNS,
                           (i8) (lines_between_top_bottom_margin - lines_to_scroll), 0, TOP_MARGIN + lines_to_scroll);
                block_clear(0, TOP_MARGIN, COLUMNS, lines_to_scroll);
            } else { finish_sequence(); }
            break;
        case 'X': {
            const i8 arg = (i8) getArg(emulator, 0, 1, 1);
            CLEAR_ABOUT_TO_AUTOWRAP;
            block_set(SCREEN_BUFF, CURSOR_COL, CURSOR_ROW, MIN(arg, COLUMNS - CURSOR_COL), 1,
                      (Glyph) {.code=' ', .style= CURSOR_STYLE});
        }
            break;
        case 'Z': {
            i8 number_of_tabs = (i8) getArg(emulator, 0, 1, 1);
            i8 new_col = LEFT_MARGIN;
            for (int i = CURSOR_COL - 1; 0 <= i; i--) {
                if (0 == --number_of_tabs) {
                    new_col = (i8) (MAX(i, new_col));
                }
            }
            CURSOR_COL = new_col;
        }
            break;
        case '?':
            continue_sequence(ESC_CSI_QUESTIONMARK);
            break;
        case '>':
            continue_sequence(ESC_CSI_BIGGERTHAN);
            break;
        case '`':
            set_cursor_position_origin_mode(emulator, (i8) (getArg(emulator, 0, 1, 1) - 1), CURSOR_ROW);
            break;
        case 'b':
            if (-1 != LAST_CODEPOINT) {
                i8 num_repeat = (i8) getArg(emulator, 0, 1, 1);
                while (0 < num_repeat--)
                    emit_codepoint(emulator, LAST_CODEPOINT);
            }

            break;
        case 'c':
            if (0 == getArg(emulator, 0, 0, 1))
                write(FD, "\033[?64;1;2;6;9;15;18;21;22c", 26);
            break;
        case 'd': {
            const i8 arg = (i8) getArg(emulator, 0, 1, 1);
            CURSOR_ROW = LIMIT(arg, 1, ROWS) - 1;
            CLEAR_ABOUT_TO_AUTOWRAP;
        }
            break;
        case 'e':
            set_cursor_position_origin_mode(emulator, CURSOR_COL, (i8) (CURSOR_ROW + getArg(emulator, 0, 1, 1)));
            break;
        case 'g': {
            const i8 arg = (i8) getArg(emulator, 0, 1, 1);
            switch (arg) {
                case 0:
                    TAB_STOP[CURSOR_COL / 8] &= ~(1 << (CURSOR_COL & 7));
                    break;
                case 3:
                    memset(TAB_STOP, 0, (size_t) CELI(COLUMNS, 8));
                    break;
            }
        }
            break;
        case 'h':
        case 'l': { //do_set_mode
            const i8 arg = (i8) getArg(emulator, 0, 0, 1);
            switch (arg) {
                case 4:
                    SET_MODE_INSERT_VAL(codepoint == 'h');
                    break;
                case 34:
                    break;
                default:
                    finish_sequence();
            }
        }
            break;
        case 'm':
            select_graphic_radiation(emulator);
            break;
        case 'n': {
            const i8 arg = (i8) CSI_ARG[0];
            switch (arg) {
                case 5: {
                    char bytes[] = {27, '[', '0', 'n'};
                    write(FD, bytes, 4);
                }
                    break;
                case 6:
                    dprintf(FD, "\033[%d;%dR", CURSOR_ROW + 1, CURSOR_COL + 1);
            }
        }
            break;
        case 'r': {
            const i8 arg0 = (i8) (CSI_ARG[0] - 1), arg1 = (i8) CSI_ARG[1];
            TOP_MARGIN = LIMIT(arg0, 0, ROWS - 2);
            BOTTOM_MARGIN = LIMIT(arg1, TOP_MARGIN + 2, ROWS);
            set_cursor_position_origin_mode(emulator, 0, 0);
        }
            break;
        case 's':
            if (get(DECSET_BIT, LEFTRIGHT_MARGIN_MODE)) {
                const i8 arg0 = (i8) (CSI_ARG[0] - 1), arg1 = (i8) CSI_ARG[1];
                LEFT_MARGIN = LIMIT(arg0, 0, COLUMNS - 2);
                RIGHT_MARGIN = LIMIT(arg1, LEFT_MARGIN + 1, COLUMNS);
                set_cursor_position_origin_mode(emulator, 0, 0);
            } else save_cursor(emulator);
            break;
        case 't': {
            const i8 arg = (i8) CSI_ARG[0];
            switch (arg) {
                case 11:
                    write(FD, "\033[1t", 4);
                    break;
                case 13:
                    write(FD, "\033[3;0;0t", 8);
                    break;
                case 14:
                    dprintf(FD, "\033[4;%d;%dt", ROWS * 12, COLUMNS * 12);
                    break;
                case 18:
                    dprintf(FD, "\033[8;%d;%dt", ROWS, COLUMNS);
                    break;
                case 19:
                    dprintf(FD, "\033[9;%d;%dt", ROWS, COLUMNS);
                    break;
                case 20:
                    write(FD, "\033]LIconLabel\033\\", 14);
                    break;
                case 21:
                    write(FD, "\033]l\033\\", 5);
                    break;
            }
        }
            break;
        case 'u':
            restore_cursor()
            break;
        case ' ':
            continue_sequence(ESC_CSI_ARGS_SPACE);
            break;
        default:
            parse_arg(emulator, (char) codepoint);
    }
}

static inline void do_esc_csi_question_mark(terminal *restrict const emulator, const int codepoint) {
    switch (codepoint) {
        case 'J':
        case 'K': {
            const i8 arg = (i8) getArg(emulator, 0, 0, 0);
            i8 start_col = -1, end_col = -1, start_row = -1, end_row = -1;
            bool just_row = 'K' == codepoint;
            Glyph fill = {.code=' ', .style= CURSOR_STYLE};
            CLEAR_ABOUT_TO_AUTOWRAP;
            switch (arg) {
                case 0:
                    start_col = CURSOR_COL;
                    start_row = CURSOR_ROW;
                    end_col = COLUMNS;
                    end_row = (i8) (just_row ? CURSOR_ROW + 1 : ROWS);
                    break;
                case 1:
                    start_col = 0;
                    start_row = (i8) (just_row ? CURSOR_ROW : 0);
                    end_col = CURSOR_COL + 1;
                    end_row = CURSOR_ROW + 1;
                    break;
                case 2:
                    start_col = 0;
                    start_row = (i8) (just_row ? CURSOR_ROW : 0);
                    end_col = CURSOR_COL + 1;
                    end_row = (i8) (just_row ? CURSOR_ROW + 1 : ROWS);
                    break;
                default:
                    finish_sequence();
            }
            for (i8 row = start_row; row < end_row; row++) {
                Glyph *const dst = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, row)] + start_col;
                for (i8 col = start_col; col < end_col; col++)
                    if (get(dst->style.effect, PROTECTED) == 0)
                        dst[col] = fill;
            }
        }
            break;
        case 'h':
        case 'l': {
            bool setting = 'h' == codepoint;
            if (CSI_INDEX >= MAX_ESCAPE_PARAMETERS)
                CSI_INDEX = MAX_ESCAPE_PARAMETERS - 1;
            for (i8 i = 0; i < CSI_INDEX; i++)
                do_dec_set_or_reset(emulator, setting, CSI_ARG[i]);
        }
            break;
        case 'n':
            if (6 == CSI_ARG[0])
                dprintf(FD, "\033[?%d;%d;1R", CURSOR_ROW + 1, CURSOR_COL + 1);
            else
                finish_sequence();
            break;
        case 'r':
        case 's':
            if (CSI_INDEX >= MAX_ESCAPE_PARAMETERS)
                CSI_INDEX = MAX_ESCAPE_PARAMETERS - 1;
            for (i8 i = 0; i < MAX_ESCAPE_PARAMETERS; i++) {
                int16_t external_bit = CSI_ARG[i];
                int16_t internal = map_decset_bit(external_bit);
                if ('s' == codepoint)
                    set(SAVED_DECSET_BIT, internal);
                else
                    do_dec_set_or_reset(emulator, get(SAVED_DECSET_BIT, internal), external_bit);
            }
            break;
        case '$':
            continue_sequence(ESC_CSI_QUESTIONMARK_ARG_DOLLAR);
            break;
        default:
            parse_arg(emulator, (char) codepoint);
            break;
    }
}

static inline void do_esc_csi_dollar(terminal *restrict const emulator, const int codepoint) {
    bool origin_mode = get(DECSET_BIT, ORIGIN_MODE);
    const i8 effective_top_margin = (i8) (origin_mode ? TOP_MARGIN : 0);
    const i8 effective_bottom_margin = (i8) (origin_mode ? BOTTOM_MARGIN : ROWS);
    const i8 effective_left_margin = (i8) (origin_mode ? LEFT_MARGIN : 0);
    const i8 effective_right_margin = (i8) (origin_mode ? RIGHT_MARGIN : COLUMNS);
    switch (codepoint) {
        case 'v': {
            const i8 arg0 = (i8) (getArg(emulator, 0, 1, 1) - 1 + effective_top_margin);
            const i8 arg1 = (i8) (getArg(emulator, 1, 1, 1) - 1 + effective_left_margin);
            const i8 arg2 = (i8) (getArg(emulator, 2, ROWS, 1) + effective_top_margin);
            const i8 arg3 = (i8) (getArg(emulator, 3, COLUMNS, 1) + effective_left_margin);
            const i8 arg5 = (i8) (getArg(emulator, 5, 1, 1) - 1 + effective_top_margin);
            const i8 arg6 = (i8) (getArg(emulator, 6, 1, 1) - 1 + effective_left_margin);
            const i8 top_source = MIN(arg0, ROWS);
            const i8 left_source = MIN(arg1, COLUMNS);
            const i8 bottom_source = LIMIT(arg2, top_source, ROWS);
            const i8 right_source = LIMIT(arg3, left_source, COLUMNS);
            const i8 dest_top = MIN(arg5, ROWS);
            const i8 dest_left = MIN(arg6, COLUMNS);
            const i8 height_to_copy = MIN(ROWS - dest_top, bottom_source - top_source);
            const i8 width_to_copy = MIN(COLUMNS - dest_left, right_source - left_source);
            block_copy(SCREEN_BUFF, left_source, top_source, width_to_copy, height_to_copy, dest_left, dest_top);
        }
            break;
        case '{':
        case 'x':
        case 'z': {
            const bool erase = 'x' != codepoint, selective = '{' == codepoint, keep_attributes = erase && selective;
            i8 arg_index = 1;
            char fill_char = erase ? ' ' : (char) CSI_ARG[0];
            if (BETWEEN(fill_char, 32, 126) || BETWEEN(fill_char, 160, 255)) {
                i8 top = (i8) (getArg(emulator, arg_index++, 1, 1) + effective_top_margin);
                i8 left = (i8) (getArg(emulator, arg_index++, 1, true) + effective_top_margin);
                i8 bottom = (i8) (getArg(emulator, arg_index++, ROWS, true) + effective_top_margin);
                i8 right = (i8) (getArg(emulator, arg_index, COLUMNS, true) + effective_left_margin);
                top = MIN(top, effective_bottom_margin + 1);
                left = MIN(left, effective_right_margin + 1);
                bottom = MIN(bottom, effective_bottom_margin);
                right = MIN(right, effective_right_margin);
                for (i8 row = (i8) (top - 1); row < bottom; row++) {
                    for (i8 col = (i8) (left - 1); col < right; col++) {
                        Glyph *const glyph1 = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, row)] + col;
                        if (!selective || get(glyph1->style.effect, PROTECTED) == 0) {
                            glyph1->code = fill_char;
                            if (!keep_attributes) glyph1->style = CURSOR_STYLE;
                        }
                    }
                }
            }
        }
            break;
        case 'r':
        case 't': {
            const bool reverse = 't' == codepoint;

            i8 top, left, bottom, right;
            {
                i8 arg0 = (i8) (getArg(emulator, 0, 1, 1) - 1), arg1 = (i8) (getArg(emulator, 1, 1, 1) -
                                                                             1), arg2 = (i8) (
                        getArg(emulator, 2, ROWS, 1) + 1), arg3 = (i8) (getArg(emulator, 3, COLUMNS, 1) + 1);
                top = MIN(arg0, effective_bottom_margin) + effective_top_margin;
                left = MIN(arg1, effective_right_margin) + effective_left_margin;
                bottom = MIN(arg2, effective_bottom_margin - 1) + effective_top_margin;
                right = MIN(arg3, effective_right_margin - 1) + effective_left_margin;
            }
            if (4 <= CSI_INDEX) {
                bool set_or_clear;
                uint16_t bits;
                i8 arg;
                if (CSI_INDEX >= MAX_ESCAPE_PARAMETERS)
                    CSI_INDEX = MAX_ESCAPE_PARAMETERS - 1;
                for (i8 i = 4; i < CSI_INDEX; i++) {
                    set_or_clear = true;
                    arg = (i8) CSI_ARG[i];
                    switch (arg) {
                        case 0:
                            bits = BOLD | UNDERLINE | BLINK;
                            if (!reverse)set_or_clear = false;
                            break;
                        case 1:
                            bits = BOLD;
                            break;
                        case 4:
                            bits = UNDERLINE;
                            break;
                        case 5:
                            bits = BLINK;
                            break;
                        case 7:
                            bits = INVERSE;
                            break;
                        case 22:
                            bits = BOLD;
                            set_or_clear = false;
                            break;
                        case 24:
                            bits = UNDERLINE;
                            set_or_clear = false;
                            break;
                        case 25:
                            bits = BLINK;
                            set_or_clear = false;
                            break;
                        case 27:
                            bits = INVERSE;
                            set_or_clear = false;
                            break;
                        default:
                            bits = 0;
                    }

                    if (!reverse || set_or_clear) {
                        Term_row line;
                        uint16_t effect;
                        i8 x;
                        i8 end_of_line;
                        for (i8 y = top; y < bottom; y++) {
                            line = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y)];
                            x = get(DECSET_BIT, RECTANGULAR_CHANGE_ATTRIBUTE) || y == top ? left
                                                                                          : effective_left_margin;
                            end_of_line = get(DECSET_BIT, RECTANGULAR_CHANGE_ATTRIBUTE) || y + 1 == bottom ? right
                                                                                                           : effective_right_margin;
                            for (; x < end_of_line; x++) {
                                effect = line[x].style.effect;
                                if (reverse)
                                    line[x].style.effect = (effect & ~bits) || (bits & ~effect);
                            }
                        }
                    }
                }
            }
        }
            break;
        default:
            finish_sequence();
            break;
    }
}

static inline void do_device_control(terminal *restrict const emulator, const int codepoint) {
    if ('\\' == codepoint) {
        if (strncmp(OSC_ARG, "$q", 2) == 0) {
            if (strncmp("$q\"p", OSC_ARG, 4) == 0)
                write(FD, "\033P1$r64;1\"p\033\\", 13);
            else
                finish_sequence();
        } else if (strncmp(OSC_ARG, "+q", 2) == 0) {
            char *dcs = OSC_ARG + 2;
            int16_t len = 0;
            while (true) {
                len = (int16_t) (strchr(dcs, ';') - dcs);
                if (0 >= len) {
                    len = (int16_t) (OSC_LEN - (dcs - OSC_ARG));
                    if (0 >= len) break;
                }
                if (0 == len % 2) {
                    char trans_buff[len];
                    char c;
                    bool err = false;
                    int16_t trans_buff_len = 0;
                    char *response = NULL;
                    size_t response_len;
                    for (int i = 0; i < len; i += 2) {
                        c = (char) str_to_hex_int8(dcs + i, &err, 2);
                        if (err) continue;
                        trans_buff[trans_buff_len++] = c;
                    }
                    if (strncmp(trans_buff, "Co", 2) == 0 || strncmp(trans_buff, "colors", 6) == 0) {
                        response = "256";
                        response_len = 3;
                    } else if (strncmp(trans_buff, "TN", 2) == 0 || strncmp(trans_buff, "name", 4) == 0) {
                        response = "xterm";
                        response_len = 5;
                    } else
                        response_len = get_code_from_term_cap(&response, trans_buff, (size_t) trans_buff_len,
                                                              get(DECSET_BIT, APPLICATION_CURSOR_KEYS),
                                                              get(DECSET_BIT, APPLICATION_KEYPAD));

                    if (response_len == 0)
                        dprintf(FD, "\033P0+r%.*s\033\\", len, dcs);
                    else {
                        char hex_encoder[2 * response_len];
                        for (size_t i = 0; i < response_len; i++)
                            sprintf(hex_encoder + 2 * i, "%02x", response[i]);
                        dprintf(FD, "\033P1+r%.*s=%.*s\033\\", len, dcs, response_len * 2, hex_encoder);
                        free(response);
                    }
                }
                dcs += len + 1;
            }
        }
        finish_sequence();
    } else {
        if (OSC_LEN < MAX_OSC_STRING_LENGTH) {
            OSC_LEN += append_codepoint(codepoint, OSC_ARG + OSC_LEN);
            continue_sequence(ESC_STATE);
        } else {
            OSC_LEN = 0;
            finish_sequence();
        }
    }
}

static inline void handle_esc_state(terminal *restrict const emulator, const int codepoint) {
    SET_DO_NOT_CONTINUE_SEQUENCE;
    switch (ESC_STATE) {
        case ESC_APC_ESCAPE:
            (codepoint == '\\') ? finish_sequence() : continue_sequence(ESC_APC);
            return;
        case ESC_NONE:
            if (32 <= codepoint)
                emit_codepoint(emulator, codepoint);
            break;
        case ESC:
            do_esc(emulator, codepoint);
            break;
        case ESC_POUND:
            if ('8' == codepoint)
                block_set(SCREEN_BUFF, 0, 0, COLUMNS, ROWS, (Glyph) {.code='E', .style= CURSOR_STYLE});
            else
                finish_sequence();
            break;
        case ESC_SELECT_LEFT_PAREN:
            set_val(TERM_MODE, G0, '0' == codepoint);
            break;
        case ESC_SELECT_RIGHT_PAREN:
            set_val(TERM_MODE, G1, '0' == codepoint);
            break;
        case ESC_CSI:
            do_esc_csi(emulator, codepoint);
            break;
        case ESC_CSI_EXCLAMATION:
            if ('p' == codepoint) reset_emulator(emulator);
            else
                finish_sequence();
            break;
        case ESC_CSI_QUESTIONMARK:
            do_esc_csi_question_mark(emulator, codepoint);
            break;
        case ESC_CSI_BIGGERTHAN:
            switch (codepoint) {
                case 'c':
                    write(FD, "\033[>41;320;0c", 12);
                    break;
                case 'm':
                    break;
                default:
                    parse_arg(emulator, (char) codepoint);
            }
            break;
        case ESC_CSI_DOLLAR:
            do_esc_csi_dollar(emulator, codepoint);
            break;
        case ESC_CSI_DOUBLE_QUOTE:
            if ('q' == codepoint) {
                i8 arg = (i8) getArg(emulator, 0, 0, 0);
                switch (arg) {
                    case 0:
                    case 2:
                        clear(CURSOR_STYLE.effect, PROTECTED);
                        break;
                    case 1:
                        set(CURSOR_STYLE.effect, PROTECTED);
                        break;
                    default:
                        finish_sequence();
                }
            } else
                finish_sequence();
            break;
        case ESC_CSI_SINGLE_QUOTE:
            switch (codepoint) {
                case '}': {
                    const i8 columns_after_cursor = RIGHT_MARGIN - CURSOR_COL;
                    const i8 arg = (i8) getArg(emulator, 0, 1, 1);
                    const i8 columns_to_insert = MIN(arg, columns_after_cursor);
                    const i8 columns_to_move = (i8) (columns_after_cursor - columns_to_insert);
                    block_copy(SCREEN_BUFF, CURSOR_COL, 0, columns_to_move, ROWS, CURSOR_COL, 0);
                    block_clear(CURSOR_COL, 0, columns_to_insert, ROWS);
                }
                    break;
                case '~': {
                    const i8 columns_after_cursor = RIGHT_MARGIN - CURSOR_COL;
                    const i8 arg = (i8) getArg(emulator, 0, 1, 1);
                    const i8 columns_to_delete = MIN(arg, columns_after_cursor);
                    const i8 columns_to_move = (i8) (columns_after_cursor - columns_to_delete);
                    block_copy(SCREEN_BUFF, CURSOR_COL + columns_to_delete, 0, columns_to_move, ROWS, CURSOR_COL, 0);
                }
                    break;
                default:
                    finish_sequence();
                    break;
            }
            break;
        case ESC_PERCENT:
            break;
        case ESC_OSC:
            if (codepoint == 7) //Codepoint 27 handled at the beginning
                do_osc_set_text_parameters(emulator, "\007");
            else if (OSC_LEN < MAX_OSC_STRING_LENGTH) {
                OSC_LEN += append_codepoint(codepoint, OSC_ARG + OSC_LEN);
                continue_sequence(ESC_STATE);
            } else
                finish_sequence();
            break;
        case ESC_OSC_ESC:
            if ('\\' == codepoint)
                do_osc_set_text_parameters(emulator, "\033\\");
            else {
                if (OSC_LEN < MAX_OSC_STRING_LENGTH) {
                    OSC_ARG[OSC_LEN++] = 27;
                    OSC_LEN += append_codepoint(codepoint, OSC_ARG + OSC_LEN);
                    continue_sequence(ESC_STATE);
                } else
                    finish_sequence();
            }
            break;
        case ESC_P:
            do_device_control(emulator, codepoint);
            break;
        case ESC_CSI_QUESTIONMARK_ARG_DOLLAR:
            if ('p' == codepoint) {
                const int16_t mode = CSI_ARG[0];
                i8 value;
                switch (mode) {
                    case 47:
                    case 1047:
                    case 1049:
                        value = (is_alt_buff_active()) ? 1 : 2;
                        break;
                    default: {
                        int16_t internal = map_decset_bit(mode);
                        value = (i8) (internal == -1 ? 0 : get(DECSET_BIT, internal) ? 1 : 2);
                    }
                        break;
                }
                dprintf(FD, "\033[?%d;%d$y", mode, value);
            } else
                finish_sequence();
            break;

        case ESC_CSI_ARGS_SPACE:
            switch (codepoint) {
                case 'q': {
                    const i8 arg = (i8) getArg(emulator, 0, 0, 0);
                    switch (arg) {
                        case 0:
                        case 1:
                        case 2:
                            emulator->cursor_shape = BLOCK;
                            break;
                        case 3:
                        case 4:
                            emulator->cursor_shape = C_UNDERLINE;
                            break;
                        case 5:
                        case 6:
                            emulator->cursor_shape = BAR;
                            break;
                    }
                }
                    break;
                case 't':
                case 'u':
                    break;
                default:
                    finish_sequence();
                    break;
            }
            break;
        case ESC_CSI_ARGS_ASTERIX: {
            const i8 attribute_change_extent = (i8) CSI_ARG[0];
            ('x' == codepoint && BETWEEN(attribute_change_extent, 0, 2)) ? set_val(DECSET_BIT,
                                                                                   RECTANGULAR_CHANGE_ATTRIBUTE,
                                                                                   2 == attribute_change_extent)
                                                                         : finish_sequence();
        }
            break;
        default:
            finish_sequence();
    }
    if (DO_NOT_CONTINUE_SEQUENCE) finish_sequence();
}

static inline void process_codepoint(terminal *restrict const emulator, const int codepoint) {

    switch (codepoint) {
        case 0:
            break;
        case 8:
            if (LEFT_MARGIN == CURSOR_COL) {
                const i8 prev_row = CURSOR_ROW - 1;
                uint16_t *const effect_bit = &SCREEN_BUFF->lines[prev_row][RIGHT_MARGIN - 1].style.effect;
                if (0 <= prev_row && get(*effect_bit, AUTO_WRAPPED)) {
                    clear(*effect_bit, AUTO_WRAPPED);
                    set_cursor_row_col(emulator, prev_row, RIGHT_MARGIN - 1);
                }
            } else {
                CURSOR_COL--;
                CLEAR_ABOUT_TO_AUTOWRAP;
            }
            break;
        case 9: {
            const i8 i = CURSOR_COL + 1;
            CURSOR_COL = (i8) ((i < COLUMNS) ? MIN(i, RIGHT_MARGIN) : RIGHT_MARGIN - 1);
        }
            break;
        case 10:
        case 11:
        case 12:
            do_line_feed(emulator);
            break;
        case 13:
            CURSOR_COL = LEFT_MARGIN;
            CLEAR_ABOUT_TO_AUTOWRAP;
            break;
        case 14:
            set(TERM_MODE, USE_G0);
            break;
        case 15:
            clear(TERM_MODE, USE_G0);
            break;
        case 24:
        case 26:
            if (ESC_NONE != ESC_STATE) {
                finish_sequence();
                emit_codepoint(emulator, 127);
            }
            break;
        case 27:
            switch (ESC_STATE) {
                case ESC_APC:
                    continue_sequence(ESC_APC_ESCAPE);
                    return;
                case ESC_OSC:
                    continue_sequence(ESC_OSC_ESC);
                    break;
                default:
                    set_esc_state(ESC);
                    CSI_INDEX = 0;
                    memset(CSI_ARG, -1, MAX_ESCAPE_PARAMETERS);
            }
            break;
        default:
            handle_esc_state(emulator, codepoint);
    }
}

void create_terminal_environment(struct terminal_config *config) {
    TRANSCRIPT_ROWS = LIMIT(config->transcript_rows, 0, 8129);
    renderer = config->render_conf;
    copy_base64_to_clipboard_hooked = config->copy_to_clipboard;
    get_code_from_term_cap = config->get_code_from_term_cap;
}

terminal *new_terminal(const struct terminal_session_info *restrict const session_info) {
    const int ptm = open("/dev/ptmx", O_RDWR | O_CLOEXEC);
    struct termios tios;
    char devname[64];
    const struct winsize sz = {.ws_row=(unsigned short) session_info->rows, .ws_col=(unsigned short) session_info->columns, .ws_xpixel=session_info->cell_width, .ws_ypixel=session_info->cell_height};
    pid_t pid;

    grantpt(ptm);
    unlockpt(ptm);
    ptsname_r(ptm, devname, sizeof(devname));

    // Enable UTF-8 effect and disable flow control to prevent Ctrl+S from locking up the display.

    tcgetattr(ptm, &tios);
    tios.c_iflag |= IUTF8;
    tios.c_iflag &= (tcflag_t) ~(IXON | IXOFF);
    tcsetattr(ptm, TCSANOW, &tios);


    ioctl(ptm, TIOCSWINSZ, &sz);

    pid = fork();
    if (pid > 0) {
        terminal *restrict const emulator = calloc(1, sizeof(terminal));
        const Glyph glyph = {.style=NORMAL, .code= ' '};
        const size_t row_byte_size = sizeof(Glyph) * (unsigned int) COLUMNS;
        ROWS = session_info->rows;
        COLUMNS = session_info->columns;
        TAB_STOP = malloc((size_t) CELI(COLUMNS, 8) * sizeof(ui8));
        MAIN_BUFF.history_rows = ALT_BUFF.history_rows = 0;
        MAIN_BUFF.lines = malloc((size_t) TRANSCRIPT_ROWS * sizeof(Term_row));
        ALT_BUFF.lines = malloc((size_t) ROWS * sizeof(Term_row));
        for (i8 y = 0; y < ROWS; y++) {
            MAIN_BUFF.lines[y] = malloc(row_byte_size);
            ALT_BUFF.lines[y] = malloc(row_byte_size);
            for (int x = 0; x < COLUMNS; x++)
                MAIN_BUFF.lines[y][x] = ALT_BUFF.lines[y][x] = glyph;
        }
        for (int16_t y = (int16_t) ROWS; y < TRANSCRIPT_ROWS; y++) {
            MAIN_BUFF.lines[y] = malloc(row_byte_size);
            for (i8 x = 0; x < COLUMNS; x++)
                MAIN_BUFF.lines[y][x] = glyph;
        }
        SCREEN_BUFF = &MAIN_BUFF;
        LAST_CODEPOINT = -1;
        reset_emulator(emulator);
        emulator->pid = pid;
        FD = ptm;
        return emulator;
    } else {
        // Clear signals which the Android java process may have blocked:
        sigset_t signals_to_unblock;
        int pts;
        DIR *self_dir;
        sigfillset(&signals_to_unblock);
        sigprocmask(SIG_UNBLOCK, &signals_to_unblock, 0);

        close(ptm);
        setsid();

        pts = open(devname, O_RDWR);
        if (pts < 0) exit(-1);

        dup2(pts, 0);
        dup2(pts, 1);
        dup2(pts, 2);

        self_dir = opendir("/proc/self/FD");
        if (self_dir != NULL) {
            int self_dir_fd = dirfd(self_dir);
            struct dirent *entry;
            while ((entry = readdir(self_dir)) != NULL) {
                int _fd = atoi(entry->d_name);
                if (_fd > 2 && _fd != self_dir_fd) close(_fd);
            }
            closedir(self_dir);
        }
        for (size_t i = 0; i < session_info->env_len; i++)
            putenv(session_info->env[i]);
        chdir(session_info->cwd);
        execvp(session_info->cmd, session_info->argv);
        _exit(1);
    }
}

pid_t get_pid(const terminal *restrict const emulator) {
    return PID;
}

int get_fd(const terminal *restrict const emulator) {
    return FD;
}

void read_data(terminal *restrict const emulator) {
    _Thread_local static int read_len;
    _Thread_local static u_char buffer[BUFFER_SIZE];
    _Thread_local static ui8 remaining_utf_len = 0;
    do {
        _Thread_local static int write_len;
        read_len = read(FD, buffer + remaining_utf_len, BUFFER_SIZE - remaining_utf_len) + remaining_utf_len;
        remaining_utf_len = 0;
        for (write_len = 0; write_len < read_len;) {
            _Thread_local static int codepoint;
            _Thread_local static ui8 byte_len;
            decode_utf(buffer, codepoint, read_len - write_len, byte_len, remaining_utf_len)
            write_len += byte_len;
            process_codepoint(emulator, codepoint);
        }
    } while (remaining_utf_len > 0);
}

void render_terminal(const terminal *restrict const emulator) {
    static i8 x, y;
    static Term_row line;
    static Glyph glyph;
    renderer->on_start_drawing();
    if (get(DECSET_BIT, REVERSE_VIDEO)) renderer->fill_color(COLORS[COLOR_INDEX_FOREGROUND]);
    for (y = 0; y < ROWS; y++) {
        line = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, renderer->top_row + y)];
        for (x = 0; x < COLUMNS; x++) {
            glyph = line[x];
            if (CURSOR.x == x && (CURSOR.y == (y + SCREEN_BUFF->history_rows)) && get(DECSET_BIT, CURSOR_ENABLED)) {
                float w = 1, h = 1;
                switch (CURSOR_SHAPE) {
                    case BLOCK:
                        set_val(glyph.style.effect, INVERSE, get(glyph.style.effect, INVERSE) == 0);
                        break;
                    case C_UNDERLINE:
                        h = 0.25f;
                        break;
                    case BAR:
                        w = 0.25f;
                        break;
                }
                renderer->draw_cell(x, y, w, h, COLORS[COLOR_INDEX_CURSOR]);
            }
            if (get(glyph.style.effect, TRUE_COLOR_BG) == 0) glyph.style.bg.color = COLORS[glyph.style.bg.index];
            if (get(glyph.style.effect, TRUE_COLOR_FG) == 0) glyph.style.fg.color = COLORS[glyph.style.fg.index];
            if (get(glyph.style.effect, INVISIBLE) == 0) renderer->draw_glyph(glyph, x, y);
        }
    }
    renderer->on_end_drawing();
}

void send_mouse_event(const terminal *restrict const emulator, i8 mouse_button, i8 x, i8 y, bool pressed) {
    if (mouse_button != MOUSE_LEFT_BUTTON_MOVED || get(DECSET_BIT, MOUSE_TRACKING_BUTTON_EVENT)) {
        x = LIMIT(x, 1, COLUMNS);
        y = LIMIT(y, 1, ROWS);
        if (get(DECSET_BIT, MOUSE_PROTOCOL_SGR))
            dprintf(FD, "\033[<%d;%d;%d%c", mouse_button, x, y, pressed ? 'M' : 'm');
        else {
            if (!pressed) mouse_button = 3;
            write(FD, (ui8[]) {'\033', '[', 'M', ((ui8) (32 + mouse_button)), ((ui8) (32 + x)), ((ui8) (32 + y))}, 6);
        }
    }
}

char *get_terminal_text(const terminal *restrict const emulator, int x1, int y1, int x2, int y2) {
    int text_size;
    size_t index = 0;
    char *text;
    Term_row row;
    x1 = LIMIT(x1, 0, COLUMNS);
    x2 = LIMIT(x2, x1, COLUMNS);
    y1 = LIMIT(y1, -SCREEN_BUFF->history_rows, is_alt_buff_active() ? ROWS : TRANSCRIPT_ROWS);
    y2 = LIMIT(y2, y1, is_alt_buff_active() ? ROWS : TRANSCRIPT_ROWS);
    text_size = ((COLUMNS - x1) + ((y2 - y1 - 1) * COLUMNS) + x2) + 1;
    text = malloc((unsigned int) text_size * 4 * sizeof(char));
    row = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y1)];
    for (; x1 < COLUMNS; x1++)
        index += append_codepoint(row[x1].code, text + index);
    for (; y1 < y2 - 1; y1++) {
        row = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y1)];
        for (int x = 0; x < COLUMNS; x++) index += append_codepoint(row[x].code, text + index);
    }
    row = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y2)];
    for (int x = 0; x < x2; x++) index += append_codepoint(row[x].code, text + index);
    text = realloc(text, (index + 1) * sizeof(char));
    return text;
}

void destroy_term_emulator(terminal *const emulator) {
    close(FD);
    for (int i = 0; i < ROWS; i++) {
        free(MAIN_BUFF.lines[i]);
        free(ALT_BUFF.lines[i]);
    }
    for (int16_t i = (int16_t) ROWS; i < TRANSCRIPT_ROWS; i++)
        free(MAIN_BUFF.lines[i]);
    free(TAB_STOP);
    free(MAIN_BUFF.lines);
    free(ALT_BUFF.lines);
    free(emulator);
}
