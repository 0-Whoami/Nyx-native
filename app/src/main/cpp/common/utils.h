#ifndef UTILS
#define UTILS

#include <android/log.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, low, high) (x >= low && x <= high)
#define LIMIT(x, a, b) (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define CELI(a, b) ((a+b-1)/b)
#define LOG(...)  __android_log_print(ANDROID_LOG_INFO, "Native", __VA_ARGS__)

int str_to_hex(char *restrict, char **, size_t);

int8_t codepoint_to_utf(uint_least32_t, char *);

extern inline double square(double);

unsigned int map_search(unsigned int, unsigned int, const unsigned int map[][2], size_t);

inline double square(const double d) {
    return d * d;
}

unsigned int map_search(const unsigned int key, const unsigned int default_val, const unsigned int map[][2], size_t len) {
    size_t low = 0, mid;
    while (low <= len) {
        mid = low + (len - low) / 2;
        if (map[mid][0] == key) return map[mid][1];
        else if (map[mid][0] < len) low = mid + 1;
        else len = mid - 1;
    }
    return default_val;
}

int str_to_hex(char *restrict const string, char **err, size_t len) {
    char c;
    int result = 0;
    for (unsigned int i = 0; i < len; i++) {
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
                    *err = string + i;
                return 0;
        }
        result = result * 16 + c;
    }
    return result;
}

#define ASCII_MAX      0b01111111 //0xxxxxxx
#define UTF_2_BYTE_MAX 0b11011111 //110xxxxx
#define UTF_3_BYTE_MAX 0b11101111 //1110xxxx
#define UTF_4_BYTE_MAX 0b11110111 //11110xxx

int8_t codepoint_to_utf(uint_least32_t codepoint, char *buffer) {
    if (codepoint <= ASCII_MAX) {
        // 1-byte sequence: U+0000 to U+007F
        buffer[0] = codepoint;
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

#define DECODE_UTF8(buffer, pos, len, cp, bl) {                                \
    bl = 1;                                                                    \
    cp = buffer[pos];                                                          \
    if (cp > ASCII_MAX) {                                                      \
        bl = cp > UTF_2_BYTE_MAX ? cp > UTF_3_BYTE_MAX ? 4 : 3 : 2;            \
        if (pos + bl > len) {                                                  \
            utf_buff_len = bl;                                                 \
            memcpy(buffer, buffer + pos, utf_buff_len);                        \
            break;                                                             \
        }                                                                      \
        if (cp > UTF_4_BYTE_MAX)                                               \
            cp = UNICODE_REPLACEMENT_CHARACTER;                                \
        else {                                                                 \
            cp = (cp & (0xFF >> bl)) << ((bl - 1) * 6);                        \
            for (int i = 1; i < bl; i++)                                       \
                cp |= (buffer[pos + i] & 0x3F) << ((bl - i - 1) * 6);          \
        }                                                                      \
    }                                                                          \
}
#endif
