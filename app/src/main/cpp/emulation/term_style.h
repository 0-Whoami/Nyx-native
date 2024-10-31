#ifndef TERM_STYLE
#define TERM_STYLE

#include <stdlib.h>
#include <string.h>


#define COLOR_INDEX_FOREGROUND  15
#define COLOR_INDEX_BACKGROUND 0
#define COLOR_INDEX_CURSOR  15
#define NUM_INDEXED_COLORS  256

typedef union {
    struct {
        bool BOLD: 1;
        bool ITALIC: 1;
        bool UNDERLINE: 1;
        bool BLINK: 1;
        bool INVERSE: 1;
        bool INVISIBLE: 1;
        bool STRIKETHROUGH: 1;
        bool PROTECTED: 1;
        bool DIM: 1;
        bool TURE_COLOR_FG: 1;
        bool TURE_COLOR_BG: 1;
        bool WIDE: 1;
        bool AUTO_WRAPPED: 1;
    };
    unsigned short raw;
} text_effect;

typedef struct {
    union color {
        struct {
            uint8_t r, g, b;
        };
        uint8_t index;
    } fg, bg;
    text_effect effect;    //Text effect
} glyph_style;
/**
 * Character of terminal with metadata
 * */
typedef struct {
    glyph_style style;      //Style
    uint_least32_t code;     //Character
} Glyph;

const glyph_style NORMAL = {.fg={.index=COLOR_INDEX_FOREGROUND}, .bg={.index=COLOR_INDEX_BACKGROUND}, .effect={0}};

void parse_color_to_index(uint_least32_t colors[static NUM_INDEXED_COLORS], unsigned int index, char *color_string, size_t len);

/**
 * [...](<a href="http://upload.wikimedia.org/wikipedia/en/1/15/Xterm_256color_chart.svg">...</a>), but with blue color brighter.
 */
