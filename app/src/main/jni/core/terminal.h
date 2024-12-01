//
// Created by Rohit Paul on 13-11-2024.
//

#ifndef NYX_TERMINAL_H
#define NYX_TERMINAL_H

#include <sys/types.h>
#include <stdbool.h>
#include "core/color.h"

enum t_fx {
    BOLD = 1 << 0,
    DIM = 1 << 1,
    ITALIC = 1 << 2,
    UNDERLINE = 1 << 3,
    BLINK = 1 << 4,
    INVERSE = 1 << 5,
    INVISIBLE = 1 << 6,
    STRIKETHROUGH = 1 << 7,
    PROTECTED = 1 << 8,
    TRUE_COLOR_FG = 1 << 9,
    TRUE_COLOR_BG = 1 << 10,
    WIDE = 1 << 11,
    AUTO_WRAPPED = 1 << 12,
};

enum mouse_button {
    MOUSE_LEFT_BUTTON_MOVED = 32, MOUSE_WHEEL_UP_BUTTON = 64, MOUSE_WHEEL_DOWN_BUTTON = 65
};

typedef int_fast8_t i8;
typedef uint_fast8_t ui8;

typedef struct {
    union {
        color color;
        uint8_t index;
    } fg, bg;
    u_short effect;    //Text effect
} glyph_style;

typedef struct {
    glyph_style style;      //Style
    int code;     //Character
} Glyph;

struct render_data {
    int16_t top_row;

    void (*on_start_drawing)(void);

    void (*fill_color)(color);

    void (*draw_glyph)(Glyph, i8 x, i8 y);

    void (*draw_cell)(i8 x, i8 y, float w, float h, color color);

    void (*on_end_drawing)(void);
};

typedef struct terminal_emulator terminal;

struct terminal_config {
    int16_t transcript_rows;
    struct render_data *render_conf;

    size_t (*get_code_from_term_cap)(char **write_onto, const char *string, size_t len, bool cursorApp, bool keypadApp);

    void (*copy_to_clipboard)(char *text);
};

struct terminal_session_info {
    int8_t rows, columns;
    uint8_t cell_width, cell_height;
    char *const cmd;
    char *const cwd;
    char *const *const env;
    size_t env_len;
    char *const *const argv;
};

void create_terminal_environment(struct terminal_config *);

terminal *new_terminal(const struct terminal_session_info *restrict);

pid_t get_pid(const terminal *);

int get_fd(const terminal *);

void read_data(terminal *);

void render_terminal(const terminal *);

void send_mouse_event(const terminal *restrict, i8 mouse_button, i8 x, i8 y, bool pressed);

char *get_terminal_text(const terminal *restrict, int x1, int y1, int x2, int y2);

void destroy_term_emulator(terminal *);

#endif //NYX_TERMINAL_H
