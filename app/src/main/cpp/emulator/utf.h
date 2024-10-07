
#include <stdint.h>
#include "utils/utils.h"

#define UTF_SIZ 4
/**
 * Used for invalid data - [...](<a href="http://en.wikipedia.org/wiki/Replacement_character#Replacement_character">...</a>)
 */
#define UNICODE_REPLACEMENT_CHAR  0xFFFD

static const unsigned char utfMask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const unsigned char utfByte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const uint_fast32_t utfMin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static const uint_fast32_t utfMax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static inline uint_fast32_t utf8Byte(const unsigned char c, size_t *const i) {
    for (*i = 0; *i < sizeof(utfMask); ++*i) {
        if ((c & utfMask[*i]) == utfByte[*i]) return c & ~utfMask[*i];
    }
    return 0;
}

static inline size_t utf8validate(uint_fast32_t *const u, size_t i) {
    if (!BETWEEN(*u, utfMin[i], utfMax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
        *u = UNICODE_REPLACEMENT_CHAR;
    for (i = 1; *u > utfMax[i]; ++i);

    return i;
}
/**
 * Decode utf code from char array
 * @param b char array to decode from
 * @param c pointer variable where to decode
 * @param _len length to read from array
 * @returns length of utf char
 * */
size_t utf_decode(const char *b, uint_fast32_t *const c, const size_t _len) {
    size_t i, j, len, type;
    uint_fast32_t unDecoded;

    *c = UNICODE_REPLACEMENT_CHAR;
    if (!_len)
        return 0;
    unDecoded = utf8Byte(b[0], &len);
    if (!BETWEEN(len, 1, UTF_SIZ))
        return 1;
    for (i = 1, j = 1; i < _len && j < len; ++i, ++j) {
        unDecoded = (unDecoded << 6) | utf8Byte(b[i], &type);
        if (type != 0)
            return j;
    }
    if (j < len)
        return 0;
    *c = unDecoded;
    utf8validate(c, len);
    return len;
}
