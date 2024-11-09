#ifndef TERM_EMULATOR
#define TERM_EMULATOR

#include <unistd.h>
#include <wchar.h>
#include "term_buffer.h"
#include "key_handler.h"
#include "common/utils.h"

#include <dirent.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>

#define UNICODE_REPLACEMENT_CHARACTER 0xFFFD
#define MAX_ESCAPE_PARAMETERS 16
#define MAX_OSC_STRING_LENGTH 2048

#define ESC_NONE  0
#define ESC  1
#define ESC_POUND  2
#define ESC_SELECT_LEFT_PAREN  3
#define ESC_SELECT_RIGHT_PAREN  4
#define ESC_CSI  6
#define ESC_CSI_QUESTIONMARK  7
#define ESC_CSI_DOLLAR  8
#define ESC_PERCENT  9
#define ESC_OSC  10
#define ESC_OSC_ESC  11
#define ESC_CSI_BIGGERTHAN  12
#define ESC_P  13
#define ESC_CSI_QUESTIONMARK_ARG_DOLLAR  14
#define ESC_CSI_ARGS_SPACE  15
#define ESC_CSI_ARGS_ASTERIX  16
#define ESC_CSI_DOUBLE_QUOTE  17
#define ESC_CSI_SINGLE_QUOTE  18
#define ESC_CSI_EXCLAMATION  19
#define ESC_APC  20
#define ESC_APC_ESCAPE  21

typedef union {
    struct {
        bool APPLICATION_CURSOR_KEYS: 1;
        bool REVERSE_VIDEO: 1;
        bool ORIGIN_MODE: 1;
        bool AUTOWRAP: 1;
        bool CURSOR_ENABLED: 1;
        bool APPLICATION_KEYPAD: 1;
        bool LEFTRIGHT_MARGIN_MODE: 1;
        bool MOUSE_TRACKING_PRESS_RELEASE: 1;
        bool MOUSE_TRACKING_BUTTON_EVENT: 1;
        bool SEND_FOCUS_EVENTS: 1;
        bool MOUSE_PROTOCOL_SGR: 1;
        bool BRACKETED_PASTE_MODE: 1;
        bool RECTANGULAR_CHANGEATTRIBUTE: 1;

        bool G0: 1, G1: 1, USE_G0: 1;//IT IS NOT DECSET THIS IS TO SAVE SOME MEMORY
    };
    unsigned short raw;
} DECSET_BIT;

typedef enum {
    BLOCK = 0, UNDERLINE = 1, BAR = 2
} CURSOR_SHAPE;

typedef struct {
    glyph_style style;
    int x, y;
    DECSET_BIT decsetBit;
} term_cursor;

typedef struct {
    uint8_t *_tabStop;
    uint_least32_t colors[NUM_INDEXED_COLORS];
    term_cursor cursor, _saved_state, _saved_state_alt;
    struct CSI {
        short arg[MAX_ESCAPE_PARAMETERS];
        uint8_t state;
        uint8_t index;
        bool dontContinueSequence: 1;
    } _csi;
    Term_buffer _main, _alt;
    int fd;
    pid_t pid;
    Term_buffer *screen_buff;
    uint_least32_t _last_codepoint;
    int _leftMargin, _rightMargin, _topMargin, _bottomMargin;
    struct OSC {
        char args[MAX_OSC_STRING_LENGTH];
        int len;
    } _osc;
    unsigned short _saved_decset;
    CURSOR_SHAPE cursor_shape: 2;
    bool MODE_INSERT: 1, ABOUT_TO_AUTO_WRAP: 1;
} Term_emulator;

static unsigned short ROWS = 24;
static unsigned short COLUMNS = 45;
static unsigned short TRANSCRIPT_ROWS = 200;

#define FD emulator->fd
#define finishSequence emulator->_csi.state=ESC_NONE
#define continue_sequence(new_esc_state) {emulator->_csi.state = new_esc_state;emulator->_csi.dontContinueSequence=false;}
#define cursor_col emulator->cursor.x
#define cursor_row emulator->cursor.y
#define cursor_style emulator->cursor.style
#define left_margin emulator->_leftMargin
#define right_margin emulator->_rightMargin
#define top_margin emulator->_topMargin
#define bottom_margin emulator->_bottomMargin
#define esc_state emulator->_csi.state
#define decset_bit emulator->cursor.decsetBit
#define term_mode decset_bit
#define osc_arg emulator->_osc.args
#define about_to_autowrap emulator->ABOUT_TO_AUTO_WRAP
#define screen emulator->screen_buff

//TODO HOOK THIS UP
__attribute__((weak))void copy_base64_to_clipboard_hooked(char *text);

Term_emulator *new_term_emulator(const char *restrict cmd);

void read_data(Term_emulator *emulator);

void destroy_term_emulator(Term_emulator *emulator);

static int map_decset_bit(int bit) {
    const static unsigned int lookupTable[][2] = {{1,    1 << 0},   // DECSET_BIT_APPLICATION_CURSOR_KEYS
                                                  {5,    1 << 1},   // REVERSE_VIDEO
                                                  {6,    1 << 2},   // ORIGIN_MODE
                                                  {7,    1 << 3},   // AUTOWRAP
                                                  {25,   1 << 4},   // CURSOR_ENABLED
                                                  {66,   1 << 5},   // APPLICATION_KEYPAD
                                                  {69,   1 << 6},   // LEFTRIGHT_MARGIN_MODE
                                                  {1000, 1 << 7},   // MOUSE_TRACKING_PRESS_RELEASE
                                                  {1002, 1 << 8},   // MOUSE_TRACKING_BUTTON_EVENT
                                                  {1004, 1 << 9},   // SEND_FOCUS_EVENTS
                                                  {1006, 1 << 10},  // MOUSE_PROTOCOL_SGR
                                                  {2004, 1 << 11}   // BRACKETED_PASTE_MODE
    };
    return (int) map_search(bit, -1, lookupTable, 12);
}

static void reset_emulator(Term_emulator *restrict const emulator) {
    emulator->_csi.index = 0;
    esc_state = ESC_NONE;
    emulator->_csi.dontContinueSequence = true;
    term_mode.USE_G0 = true;
    term_mode.G0 = term_mode.G1 = emulator->MODE_INSERT = false;
    top_margin = left_margin = 0;
    bottom_margin = ROWS;
    right_margin = COLUMNS;
    cursor_style.fg.index = COLOR_INDEX_FOREGROUND;
    cursor_style.bg.index = COLOR_INDEX_BACKGROUND;
    {
        int temp = CELI(COLUMNS, 8);
        for (int i = 0; i < temp; i++) emulator->_tabStop[i] = 1;//Setting default tab stop

    }
    decset_bit.AUTOWRAP = true;
    decset_bit.CURSOR_ENABLED = true;
    emulator->_saved_state = emulator->_saved_state_alt = (term_cursor) {.style=NORMAL, .decsetBit= emulator->cursor.decsetBit};
    reset_all_color()
}

