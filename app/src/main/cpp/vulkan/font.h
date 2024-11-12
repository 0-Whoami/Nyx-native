//
// Created by Rohit Paul on 11-11-2024.
//

#ifndef NYX_FONT_H
#define NYX_FONT_H

#include <malloc.h>
#include <string.h>
#include "common/utils.h"
#include <android/system_fonts.h>

typedef struct {
    char *data;
    size_t size;
} FONT;

void load_font(FONT *, char *);

void destroy_font(FONT *font);

#ifdef DEBUG

void test_font(void);

#endif

static uint32_t read_be32(const FILE *const file) {
    return (fgetc(file) << 24) | (fgetc(file) << 16) | (fgetc(file) << 8) | fgetc(file);
}

static uint16_t read_be16(const FILE *const file) {
    return (uint16_t) ((fgetc(file) << 8) | fgetc(file));
}

#define SKIP(file, byte) fseek(file,byte,SEEK_CUR)

void load_font(FONT *font, char *font_path) {
    if(strlen(font_path) > 0) {
        FILE *file = fopen(font_path, "rb");
        if(file) {
            uint16_t num_tables;
            //reading the  offset sub-table
            //first 32 bit or 4 byte scalar_type
            SKIP(file, 4);
            // 2 byte num tables
            num_tables = read_be16(file);
            //2 byte search range
            SKIP(file, 2);
            //2 byte entry selector
            SKIP(file, 2);
            //2 byte range shift
            SKIP(file, 2);
            //reading tables
            LOG("#)\ttag\toffset\tlen");
            for(int i = 0; i < num_tables; i++) {
                char tag[4];
                fread(tag,4,1,file);
                uint32_t check_sum = read_be32(file);
                uint32_t offset = read_be32(file);
                uint32_t length = read_be32(file);
                LOG("%d)\t%.4s\t%u\t%u", i + 1, (char *) &tag, offset, length);
            }
            fclose(file);
        }
    }
}

void destroy_font(FONT *font) {

}

#ifdef DEBUG

void test_font(void) {
    FONT font;
    load_font(&font, "/system/fonts/NotoSansInscriptionalPahlavi-Regular.ttf");
}

#endif
#endif //NYX_FONT_H
