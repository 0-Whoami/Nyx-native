#ifndef TERM_STYLE
#define TERM_STYLE

#include "terminal_data_types.h"

#define COLOR_INDEX_FOREGROUND  15
#define COLOR_INDEX_BACKGROUND 0
#define COLOR_INDEX_CURSOR  15
#define NUM_INDEXED_COLORS  256

/**
 * [...](<a href="http://upload.wikimedia.org/wikipedia/en/1/15/Xterm_256color_chart.svg">...</a>)), but with blue color brighter.
 */

void reset_color(vec3 dst[NUM_INDEXED_COLORS], ui8 index);

void reset_all_color(vec3 dst[NUM_INDEXED_COLORS]);

#endif //TERM_STYLE