static void do_osc_set_text_parameters(Term_emulator *restrict const emulator, const char *const terminator) {
    int value = 0;
    char *text_param;
    //TODO LIGHT STRLEN
    int len;
    bool end_of_input;

    for (int osc_index = 0; osc_index < emulator->_osc.len; osc_index++) {
        const char b = osc_arg[osc_index];
        switch (b) {
            case ';':
                text_param = osc_arg + osc_index + 1;
                len = emulator->_osc.len - (osc_index + 1);
                goto exit_loop;
            case '0'...'9':
                value = value * 10 + (b - '0');
                break;
            default:
                finishSequence;
                return;
        }
    }
    exit_loop:;

    switch (value) {
        case 0:
        case 1:
        case 2:
        case 119:
        default:
            finishSequence;
            return;
        case 4: {
            int color_index = -1;
            int parsing_pair_start = -1;
            char b;
            for (int i = 0;; i++) {
                end_of_input = i == len;
                b = text_param[i];
                if (end_of_input || ';' == b) {
                    if (0 > parsing_pair_start) parsing_pair_start = (int) i + 1;
                    else {
                        if (BETWEEN(color_index, 0, 255)) {
                            parse_color_to_index(emulator->colors, color_index, text_param + parsing_pair_start, i - parsing_pair_start);
                            color_index = parsing_pair_start = -1;
                        } else {
                            finishSequence;
                            return;
                        }
                    }
                } else if (0 > parsing_pair_start && BETWEEN(b, '0', '9'))
                    color_index = (0 < color_index ? 0 : color_index * 10) + (b - '0');
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
            int special_index = COLOR_INDEX_FOREGROUND + value - 10;
            unsigned last_semi_index = 0;
            for (int char_index = 0;; char_index++) {
                end_of_input = char_index == len;
                if (end_of_input || ';' == text_param[char_index]) {
                    char *color_spec = text_param + last_semi_index;
                    if ('?' == *color_spec) {
                        const uint_least32_t rgb = emulator->colors[special_index];
                        unsigned int r = (65535 * ((rgb & 0x00FF0000) >> 16)) / 255;
                        unsigned int g = (65535 * ((rgb & 0x0000FF00) >> 8)) / 255;
                        unsigned int b = (65535 * (rgb & 0x000000FF)) / 255;
                        dprintf(FD, "\033]%d;rgb:%04x/%04x/%04x%s", value, r, g, b, terminator);
                    } else
                        parse_color_to_index(emulator->colors, special_index, color_spec, char_index - last_semi_index);
                    special_index++;
                    if (end_of_input || COLOR_INDEX_CURSOR < special_index || ++char_index >= len) break;
                    last_semi_index = char_index;
                }
            }
        }
            break;
        case 52:
            copy_base64_to_clipboard_hooked(strchr(text_param, ';'));
            break;

        case 104:
            if (len == 0) reset_all_color()
            else {
                int last_index = 0;
                for (int char_index = 0;; char_index++) {
                    end_of_input = char_index == len;
                    if (end_of_input || ';' == text_param[char_index]) {
                        int char_len = char_index - last_index;
                        int color_to_reset = 0;
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
}

static void scroll_down(Term_emulator *restrict const emulator, int n) {
    if (0 != left_margin || right_margin != COLUMNS) {
        block_copy(screen, left_margin, top_margin + 1, right_margin - left_margin, bottom_margin - top_margin - 1, left_margin, top_margin);
        block_set(screen, left_margin, bottom_margin - 1, right_margin - left_margin, 1, (Glyph) {.code=' ', .style= cursor_style});
    } else {
        int total_row = screen == &emulator->_alt ? ROWS : TRANSCRIPT_ROWS;
        Term_row temp_rows[n], temp;
        Glyph fill = {.code=' ', .style=cursor_style};
        memcpy(temp_rows, screen->lines, n * sizeof(Term_row));
        memcpy(screen->lines, screen->lines + n, (total_row - n) * sizeof(Term_row));
        for (int y = 0; y < n; y++) {
            temp = temp_rows[y];
            screen->lines[total_row - 1 - y] = temp;
            for (int x = 0; x < COLUMNS; x++) temp[x] = fill;
        }
        if (screen->history_rows + n - 1 < total_row - ROWS) screen->history_rows += n;
    }
}

static void do_line_feed(Term_emulator *restrict const emulator) {
    int new_cursor_row = cursor_row + 1;
    if (cursor_row >= bottom_margin) {
        if (cursor_row != ROWS - 1) {
            cursor_row = new_cursor_row;
            about_to_autowrap = false;
        }
    } else {
        if (new_cursor_row == bottom_margin) {
            scroll_down(emulator, 1);
            new_cursor_row = bottom_margin - 1;
        }
        cursor_row = new_cursor_row;
        about_to_autowrap = false;
    }
}

#define set_char(codepoint, x, y)  screen->lines[internal_row_buff(screen,y)][x] = (Glyph) {.code=codepoint, .style= cursor_style}

static void emit_codepoint(Term_emulator *restrict const emulator, wchar_t codepoint) {
    static const unsigned int table[][2] = {{'0', L'█'},    // Solid block
                                            {'_', L' '},    // Blank
                                            {'`', L'◆'},    // Diamond
                                            {'a', L'▒'},    // Checker board
                                            {'b', L'␉'},    // Horizontal tab
                                            {'c', L'␌'},    // Form feed
                                            {'d', L'\r'},   // Carriage return
                                            {'e', L'␊'},    // Linefeed
                                            {'f', L'°'},    // Degree
                                            {'g', L'±'},    // Plus-minus
                                            {'h', L'\n'},   // Newline
                                            {'i', L'␋'},    // Vertical tab
                                            {'j', L'┘'},    // Lower right corner
                                            {'k', L'┐'},    // Upper right corner
                                            {'l', L'┌'},    // Upper left corner
                                            {'m', L'└'},    // Lower left corner
                                            {'n', L'┼'},    // Crossing lines
                                            {'o', L'⎺'},    // Horizontal line - scan 1
                                            {'p', L'⎻'},    // Horizontal line - scan 3
                                            {'q', L'─'},    // Horizontal line - scan 5
                                            {'r', L'⎼'},    // Horizontal line - scan 7
                                            {'s', L'⎽'},    // Horizontal line - scan 9
                                            {'t', L'├'},    // T facing rightwards
                                            {'u', L'┤'},    // T facing leftwards
                                            {'v', L'┴'},    // T facing upwards
                                            {'w', L'┬'},    // T facing downwards
                                            {'x', L'│'},    // Vertical line
                                            {'y', L'≤'},    // Less than or equal to
                                            {'z', L'≥'},    // Greater than or equal to
                                            {'{', L'π'},    // Pi
                                            {'|', L'≠'},    // Not equal to
                                            {'}', L'£'},    // UK pound
                                            {'~', L'·'},    // Centered dot
    };

    const int display_width = wcwidth(codepoint);
    const bool cursor_in_last_column = cursor_col == right_margin - 1;

    emulator->_last_codepoint = codepoint;
    if (term_mode.USE_G0 ? term_mode.G0 : term_mode.G1) codepoint = map_search(codepoint, codepoint, table, 33);
    if (decset_bit.AUTOWRAP) {
        if (cursor_in_last_column && ((about_to_autowrap && 1 == display_width) || 2 == display_width)) {
            screen->lines[cursor_row][cursor_col].style.effect.AUTO_WRAPPED = true;
            cursor_col = left_margin;
            if (cursor_row + 1 < bottom_margin)cursor_row++;
            else scroll_down(emulator, 1);
        }
    } else if (cursor_in_last_column && display_width == 2) return;
    if (emulator->MODE_INSERT && 0 < display_width) {
        const int dest_col = cursor_col + display_width;
        if (dest_col < right_margin)
            block_copy(screen, cursor_col, cursor_row, right_margin - dest_col, 1, dest_col, cursor_row);
    }

    set_char(codepoint, cursor_col - (0 >= display_width && about_to_autowrap), cursor_row);

    if (decset_bit.AUTOWRAP && 0 < display_width) about_to_autowrap = (cursor_col == (right_margin - display_width));

    cursor_col = MIN(cursor_col + display_width, right_margin - 1);
}

static void save_cursor(Term_emulator *restrict const emulator) {
    if (screen == &emulator->_alt)
        emulator->_saved_state_alt = emulator->cursor;
    else emulator->_saved_state = emulator->cursor;
}

#define restore_cursor() emulator->cursor = (screen == &emulator->_alt) ? emulator->_saved_state_alt : emulator->_saved_state;

#define block_clear(sx, sy, w, h) block_set(screen, sx, sy, w, h, (Glyph) { cursor_style,' '})

#define clear_transcript() block_set(&emulator->_main, 0, 0, COLUMNS, TRANSCRIPT_ROWS, (Glyph) {.code=' ',.style= NORMAL}); \
                           emulator->_main.history_rows=0;

static void set_cursor_position_origin_mode(Term_emulator *restrict const emulator, int x, int y) {
    bool origin_mode = decset_bit.ORIGIN_MODE;
    const int effective_top_margin = origin_mode ? top_margin : 0;
    const int effective_bottom_margin = (origin_mode ? bottom_margin : ROWS) - 1;
    const int effective_left_margin = origin_mode ? left_margin : 0;
    const int effective_right_margin = (origin_mode ? right_margin : COLUMNS) - 1;
    const int temp_col = effective_left_margin + x, temp_row = effective_top_margin + y;
    cursor_col = LIMIT(temp_col, effective_left_margin, effective_right_margin);
    cursor_row = LIMIT(temp_row, effective_top_margin, effective_bottom_margin);
    about_to_autowrap = false;
}

static void set_cursor_row_col(Term_emulator *restrict const emulator, int x, int y) {
    cursor_row = LIMIT(y, 0, ROWS - 1);
    cursor_col = LIMIT(x, 0, COLUMNS - 1);
    about_to_autowrap = false;
}

static int getArg(Term_emulator *restrict const emulator, int index, int default_value, bool treat_zero_as_default) {
    int result = emulator->_csi.arg[index];
    if (0 > result || (0 == result && treat_zero_as_default)) result = default_value;
    return result;
}

static int next_tab_stop(Term_emulator *restrict const emulator, int num_tabs) {
    for (int i = cursor_col + 1; i < COLUMNS; i++)
        if (emulator->_tabStop[i / 8] & (1 << (i & 7)) && (0 == --num_tabs)) return MIN(i, right_margin);

    return right_margin - 1;
}

static void parse_arg(Term_emulator *restrict const emulator, wchar_t input_byte) {
    switch (input_byte) {
        case '0'...'9':
            if (emulator->_csi.index < MAX_ESCAPE_PARAMETERS) {
                unsigned int old_value = emulator->_csi.arg[emulator->_csi.index];
                const unsigned int this_digit = input_byte - '0';
                old_value = (0 < old_value) ? (old_value * 10) + this_digit : this_digit;
                if (old_value > 9999)old_value = 9999;
                emulator->_csi.arg[emulator->_csi.index] = (short) old_value;
            }
            continue_sequence(emulator->_csi.state)
            break;
        case ';':
            if (emulator->_csi.index < MAX_ESCAPE_PARAMETERS) emulator->_csi.index++;
            continue_sequence(emulator->_csi.state)
            break;
        default:
            finishSequence;
            break;
    }
}

static void do_dec_set_or_reset(Term_emulator *restrict const emulator, bool setting, int external_bit) {

    switch (external_bit) {
        case 1 :
            decset_bit.APPLICATION_CURSOR_KEYS = setting;
            break;
        case 3: {
            left_margin = top_margin = 0;
            bottom_margin = ROWS;
            right_margin = COLUMNS;
            decset_bit.LEFTRIGHT_MARGIN_MODE = false;
            block_clear(0, 0, COLUMNS, ROWS);
            cursor_row = cursor_col = 0;
            about_to_autowrap = false;
        }
            break;
        case 4:
        case 9:
        case 12:
        case 40:
        case 45:
        case 1001:
        case 1003:
        case 1005:
        case 1015:
        case 1034:
            break;
        case 5 :
            decset_bit.REVERSE_VIDEO = setting;
            break;
        case 6 :
            decset_bit.ORIGIN_MODE = setting;
            if (setting)set_cursor_position_origin_mode(emulator, 0, 0);
            break;
        case 7 :
            decset_bit.AUTOWRAP = setting;
            break;
        case 25 :
            decset_bit.CURSOR_ENABLED = setting;
            break;
        case 66 :
            decset_bit.APPLICATION_KEYPAD = setting;
            break;
        case 69 :
            decset_bit.LEFTRIGHT_MARGIN_MODE = setting;
            if (!setting) {
                left_margin = 0;
                right_margin = COLUMNS;
            }
            break;
        case 1000 :
            decset_bit.MOUSE_TRACKING_PRESS_RELEASE = setting;
            break;
        case 1002 :
            decset_bit.MOUSE_TRACKING_BUTTON_EVENT = setting;
            break;
        case 1004 :
            decset_bit.SEND_FOCUS_EVENTS = setting;
            break;
        case 1006 :
            decset_bit.MOUSE_PROTOCOL_SGR = setting;
            break;
        case 1048:
            if (setting) save_cursor(emulator);
            else
                restore_cursor()
            break;
        case 47:
        case 1047:
        case 1049: {
            Term_buffer *new_screen = setting ? &emulator->_alt : &emulator->_main;
            if (new_screen != screen) {
                if (setting) save_cursor(emulator);
                else
                    restore_cursor()
                if (new_screen == &emulator->_alt) block_set(new_screen, 0, 0, COLUMNS, ROWS, (Glyph) {.code=' ', .style= cursor_style});
            }
        }
            break;
        case 2004 :
            decset_bit.BRACKETED_PASTE_MODE = setting;
            break;
        default:
            finishSequence;
            break;
    }
}

__attribute__((hot)) static void process_codepoint(Term_emulator *restrict const emulator, wchar_t codepoint) {

    switch (codepoint) {
        case 0:
            break;
        case 8:
            if (left_margin == cursor_col) {
                const int prev_row = cursor_row - 1;
                text_effect *effect_bit = &screen->lines[prev_row][right_margin - 1].style.effect;
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
            int i = cursor_col + 1;
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
            switch (esc_state) {
                case ESC_APC: continue_sequence(ESC_APC_ESCAPE)
                    return;
                case ESC_OSC: continue_sequence(ESC_OSC_ESC)
                    break;
                default:
                    esc_state = ESC;
                    emulator->_csi.index = 0;
                    memset(emulator->_csi.arg, -1, MAX_ESCAPE_PARAMETERS);
                    break;
            }
            break;
        default: {
            emulator->_csi.dontContinueSequence = true;
            switch (esc_state) {
                case ESC_APC_ESCAPE:
                    if (codepoint == '\\') finishSequence;
                    else continue_sequence(ESC_APC)
                    return;
                case ESC_NONE:
                    if (32 <= codepoint)emit_codepoint(emulator, codepoint);
                    break;
                case ESC:
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
                                const int rows = bottom_margin - top_margin;
                                block_copy(screen, left_margin, top_margin, right_margin - left_margin - 1, rows, left_margin + 1, top_margin);
                                block_set(screen, right_margin - 1, top_margin, 1, rows,
                                          (Glyph) {.code=' ', .style={cursor_style.fg, cursor_style.bg, {0}}});
                            }
                            break;
                        case '7':
                            save_cursor(emulator);
                            break;
                        case '8':
                            restore_cursor()
                            break;
                        case '9':
                            if (cursor_col < right_margin - 1)cursor_col++;
                            else {
                                const int rows = bottom_margin - top_margin;
                                block_copy(screen, left_margin + 1, top_margin, right_margin - left_margin - 1, rows, left_margin, top_margin);
                                block_set(screen, right_margin - 1, top_margin, 1, rows,
                                          (Glyph) {.code=' ', .style= {cursor_style.fg, cursor_style.bg, {0}}});
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
                            cursor_col = decset_bit.ORIGIN_MODE ? left_margin : 0;
                            about_to_autowrap = false;
                            do_line_feed(emulator);
                            break;
                        case 'F':
                            set_cursor_row_col(emulator, 0, bottom_margin - 1);
                            break;
                        case 'H':
                            emulator->_tabStop[cursor_col / 8] |= true << (cursor_col & 7);
                            break;
                        case 'M':
                            if (cursor_row <= top_margin) {
                                block_copy(screen, 0, top_margin, COLUMNS, bottom_margin - top_margin - 1, 0, top_margin + 1);
                                block_clear(0, top_margin, COLUMNS, 1);
                            } else
                                cursor_row--;
                            break;
                        case 'N':
                        case '0':
                            break;
                        case 'P':
                            emulator->_osc.len = 0;
                            continue_sequence(ESC_P)
                            break;
                        case '[': continue_sequence(ESC_CSI)
                            break;
                        case '=':
                            decset_bit.APPLICATION_KEYPAD = true;
                            break;
                        case ']':
                            emulator->_osc.len = 0;
                            continue_sequence(ESC_OSC)
                            break;
                        case '>':
                            decset_bit.APPLICATION_KEYPAD = false;
                            break;
                        default:
                            finishSequence;
                            break;
                    }
                    break;
                case ESC_POUND:
                    if ('8' == codepoint) block_set(screen, 0, 0, COLUMNS, ROWS, (Glyph) {.code='E', .style= cursor_style});
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
                            const int columns_after_cursor = COLUMNS - cursor_col;
                            const int arg = getArg(emulator, 0, 1, true);
                            const int spaces_to_insert = MIN(arg, columns_after_cursor);
                            const int chars_to_move = columns_after_cursor - spaces_to_insert;
                            about_to_autowrap = false;
                            block_copy(screen, cursor_col, cursor_row, chars_to_move, 1, cursor_col + spaces_to_insert, cursor_row);
                            block_clear(cursor_col, cursor_row, spaces_to_insert, 1);
                        }
                            break;
                        case 'A': {
                            const int row = cursor_row - getArg(emulator, 0, 1, 1);
                            cursor_row = MAX(0, row);
                            about_to_autowrap = false;
                        }
                            break;
                        case 'B': {
                            const int row = cursor_row + getArg(emulator, 0, 1, 1);
                            cursor_row = MIN(ROWS - 1, row);
                            about_to_autowrap = false;
                        }
                            break;
                        case 'C':
                        case 'a': {
                            const int col = cursor_col + getArg(emulator, 0, 1, 1);
                            cursor_col = MIN(right_margin - 1, col);
                            about_to_autowrap = false;
                        }
                            break;
                        case 'D': {
                            const int col = cursor_col - getArg(emulator, 0, 1, 1);
                            cursor_col = MAX(left_margin, col);
                            about_to_autowrap = false;
                        }
                            break;
                        case 'E':
                            set_cursor_position_origin_mode(emulator, 0, cursor_row + getArg(emulator, 0, 1, 1));
                            break;
                        case 'F':
                            set_cursor_position_origin_mode(emulator, 0, cursor_row - getArg(emulator, 0, 1, 1));
                            break;
                        case 'G': {
                            const int arg = emulator->_csi.arg[0];
                            cursor_col = LIMIT(arg, 1, COLUMNS);
                            about_to_autowrap = false;
                        }
                            break;
                        case 'H':
                        case 'f':
                            set_cursor_position_origin_mode(emulator, getArg(emulator, 1, 1, 1) - 1, getArg(emulator, 0, 1, 1));
                            break;
                        case 'I':
                            cursor_col = next_tab_stop(emulator, getArg(emulator, 0, 1, 1));
                            about_to_autowrap = false;
                            break;
                        case 'J': {
                            const int arg = getArg(emulator, 0, 0, 1);
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
                                clear_transcript()
                                    break;
                                default:
                                    finishSequence;
                                    return;

                            }
                            about_to_autowrap = false;
                        }
                            break;
                        case 'K': {
                            const int arg = getArg(emulator, 0, 0, 1);
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
                            const int arg = getArg(emulator, 0, 1, 1);
                            const int lines_after_cursor = bottom_margin - cursor_row;
                            const int lines_to_insert = MIN(arg, lines_after_cursor);
                            const int lines_to_move = lines_after_cursor - lines_to_insert;
                            block_copy(screen, 0, cursor_row, COLUMNS, lines_to_move, 0, cursor_row + lines_to_insert);
                            block_clear(0, cursor_row + lines_to_move, COLUMNS, lines_to_insert);
                        }
                            break;
                        case 'M': {
                            const int arg = getArg(emulator, 0, 1, 1);
                            const int lines_after_cursor = bottom_margin - cursor_row;
                            const int lines_to_delete = MIN(arg, lines_after_cursor);
                            const int lines_to_move = lines_after_cursor - lines_to_delete;
                            about_to_autowrap = false;
                            block_copy(screen, 0, cursor_row + lines_to_delete, COLUMNS, lines_to_move, 0, cursor_row);
                            block_clear(0, cursor_row + lines_to_move, COLUMNS, lines_to_delete);
                        }
                            break;
                        case 'P': {
                            int arg = getArg(emulator, 0, 1, 1);
                            const int cells_after_cursor = COLUMNS - cursor_col;
                            const int cells_to_delete = MIN(arg, cells_after_cursor);
                            const int cells_to_move = cells_after_cursor - cells_to_delete;
                            about_to_autowrap = false;
                            block_copy(screen, cursor_col + cells_to_delete, cursor_row, cells_to_move, 1, cursor_col, cursor_row);
                            block_clear(cursor_col + cells_to_move, cursor_row, cells_to_delete, 1);
                        }
                            break;
                        case 'S':
                            scroll_down(emulator, getArg(emulator, 0, 1, 1));
                            break;
                        case 'T':
                            if (0 == emulator->_csi.index) {
                                const int lines_to_scroll_arg = getArg(emulator, 0, 1, 1);
                                const int lines_between_top_bottom_margin = bottom_margin - top_margin;
                                const int lines_to_scroll = MIN(lines_between_top_bottom_margin, lines_to_scroll_arg);
                                block_copy(screen, 0, top_margin, COLUMNS, lines_between_top_bottom_margin - lines_to_scroll, 0,
                                           top_margin + lines_to_scroll);
                                block_clear(0, top_margin, COLUMNS, lines_to_scroll);
                            } else { finishSequence; }
                            break;
                        case 'X': {
                            const int arg = getArg(emulator, 0, 1, 1);
                            about_to_autowrap = false;
                            block_set(screen, cursor_col, cursor_row, MIN(arg, COLUMNS - cursor_col), 1, (Glyph) {.code=' ', .style= cursor_style});
                        }
                            break;
                        case 'Z': {
                            int number_of_tabs = getArg(emulator, 0, 1, 1);
                            int new_col = left_margin;
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
                        case 'b':
                            if ((unsigned) -1 != emulator->_last_codepoint) {
                                int num_repeat = getArg(emulator, 0, 1, 1);
                                while (0 < num_repeat--)
                                    emit_codepoint(emulator, emulator->_last_codepoint);
                            }

                            break;
                        case 'c':
                            if (0 == getArg(emulator, 0, 0, 1)) write(FD, "\033[?64;1;2;6;9;15;18;21;22c", 26);
                            break;
                        case 'd': {
                            const int arg = getArg(emulator, 0, 1, 1);
                            cursor_row = LIMIT(arg, 1, ROWS) - 1;
                            about_to_autowrap = false;
                        }
                            break;
                        case 'e':
                            set_cursor_position_origin_mode(emulator, cursor_col, cursor_row + getArg(emulator, 0, 1, 1));
                            break;
                        case 'g': {
                            int arg = getArg(emulator, 0, 1, 1);
                            switch (arg) {
                                default:
                                    break;
                                case 0:
                                    emulator->_tabStop[cursor_col / 8] &= ~(1 << (cursor_col & 7));
                                    break;
                                case 3:
                                    memset(emulator->_tabStop, 0, CELI(COLUMNS, 8));
                                    break;
                            }
                        }
                            break;
                        case 'h':
                        case 'l': { //do_set_mode
                            const int arg = getArg(emulator, 0, 0, 1);
                            switch (arg) {
                                case 4:
                                    emulator->MODE_INSERT = codepoint == 'h';
                                    break;
                                case 34:
                                    break;
                                default:
                                    finishSequence;
                                    break;
                            }
                        }
                            break;
                        case 'm'://GRAPHIC RENDITION
                            if (emulator->_csi.index >= MAX_ESCAPE_PARAMETERS) emulator->_csi.index = MAX_ESCAPE_PARAMETERS - 1;
                            for (int i = 0; i < emulator->_csi.index; i++) {
                                int code = emulator->_csi.arg[i];
                                if (0 > code) {
                                    if (0 < emulator->_csi.index)continue;
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
                                    case 30 ... 37:
                                        cursor_style.fg.index = (uint8_t) (code - 30);
                                        break;
                                    case 38:
                                    case 48: {
                                        const int first_arg = emulator->_csi.arg[i + 1];
                                        if (i + 2 > emulator->_csi.index) continue;
                                        switch (first_arg) {
                                            case 2:
                                                if (i + 4 <= emulator->_csi.index) {
                                                    unsigned int red = emulator->_csi.arg[i + 2];
                                                    unsigned int green = emulator->_csi.arg[i + 3];
                                                    unsigned int blue = emulator->_csi.arg[i + 4];
                                                    if (255 < red || 255 < green || 255 < blue)
                                                        finishSequence;
                                                    else {

                                                        if (code == 38) {
                                                            cursor_style.fg = (union color) {{(uint8_t) red, (uint8_t) green, (uint8_t) blue}};
                                                            cursor_style.effect.TURE_COLOR_FG = true;
                                                        } else {
                                                            cursor_style.bg = (union color) {{(uint8_t) red, (uint8_t) green, (uint8_t) blue}};
                                                            cursor_style.effect.TURE_COLOR_BG = true;
                                                        }

                                                    }
                                                    i += 4;
                                                }
                                                break;
                                            case 5: {
                                                int color = emulator->_csi.arg[i + 2];
                                                i += 2;
                                                if (BETWEEN(color, 0, NUM_INDEXED_COLORS)) {
                                                    if (code == 38)
                                                        cursor_style.fg.index = (uint8_t) color;
                                                    else
                                                        cursor_style.bg.index = (uint8_t) color;

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
                                        cursor_style.fg.index = COLOR_INDEX_FOREGROUND;
                                        break;
                                    case 40 ... 47:
                                        cursor_style.bg.index = (uint8_t) (code - 40);
                                        break;
                                    case 49:
                                        cursor_style.bg.index = COLOR_INDEX_BACKGROUND;
                                        break;
                                    case 90 ... 97:
                                        cursor_style.fg.index = (uint8_t) (code - 90 + 8);
                                        break;
                                    case 100 ... 107:
                                        cursor_style.bg.index = (uint8_t) (code - 100 + 8);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case 'n': {
                            int arg = emulator->_csi.arg[0];
                            switch (arg) {
                                default:
                                    break;
                                case 5: {
                                    char bytes[] = {27, '[', '0', 'n'};
                                    write(FD, bytes, 4);
                                }
                                    break;
                                case 6:
                                    dprintf(FD, "\033[%d;%dR", cursor_row + 1, cursor_col + 1);
                                    break;
                            }
                        }
                            break;
                        case 'r': {
                            const int arg0 = emulator->_csi.arg[0] - 1, arg1 = emulator->_csi.arg[1];
                            top_margin = LIMIT(arg0, 0, ROWS - 2);
                            bottom_margin = LIMIT(arg1, top_margin + 2, ROWS);
                            set_cursor_position_origin_mode(emulator, 0, 0);
                        }
                            break;
                        case 's':
                            if (decset_bit.LEFTRIGHT_MARGIN_MODE) {
                                int arg0 = emulator->_csi.arg[0] - 1, arg1 = emulator->_csi.arg[1];
                                left_margin = LIMIT(arg0, 0, COLUMNS - 2);
                                right_margin = LIMIT(arg1, left_margin + 1, COLUMNS);
                                set_cursor_position_origin_mode(emulator, 0, 0);
                            } else save_cursor(emulator);
                            break;
                        case 't': {
                            int arg = emulator->_csi.arg[0];
                            switch (arg) {
                                default:
                                    break;
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
                        case ' ': continue_sequence(ESC_CSI_ARGS_SPACE)
                            break;
                        default:
                            parse_arg(emulator, codepoint);
                            break;
                    }
                    break;
                case ESC_CSI_EXCLAMATION:
                    if ('p' == codepoint) reset_emulator(emulator);
                    else
                        finishSequence;
                    break;
                case ESC_CSI_QUESTIONMARK:
                    switch (codepoint) {
                        case 'J':
                        case 'K': {
                            int arg = getArg(emulator, 0, 0, 0);
                            int start_col = -1, end_col = -1;
                            int start_row = -1, end_row = -1;
                            bool just_row = 'K' == codepoint;
                            Glyph fill = {.code=' ', .style= cursor_style};
                            about_to_autowrap = false;
                            switch (arg) {
                                case 0:
                                    start_col = cursor_col;
                                    start_row = cursor_row;
                                    end_col = COLUMNS;
                                    end_row = just_row ? cursor_row + 1 : ROWS;
                                    break;
                                case 1:
                                    start_col = 0;
                                    start_row = just_row ? cursor_row : 0;
                                    end_col = cursor_col + 1;
                                    end_row = cursor_row + 1;
                                    break;
                                case 2:
                                    start_col = 0;
                                    start_row = just_row ? cursor_row : 0;
                                    end_col = cursor_col + 1;
                                    end_row = just_row ? cursor_row + 1 : ROWS;
                                    break;
                                default:
                                    finishSequence;
                                    break;
                            }
                            for (int row = start_row; row < end_row; row++) {
                                Glyph *const dst = screen->lines[internal_row_buff(screen, row)] + start_col;
                                for (int col = start_col; col < end_col; col++) {
                                    if (dst->style.effect.PROTECTED == 0)
                                        dst[col] = fill;
                                }
                            }
                        }
                            break;
                        case 'h':
                        case 'l': {
                            bool setting = 'h' == codepoint;
                            if (emulator->_csi.index >= MAX_ESCAPE_PARAMETERS)emulator->_csi.index = MAX_ESCAPE_PARAMETERS - 1;
                            for (int i = 0; i < emulator->_csi.index; i++)
                                do_dec_set_or_reset(emulator, setting, emulator->_csi.arg[i]);
                        }
                            break;
                        case 'n':
                            if (6 == emulator->_csi.arg[0])
                                dprintf(FD, "\033[?%d;%d;1R", cursor_row + 1, cursor_col + 1);
                            else
                                finishSequence;
                            break;
                        case 'r':
                        case 's':
                            if (emulator->_csi.index >= MAX_ESCAPE_PARAMETERS) emulator->_csi.index = MAX_ESCAPE_PARAMETERS - 1;
                            for (int i = 0; i < MAX_ESCAPE_PARAMETERS; i++) {
                                int external_bit = emulator->_csi.arg[i];
                                int internal = map_decset_bit(external_bit);
                                if ('s' == codepoint)
                                    emulator->_saved_decset |= internal;
                                else
                                    do_dec_set_or_reset(emulator, emulator->_saved_decset & internal, external_bit);
                            }
                            break;
                        case '$': continue_sequence(ESC_CSI_QUESTIONMARK_ARG_DOLLAR)
                            break;
                        default:
                            parse_arg(emulator, codepoint);
                            break;
                    }
                    break;
                case ESC_CSI_BIGGERTHAN:
                    switch (codepoint) {
                        case 'c':
                            write(FD, "\033[>41;320;0c", 12);
                            break;
                        case 'm':
                            break;
                        default:
                            parse_arg(emulator, codepoint);
                    }
                    break;
                case ESC_CSI_DOLLAR: {
                    bool origin_mode = decset_bit.ORIGIN_MODE;
                    const int effective_top_margin = origin_mode ? top_margin : 0;
                    const int effective_bottom_margin = origin_mode ? bottom_margin : ROWS;
                    const int effective_left_margin = origin_mode ? left_margin : 0;
                    const int effective_right_margin = origin_mode ? right_margin : COLUMNS;
                    switch (codepoint) {
                        case 'v': {
                            int arg0 = getArg(emulator, 0, 1, 1) - 1 + effective_top_margin;
                            int arg1 = getArg(emulator, 1, 1, 1) - 1 + effective_left_margin;
                            int arg2 = getArg(emulator, 2, ROWS, 1) + effective_top_margin;
                            int arg3 = getArg(emulator, 3, COLUMNS, 1) + effective_left_margin;
                            int arg5 = getArg(emulator, 5, 1, 1) - 1 + effective_top_margin;
                            int arg6 = getArg(emulator, 6, 1, 1) - 1 + effective_left_margin;
                            int top_source = MIN(arg0, ROWS);
                            int left_source = MIN(arg1, COLUMNS);
                            int bottom_source = LIMIT(arg2, top_source, ROWS);
                            int right_source = LIMIT(arg3, left_source, COLUMNS);
                            int dest_top = MIN(arg5, ROWS);
                            int dest_left = MIN(arg6, COLUMNS);
                            int height_to_copy = MIN(ROWS - dest_top, bottom_source - top_source);
                            int width_to_copy = MIN(COLUMNS - dest_left, right_source - left_source);
                            block_copy(screen, left_source, top_source, width_to_copy, height_to_copy, dest_left, dest_top);
                        }
                            break;
                        case '{':
                        case 'x':
                        case 'z': {
                            const bool erase = 'x' != codepoint, selective = '{' == codepoint, keep_attributes = erase && selective;
                            int arg_index = 0;
                            int fill_char = erase ? ' ' : getArg(emulator, arg_index++, -1, 1);
                            if (BETWEEN(fill_char, 32, 126) || BETWEEN(fill_char, 160, 255)) {
                                int top = getArg(emulator, arg_index++, 1, 1) + effective_top_margin;
                                int left = getArg(emulator, arg_index++, 1, true) + effective_top_margin;
                                int bottom = getArg(emulator, arg_index++, ROWS, true) + effective_top_margin;
                                int right = getArg(emulator, arg_index, COLUMNS, true) + effective_left_margin;
                                top = MIN(top, effective_bottom_margin + 1);
                                left = MIN(left, effective_right_margin + 1);
                                bottom = MIN(bottom, effective_bottom_margin);
                                right = MIN(right, effective_right_margin);
                                for (int row = top - 1; row < bottom; row++) {
                                    for (int col = left - 1; col < right; col++) {
                                        Glyph *const glyph1 = screen->lines[internal_row_buff(screen, row)] + col;
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

                            int top, left, bottom, right;
                            {
                                int arg0 = getArg(emulator, 0, 1, 1) - 1, arg1 = getArg(emulator, 1, 1, 1) - 1, arg2 =
                                        getArg(emulator, 2, ROWS, 1) + 1, arg3 = getArg(emulator, 3, COLUMNS, 1) + 1;
                                top = MIN(arg0, effective_bottom_margin) + effective_top_margin;
                                left = MIN(arg1, effective_right_margin) + effective_left_margin;
                                bottom = MIN(arg2, effective_bottom_margin - 1) + effective_top_margin;
                                right = MIN(arg3, effective_right_margin - 1) + effective_left_margin;
                            }
                            if (4 <= emulator->_csi.index) {
                                bool set_or_clear;
                                text_effect bits = {0};
                                int arg;
                                if (emulator->_csi.index >= MAX_ESCAPE_PARAMETERS)emulator->_csi.index = MAX_ESCAPE_PARAMETERS - 1;
                                for (int i = 4; i < emulator->_csi.index; i++) {
                                    set_or_clear = true;
                                    arg = emulator->_csi.arg[i];
                                    switch (arg) {
                                        case 0:
                                            if (!reverse)set_or_clear = false;
                                            bits.BOLD = bits.UNDERLINE = bits.BLINK = bits.INVERSE = 1;
                                            break;
                                        case 1:
                                            bits.BOLD = 1;
                                            break;
                                        case 4:
                                            bits.UNDERLINE = 1;
                                            break;
                                        case 5:
                                            bits.BLINK = 1;
                                            break;
                                        case 7:
                                            bits.INVERSE = 1;
                                            break;
                                        case 22:
                                            set_or_clear = false;
                                            bits.BOLD = 1;
                                            break;
                                        case 24:
                                            set_or_clear = false;
                                            bits.UNDERLINE = 1;
                                            break;
                                        case 25:
                                            set_or_clear = false;
                                            bits.BLINK = 1;
                                            break;
                                        case 27:
                                            set_or_clear = false;
                                            bits.INVERSE = 1;
                                            break;
                                        default:
                                            bits = (text_effect) {0};
                                            break;
                                    }

                                    if (!reverse || set_or_clear) {
                                        Term_row line;
                                        unsigned short effect;
                                        int x;
                                        int end_of_line;
                                        for (int y = top; y < bottom; y++) {
                                            line = screen->lines[internal_row_buff(screen, y)];
                                            x = decset_bit.RECTANGULAR_CHANGEATTRIBUTE || y == top ? left : effective_left_margin;
                                            end_of_line = decset_bit.RECTANGULAR_CHANGEATTRIBUTE || y + 1 == bottom ? right : effective_right_margin;
                                            for (; x < end_of_line; x++) {
                                                effect = line[x].style.effect.raw;
                                                if (reverse)
                                                    line[x].style.effect.raw = (effect & ~bits.raw) || (bits.raw & ~effect);
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
                case ESC_CSI_DOUBLE_QUOTE:
                    if ('q' == codepoint) {
                        int arg = getArg(emulator, 0, 0, 0);
                        switch (arg) {
                            case 0:
                            case 2:
                                cursor_style.effect.PROTECTED = 0;
                                break;
                            case 1:
                                cursor_style.effect.PROTECTED = 1;
                                break;
                            default:
                                finishSequence;
                        }
                    } else
                        finishSequence;
                    break;
                case ESC_CSI_SINGLE_QUOTE:
                    switch (codepoint) {
                        case '}': {
                            int columns_after_cursor = right_margin - cursor_col;
                            int arg = getArg(emulator, 0, 1, 1);
                            int columns_to_insert = MIN(arg, columns_after_cursor);
                            int columns_to_move = columns_after_cursor - columns_to_insert;
                            block_copy(screen, cursor_col, 0, columns_to_move, ROWS, cursor_col, 0);
                            block_clear(cursor_col, 0, columns_to_insert, ROWS);
                        }
                            break;
                        case '~': {
                            int columns_after_cursor = right_margin - cursor_col;
                            int arg = getArg(emulator, 0, 1, 1);
                            int columns_to_delete = MIN(arg, columns_after_cursor);
                            int columns_to_move = columns_after_cursor - columns_to_delete;
                            block_copy(screen, cursor_col + columns_to_delete, 0, columns_to_move, ROWS, cursor_col, 0);
                        }
                            break;
                        default:
                            finishSequence;
                            break;
                    }
                    break;
                case ESC_PERCENT:
                    break;
                case ESC_OSC:
                    if (codepoint == 7) //Codepoint 27 handled at the beginning
                        do_osc_set_text_parameters(emulator, "\007");
                    else if (emulator->_osc.len < MAX_OSC_STRING_LENGTH - 4) {
                        emulator->_osc.len += codepoint_to_utf(codepoint, osc_arg + emulator->_osc.len);
                        continue_sequence(esc_state)
                    } else
                        finishSequence;
                    break;
                case ESC_OSC_ESC:
                    if ('\\' == codepoint) do_osc_set_text_parameters(emulator, "\033\\");
                    else {
                        if (emulator->_osc.len < MAX_OSC_STRING_LENGTH - 4) {
                            osc_arg[emulator->_osc.len++] = 27;
                            emulator->_osc.len += codepoint_to_utf(codepoint, osc_arg + emulator->_osc.len);
                            continue_sequence(esc_state)
                        } else
                            finishSequence;
                    }
                    break;
                case ESC_P:
                    //Device Control
                    if ('\\' == codepoint) {
                        if (strncmp(osc_arg, "$q", 2) == 0) {
                            if (strncmp("$q\"p", osc_arg, 4) == 0) write(FD, "\033P1$r64;1\"p\033\\", 13);
                            else
                                finishSequence;
                        } else if (strncmp(osc_arg, "+q", 2) == 0) {
                            char *dcs = osc_arg + 2;
                            int len = 0;
                            bool exit = false;
                            while (1) {
                                len = strchr(dcs, ';') - dcs;
                                if (0 >= len) {
                                    len = (int) strlen(dcs);
                                    exit = true;//last part
                                }
                                if (0 == len % 2) {
                                    char trans_buff[len];
                                    char c;
                                    char *err = NULL;
                                    int trans_buff_len = 0;
                                    char *response = NULL;
                                    size_t response_len;
                                    for (int i = 0; i < len; i += 2) {
                                        c = (char) str_to_hex(dcs + i, &err, 2);
                                        if (err != NULL) continue;
                                        trans_buff[trans_buff_len++] = c;
                                    }
                                    if (strncmp(trans_buff, "Co", 2) == 0 || strncmp(trans_buff, "colors", 6) == 0) {
                                        response = "256";
                                        response_len = 3;
                                    } else if (strncmp(trans_buff, "TN", 2) == 0 || strncmp(trans_buff, "name", 4) == 0) {
                                        response = "xterm";
                                        response_len = 5;
                                    } else
                                        response_len = get_code_from_term_cap(&response, trans_buff, trans_buff_len,
                                                                              decset_bit.APPLICATION_CURSOR_KEYS, decset_bit.APPLICATION_KEYPAD);

                                    if (response_len == 0) {
                                        write(FD, "\033P0+r", 5);
                                        write(FD, dcs, len);
                                        write(FD, "\033\\", 2);
                                    } else {
                                        char hex_encoder[2 * response_len];
                                        for (size_t i = 0; i < response_len; i++)
                                            sprintf(hex_encoder + 2 * i, "%02x", response[i]);
                                        write(FD, "\033P1+r", 5);
                                        write(FD, dcs, len);
                                        write(FD, "=", 1);
                                        write(FD, hex_encoder, response_len * 2);
                                        write(FD, "\033\\", 2);
                                    }
                                    free(response);
                                }
                                if (exit) break;
                                dcs += len + 1;
                            }
                        }
                        finishSequence;
                    } else {
                        if (emulator->_osc.len > MAX_OSC_STRING_LENGTH - 4) {
                            emulator->_osc.len = 0;
                            finishSequence;
                        } else {
                            emulator->_osc.len += codepoint_to_utf(codepoint, osc_arg + emulator->_osc.len);
                            continue_sequence(esc_state)
                        }
                    }
                    break;
                case ESC_CSI_QUESTIONMARK_ARG_DOLLAR:
                    if ('p' == codepoint) {
                        int mode = emulator->_csi.arg[0];
                        int value;
                        switch (mode) {
                            case 47:
                            case 1047:
                            case 1049:
                                value = (screen == &emulator->_alt) ? 1 : 2;
                                break;
                            default: {
                                int internal = map_decset_bit(mode);
                                value = internal == -1 ? 0 : internal & decset_bit.raw ? 1 : 2;
                            }
                                break;
                        }
                        dprintf(FD, "\033[?%d;%d$y", mode, value);
                    } else
                        finishSequence;
                    break;

                case ESC_CSI_ARGS_SPACE:
                    switch (codepoint) {
                        case 'q': {
                            int arg = getArg(emulator, 0, 0, 0);
                            switch (arg) {
                                case 0:
                                case 1:
                                case 2:
                                    emulator->cursor_shape = BLOCK;
                                    break;
                                case 3:
                                case 4:
                                    emulator->cursor_shape = UNDERLINE;
                                    break;
                                case 5:
                                case 6:
                                    emulator->cursor_shape = BAR;
                                    break;
                                default:
                                    break;
                            }
                        }
                            break;
                        case 't':
                        case 'u':
                            break;
                        default:
                            finishSequence;
                            break;
                    }
                    break;
                case ESC_CSI_ARGS_ASTERIX: {
                    int attribute_change_extent = emulator->_csi.arg[0];
                    if ('x' == codepoint && BETWEEN(attribute_change_extent, 0, 2))
                        decset_bit.RECTANGULAR_CHANGEATTRIBUTE = 2 == attribute_change_extent;
                    else
                        finishSequence;
                }
                    break;
                default:
                    finishSequence;
            }
            if (emulator->_csi.dontContinueSequence) finishSequence;
        }
    }
}

Term_emulator *new_term_emulator(const char *restrict const cmd) {
    const int ptm = open("/dev/ptmx", O_RDWR | O_CLOEXEC);
    struct termios tios;
    char devname[64];
    const struct winsize sz = {.ws_row=ROWS, .ws_col=COLUMNS};
    pid_t pid;

    grantpt(ptm);
    unlockpt(ptm);
    ptsname_r(ptm, devname, sizeof(devname));

    // Enable UTF-8 effect and disable flow control to prevent Ctrl+S from locking up the display.

    tcgetattr(ptm, &tios);
    tios.c_iflag |= IUTF8;
    tios.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(ptm, TCSANOW, &tios);


    ioctl(ptm, TIOCSWINSZ, &sz);

    pid = fork();
    if (pid > 0) {
        Term_emulator *restrict const emulator = calloc(1, sizeof(Term_emulator));
        emulator->_tabStop = malloc(CELI(COLUMNS, 8) * sizeof(uint8_t));
        init_term_buffer(&emulator->_main, TRANSCRIPT_ROWS, COLUMNS);
        init_term_buffer(&emulator->_alt, ROWS, COLUMNS);
        screen = &emulator->_main;
        emulator->_last_codepoint = -1;
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
                int _fd = atoi(entry->d_name); // NOLINT(*-err34-c)
                if (_fd > 2 && _fd != self_dir_fd) close(_fd);
            }
            closedir(self_dir);
        }

        putenv("HOME=/data/data/com.termux/files/home");
        putenv("PWD=/data/data/com.termux/files/home");
        putenv("LANG=en_US.UTF-8");
        putenv("PREFIX=/data/data/com.termux/files/usr");
        putenv("PATH=/data/data/com.termux/files/usr/bin");
        putenv("TMPDIR=/data/data/com.termux/files/usr/tmp");
        putenv("COLORTERM=truecolor");
        putenv("TERM=xterm-256color");

        chdir("/data/data/com.termux/files/home/");
        execvp(cmd, NULL);
        _exit(1);
    }
}

#define BUFFER_SIZE 2048

void read_data(Term_emulator *restrict const emulator) {

    static unsigned char buffer[BUFFER_SIZE];
    static int read_len;
    static int write_len;
    static uint8_t utf_buff_len = 0;
    static wchar_t codepoint;
    static uint8_t byte_len;
    do {
        read_len = read(FD, buffer + utf_buff_len, BUFFER_SIZE - utf_buff_len) + utf_buff_len;
        utf_buff_len = 0;
        for (write_len = 0; write_len < read_len;) {
            DECODE_UTF8(buffer, write_len, read_len, codepoint, byte_len)
            write_len += byte_len;
            process_codepoint(emulator, codepoint);
        }
    } while (read_len == BUFFER_SIZE);
}

void destroy_term_emulator(Term_emulator *const emulator) {
    close(FD);
    for (int i = 0; i < ROWS; i++) {
        free(emulator->_main.lines[i]);
        free(emulator->_alt.lines[i]);
    }
    for (int i = ROWS; i < TRANSCRIPT_ROWS; i++)free(emulator->_main.lines[i]);
    free(emulator->_tabStop);
    free(emulator->_main.lines);
    free(emulator->_alt.lines);
    free(emulator);
}

#endif
