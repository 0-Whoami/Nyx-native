#ifndef UTILS
#define UTILS

#include <android/log.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, low, high) (x >= low && x <= high)
#define LIMIT(x, a, b) (x) < (a) ? (a) : (x) > (b) ? (b) : (x)

#define LOG(...)  __android_log_print(ANDROID_LOG_INFO, "Native", __VA_ARGS__)

int str_to_hex(char *restrict string, char **err, size_t len);

size_t codepoint_to_utf8(uint_least32_t codepoint, char *buffer);

inline double square(double d);

inline double square(const double d) {
    return d * d;
}

int str_to_hex(char *restrict const string, char **err, size_t len) {
    char c;
    int result = 0;
    for (unsigned int i = 0; i < len; i++) {
        c = string[i];
        if (BETWEEN(c, '0', '9')) c -= '0';
        else if (BETWEEN(c, 'a', 'f')) c -= ('a' - 10);
        else if (BETWEEN(c, 'A', 'F')) c -= ('A' - 10);
        else {
            if (err)
                *err = string + i;
            return 0;
        }
        result = result * 16 + c;
    }
    return result;
}

size_t codepoint_to_utf8(const uint_least32_t codepoint, char *buffer) {
    if (codepoint <= 0x7F) {
        // 1-byte sequence: U+0000 to U+007F
        buffer[0] = codepoint;
        buffer[1] = '\0';
        return 1;
    } else if (codepoint <= 0x7FF) {
        // 2-byte sequence: U+0080 to U+07FF
        buffer[0] = 0xC0 | ((codepoint >> 6) & 0x1F);
        buffer[1] = 0x80 | (codepoint & 0x3F);
        buffer[2] = '\0';
        return 2;
    } else if (codepoint <= 0xFFFF) {
        // 3-byte sequence: U+0800 to U+FFFF
        buffer[0] = 0xE0 | ((codepoint >> 12) & 0x0F);
        buffer[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[2] = 0x80 | (codepoint & 0x3F);
        buffer[3] = '\0';
        return 3;
    } else if (codepoint <= 0x10FFFF) {
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

#endif
