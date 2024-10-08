#define BETWEEN(x, a, b)    ((a) <= (x) && (x) <= (b))
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) < (b) ? (b) : (a))
#define LEN(a)            (sizeof(a) / sizeof(a)[0])
#define DIVCEIL(n, d)        (((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)        (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)         (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define MODBIT(x, set, bit)    ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

char codepoint_to_utf8(int codepoint, char *buffer) {
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