static uint_least32_t DEFAULT_COLORSCHEME[NUM_INDEXED_COLORS] = {
        // 16 original colors. First 8 are dim.
        0xff000000, // black
        0xffcd0000, // dim red
        0xff00cd00, // dim green
        0xffcdcd00, // dim yellow
        0xff6495ed, // dim blue
        0xffcd00cd, // dim magenta
        0xff00cdcd, // dim cyan
        0xffe5e5e5, // dim white
        // Second 8 are bright:
        0xff7f7f7f, // medium grey
        0xffff0000, // bright red
        0xff00ff00, // bright green
        0xffffff00, // bright yellow
        0xff5c5cff, // light blue
        0xffff00ff, // bright magenta
        0xff00ffff, // bright cyan
        0xffffffff, // bright white

        // 216 color cube, six shades of each color:
        0xff000000, 0xff00005f, 0xff000087, 0xff0000af, 0xff0000d7, 0xff0000ff, 0xff005f00, 0xff005f5f, 0xff005f87, 0xff005faf, 0xff005fd7,
        0xff005fff, 0xff008700, 0xff00875f, 0xff008787, 0xff0087af, 0xff0087d7, 0xff0087ff, 0xff00af00, 0xff00af5f, 0xff00af87, 0xff00afaf,
        0xff00afd7, 0xff00afff, 0xff00d700, 0xff00d75f, 0xff00d787, 0xff00d7af, 0xff00d7d7, 0xff00d7ff, 0xff00ff00, 0xff00ff5f, 0xff00ff87,
        0xff00ffaf, 0xff00ffd7, 0xff00ffff, 0xff5f0000, 0xff5f005f, 0xff5f0087, 0xff5f00af, 0xff5f00d7, 0xff5f00ff, 0xff5f5f00, 0xff5f5f5f,
        0xff5f5f87, 0xff5f5faf, 0xff5f5fd7, 0xff5f5fff, 0xff5f8700, 0xff5f875f, 0xff5f8787, 0xff5f87af, 0xff5f87d7, 0xff5f87ff, 0xff5faf00,
        0xff5faf5f, 0xff5faf87, 0xff5fafaf, 0xff5fafd7, 0xff5fafff, 0xff5fd700, 0xff5fd75f, 0xff5fd787, 0xff5fd7af, 0xff5fd7d7, 0xff5fd7ff,
        0xff5fff00, 0xff5fff5f, 0xff5fff87, 0xff5fffaf, 0xff5fffd7, 0xff5fffff, 0xff870000, 0xff87005f, 0xff870087, 0xff8700af, 0xff8700d7,
        0xff8700ff, 0xff875f00, 0xff875f5f, 0xff875f87, 0xff875faf, 0xff875fd7, 0xff875fff, 0xff878700, 0xff87875f, 0xff878787, 0xff8787af,
        0xff8787d7, 0xff8787ff, 0xff87af00, 0xff87af5f, 0xff87af87, 0xff87afaf, 0xff87afd7, 0xff87afff, 0xff87d700, 0xff87d75f, 0xff87d787,
        0xff87d7af, 0xff87d7d7, 0xff87d7ff, 0xff87ff00, 0xff87ff5f, 0xff87ff87, 0xff87ffaf, 0xff87ffd7, 0xff87ffff, 0xffaf0000, 0xffaf005f,
        0xffaf0087, 0xffaf00af, 0xffaf00d7, 0xffaf00ff, 0xffaf5f00, 0xffaf5f5f, 0xffaf5f87, 0xffaf5faf, 0xffaf5fd7, 0xffaf5fff, 0xffaf8700,
        0xffaf875f, 0xffaf8787, 0xffaf87af, 0xffaf87d7, 0xffaf87ff, 0xffafaf00, 0xffafaf5f, 0xffafaf87, 0xffafafaf, 0xffafafd7, 0xffafafff,
        0xffafd700, 0xffafd75f, 0xffafd787, 0xffafd7af, 0xffafd7d7, 0xffafd7ff, 0xffafff00, 0xffafff5f, 0xffafff87, 0xffafffaf, 0xffafffd7,
        0xffafffff, 0xffd70000, 0xffd7005f, 0xffd70087, 0xffd700af, 0xffd700d7, 0xffd700ff, 0xffd75f00, 0xffd75f5f, 0xffd75f87, 0xffd75faf,
        0xffd75fd7, 0xffd75fff, 0xffd78700, 0xffd7875f, 0xffd78787, 0xffd787af, 0xffd787d7, 0xffd787ff, 0xffd7af00, 0xffd7af5f, 0xffd7af87,
        0xffd7afaf, 0xffd7afd7, 0xffd7afff, 0xffd7d700, 0xffd7d75f, 0xffd7d787, 0xffd7d7af, 0xffd7d7d7, 0xffd7d7ff, 0xffd7ff00, 0xffd7ff5f,
        0xffd7ff87, 0xffd7ffaf, 0xffd7ffd7, 0xffd7ffff, 0xffff0000, 0xffff005f, 0xffff0087, 0xffff00af, 0xffff00d7, 0xffff00ff, 0xffff5f00,
        0xffff5f5f, 0xffff5f87, 0xffff5faf, 0xffff5fd7, 0xffff5fff, 0xffff8700, 0xffff875f, 0xffff8787, 0xffff87af, 0xffff87d7, 0xffff87ff,
        0xffffaf00, 0xffffaf5f, 0xffffaf87, 0xffffafaf, 0xffffafd7, 0xffffafff, 0xffffd700, 0xffffd75f, 0xffffd787, 0xffffd7af, 0xffffd7d7,
        0xffffd7ff, 0xffffff00, 0xffffff5f, 0xffffff87, 0xffffffaf, 0xffffffd7, 0xffffffff,

        // 24 grey scale ramp:
        0xff080808, 0xff121212, 0xff1c1c1c, 0xff262626, 0xff303030, 0xff3a3a3a, 0xff444444, 0xff4e4e4e, 0xff585858, 0xff626262, 0xff6c6c6c,
        0xff767676, 0xff808080, 0xff8a8a8a, 0xff949494, 0xff9e9e9e, 0xffa8a8a8, 0xffb2b2b2, 0xffbcbcbc, 0xffc6c6c6, 0xffd0d0d0, 0xffdadada,
        0xffe4e4e4, 0xffeeeeee,

};

#define reset_color(index) emulator->colors[index] = DEFAULT_COLORSCHEME[index];

#define reset_all_color() memcpy(emulator->colors, DEFAULT_COLORSCHEME, sizeof(int_least32_t) * NUM_INDEXED_COLORS);


void parse_color_to_index(uint_least32_t colors[static NUM_INDEXED_COLORS], const unsigned int index, char *color_string, const size_t len) {
    bool skip_between = 0;
    size_t chars_for_colors, component_length;
    double mult;
    uint_least32_t col[3];
    char *end_ptr = NULL;
    if ('#' == color_string[0]) color_string++;
    else if (strncmp(color_string, "rgb:", 4) == 0) {
        color_string += 4;
        skip_between = 1;
    } else return;
    chars_for_colors = len - 2 * skip_between;
    if (0 != chars_for_colors % 3) return;
    component_length = chars_for_colors / 3;
    mult = 255 / square((component_length >> 2) - 1);
    for (int i = 0; i < 3; i++) {
        col[i] = (unsigned int) (str_to_hex(color_string, &end_ptr, component_length) * mult);
        if (end_ptr != NULL) return;
        color_string += skip_between + component_length;
    }
    colors[index] = 0xFF >> 24 | (col[0] >> 16) | (col[1] >> 8) | col[2];
}

#endif //TERM_STYLE
