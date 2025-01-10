//
// Created by Rohit Paul on 22-12-2024.
//
#include <string.h>
#include "core/color.h"

#define color(c) {.r=((c&0xFF0000)>>16)/255.0f,.g=((c&0x00FF00)>>8)/255.0f,.b=(c&0x0000FF)/255.0f}

static vec3 DEFAULT_COLORSCHEME[NUM_INDEXED_COLORS] = {
        // 16 original colors. First 8 are dim.
        color(0x000000), // black
        color(0xcd0000), // dim red
        color(0x00cd00), // dim green
        color(0xcdcd00), // dim yellow
        color(0x6495ed), // dim blue
        color(0xcd00cd), // dim magenta
        color(0x00cdcd), // dim cyan
        color(0xe5e5e5), // dim white
        // Second 8 are bright:
        color(0x7f7f7f), // medium grey
        color(0xff0000), // bright red
        color(0x00ff00), // bright green
        color(0xffff00), // bright yellow
        color(0x5c5cff), // light blue
        color(0xff00ff), // bright magenta
        color(0x00ffff), // bright cyan
        color(0xffffff), // bright white

        // 216 color cube), six shades of each color:
        color(0x000000), color(0x00005f), color(0x000087), color(0x0000af), color(0x0000d7), color(0x0000ff),
        color(0x005f00), color(0x005f5f), color(0x005f87), color(0x005faf), color(0x005fd7), color(0x005fff),
        color(0x008700), color(0x00875f), color(0x008787), color(0x0087af), color(0x0087d7), color(0x0087ff),
        color(0x00af00), color(0x00af5f), color(0x00af87), color(0x00afaf), color(0x00afd7), color(0x00afff),
        color(0x00d700), color(0x00d75f), color(0x00d787), color(0x00d7af), color(0x00d7d7), color(0x00d7ff),
        color(0x00ff00), color(0x00ff5f), color(0x00ff87), color(0x00ffaf), color(0x00ffd7), color(0x00ffff),
        color(0x5f0000), color(0x5f005f), color(0x5f0087), color(0x5f00af), color(0x5f00d7), color(0x5f00ff),
        color(0x5f5f00), color(0x5f5f5f), color(0x5f5f87), color(0x5f5faf), color(0x5f5fd7), color(0x5f5fff),
        color(0x5f8700), color(0x5f875f), color(0x5f8787), color(0x5f87af), color(0x5f87d7), color(0x5f87ff),
        color(0x5faf00), color(0x5faf5f), color(0x5faf87), color(0x5fafaf), color(0x5fafd7), color(0x5fafff),
        color(0x5fd700), color(0x5fd75f), color(0x5fd787), color(0x5fd7af), color(0x5fd7d7), color(0x5fd7ff),
        color(0x5fff00), color(0x5fff5f), color(0x5fff87), color(0x5fffaf), color(0x5fffd7), color(0x5fffff),
        color(0x870000), color(0x87005f), color(0x870087), color(0x8700af), color(0x8700d7), color(0x8700ff),
        color(0x875f00), color(0x875f5f), color(0x875f87), color(0x875faf), color(0x875fd7), color(0x875fff),
        color(0x878700), color(0x87875f), color(0x878787), color(0x8787af), color(0x8787d7), color(0x8787ff),
        color(0x87af00), color(0x87af5f), color(0x87af87), color(0x87afaf), color(0x87afd7), color(0x87afff),
        color(0x87d700), color(0x87d75f), color(0x87d787), color(0x87d7af), color(0x87d7d7), color(0x87d7ff),
        color(0x87ff00), color(0x87ff5f), color(0x87ff87), color(0x87ffaf), color(0x87ffd7), color(0x87ffff),
        color(0xaf0000), color(0xaf005f), color(0xaf0087), color(0xaf00af), color(0xaf00d7), color(0xaf00ff),
        color(0xaf5f00), color(0xaf5f5f), color(0xaf5f87), color(0xaf5faf), color(0xaf5fd7), color(0xaf5fff),
        color(0xaf8700), color(0xaf875f), color(0xaf8787), color(0xaf87af), color(0xaf87d7), color(0xaf87ff),
        color(0xafaf00), color(0xafaf5f), color(0xafaf87), color(0xafafaf), color(0xafafd7), color(0xafafff),
        color(0xafd700), color(0xafd75f), color(0xafd787), color(0xafd7af), color(0xafd7d7), color(0xafd7ff),
        color(0xafff00), color(0xafff5f), color(0xafff87), color(0xafffaf), color(0xafffd7), color(0xafffff),
        color(0xd70000), color(0xd7005f), color(0xd70087), color(0xd700af), color(0xd700d7), color(0xd700ff),
        color(0xd75f00), color(0xd75f5f), color(0xd75f87), color(0xd75faf), color(0xd75fd7), color(0xd75fff),
        color(0xd78700), color(0xd7875f), color(0xd78787), color(0xd787af), color(0xd787d7), color(0xd787ff),
        color(0xd7af00), color(0xd7af5f), color(0xd7af87), color(0xd7afaf), color(0xd7afd7), color(0xd7afff),
        color(0xd7d700), color(0xd7d75f), color(0xd7d787), color(0xd7d7af), color(0xd7d7d7), color(0xd7d7ff),
        color(0xd7ff00), color(0xd7ff5f), color(0xd7ff87), color(0xd7ffaf), color(0xd7ffd7), color(0xd7ffff),
        color(0xff0000), color(0xff005f), color(0xff0087), color(0xff00af), color(0xff00d7), color(0xff00ff),
        color(0xff5f00), color(0xff5f5f), color(0xff5f87), color(0xff5faf), color(0xff5fd7), color(0xff5fff),
        color(0xff8700), color(0xff875f), color(0xff8787), color(0xff87af), color(0xff87d7), color(0xff87ff),
        color(0xffaf00), color(0xffaf5f), color(0xffaf87), color(0xffafaf), color(0xffafd7), color(0xffafff),
        color(0xffd700), color(0xffd75f), color(0xffd787), color(0xffd7af), color(0xffd7d7), color(0xffd7ff),
        color(0xffff00), color(0xffff5f), color(0xffff87), color(0xffffaf), color(0xffffd7), color(0xffffff),

        // 24 grey scale ramp:
        color(0x080808), color(0x121212), color(0x1c1c1c), color(0x262626), color(0x303030), color(0x3a3a3a),
        color(0x444444), color(0x4e4e4e), color(0x585858), color(0x626262), color(0x6c6c6c), color(0x767676),
        color(0x808080), color(0x8a8a8a), color(0x949494), color(0x9e9e9e), color(0xa8a8a8), color(0xb2b2b2),
        color(0xbcbcbc), color(0xc6c6c6), color(0xd0d0d0), color(0xdadada), color(0xe4e4e4), color(0xeeeeee),

};


__attribute__((always_inline)) void reset_color(vec3 dst[NUM_INDEXED_COLORS], ui8 index) {
    dst[index] = DEFAULT_COLORSCHEME[index];
}

__attribute__((always_inline)) void reset_all_color(vec3 dst[NUM_INDEXED_COLORS]) {
    memcpy(dst, DEFAULT_COLORSCHEME, NUM_INDEXED_COLORS * sizeof(vec3));
}
