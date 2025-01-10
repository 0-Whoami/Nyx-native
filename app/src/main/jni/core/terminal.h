//
// Created by Rohit Paul on 13-11-2024.
//

#ifndef NYX_TERMINAL_H
#define NYX_TERMINAL_H

#include <sys/types.h>
#include <termios.h>
#include <stdbool.h>
#include "terminal_data_types.h"

enum t_fx {
    BOLD = 1 << 0,
    DIM = 1 << 1,
    ITALIC = 1 << 2,
    UNDERLINE = 1 << 3,
    INVERSE = 1 << 4,
    INVISIBLE = 1 << 5,
    STRIKETHROUGH = 1 << 6,
    PROTECTED = 1 << 7
};

enum mouse_button {
    MOUSE_LEFT_BUTTON_MOVED = 32, MOUSE_WHEEL_UP_BUTTON = 64, MOUSE_WHEEL_DOWN_BUTTON = 65
};

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


typedef struct {
    Glyph glyph;
    i8 x, y;
    uint16_t decsetBit;
} term_cursor;

#define MAX_ESCAPE_PARAMETERS 16
#define MAX_OSC_STRING_LENGTH 2048

typedef struct {
    int fd;
    pid_t pid;
    vec3 *colors;
    term_cursor cursor, saved_state, saved_state_alt;
    void *main, *alt, *screen_buff;
    ui8 *tabStop;
    char last_codepoint;
    i8 leftMargin, rightMargin, topMargin, bottomMargin;
    i8 rows, columns;
    char osc_args[MAX_OSC_STRING_LENGTH + 4];
    int16_t osc_len;
    short csi_args[MAX_ESCAPE_PARAMETERS];
    ui8 esc_state; //MSB is do_not_continue_sequence and 5 lsb is state
    i8 csi_index;
    u_short aShort; //1st msb is abt to autowrap 2nd msb is mode_insert 13 lsb are saved decset
    void *ptr;
} terminal;

typedef struct {
    struct winsize sz;
    char *const cmd;
    char *const cwd;
    char *const *const env;
    size_t env_len;
    char *const *const argv;
} terminal_session_info;

extern const Glyph NORMAL;

terminal *new_terminal(const terminal_session_info *restrict);

void send_mouse_event(const terminal *restrict, i8 mouse_button, i8 x, i8 y, bool pressed);

void destroy_term_emulator(terminal *);

#endif //NYX_TERMINAL_H
