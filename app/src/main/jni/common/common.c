//
// Created by Rohit Paul on 12-12-2024.
//
//static unsigned char append_codepoint(const int codepoint, char *const buffer) {
//
//    if (codepoint <= 127) {
//        // 1-byte sequence: U+0000 to U+007F
//        buffer[0] = (char) codepoint;
//        buffer[1] = '\0';
//        return 1;
//    } else if (codepoint <= 0x07FF) {
//        // 2-byte sequence: U+0080 to U+07FF
//        buffer[0] = 0xC0 | ((codepoint >> 6) & 0x1F);
//        buffer[1] = 0x80 | (codepoint & 0x3F);
//        buffer[2] = '\0';
//        return 2;
//    } else if (codepoint <= 0xFFFF) {
//        // 3-byte sequence: U+0800 to U+FFFF
//        buffer[0] = 0xE0 | ((codepoint >> 12) & 0x0F);
//        buffer[1] = 0x80 | ((codepoint >> 6) & 0x3F);
//        buffer[2] = 0x80 | (codepoint & 0x3F);
//        buffer[3] = '\0';
//        return 3;
//    } else if (codepoint <= 0x10FFFF) {
//        // 4-byte sequence: U+10000 to U+10FFFF
//        buffer[0] = 0xF0 | ((codepoint >> 18) & 0x07);
//        buffer[1] = 0x80 | ((codepoint >> 12) & 0x3F);
//        buffer[2] = 0x80 | ((codepoint >> 6) & 0x3F);
//        buffer[3] = 0x80 | (codepoint & 0x3F);
//        buffer[4] = '\0';
//        return 4;
//    }
//    // Invalid codepoint, return 0
//    return 0;
//}
//char *get_terminal_text(const terminal *restrict const emulator, int x1, int y1, int x2, int y2) {
//    int text_size;
//    size_t index = 0;
//    char *text;
//    Term_row row;
//    x1 = LIMIT(x1, 0, COLUMNS);
//    x2 = LIMIT(x2, x1, COLUMNS);
//    y1 = LIMIT(y1, -SCREEN_BUFF->history_rows, is_alt_buff_active() ? ROWS : TRANSCRIPT_ROWS);
//    y2 = LIMIT(y2, y1, is_alt_buff_active() ? ROWS : TRANSCRIPT_ROWS);
//    text_size = ((COLUMNS - x1) + ((y2 - y1 - 1) * COLUMNS) + x2) + 1;
//    text = malloc((unsigned int) text_size * 4 * sizeof(char));
//    row = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y1)];
//    for (; x1 < COLUMNS; x1++)
//        index += append_codepoint(row[x1].code, text + index);
//    for (; y1 < y2 - 1; y1++) {
//        row = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y1)];
//        for (int x = 0; x < COLUMNS; x++) index += append_codepoint(row[x].code, text + index);
//    }
//    row = SCREEN_BUFF->lines[internal_row_buff(SCREEN_BUFF, y2)];
//    for (int x = 0; x < x2; x++) index += append_codepoint(row[x].code, text + index);
//    text = realloc(text, (index + 1) * sizeof(char));
//    return text;
//}