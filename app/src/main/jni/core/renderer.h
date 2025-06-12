//
// Created by Rohit Paul on 18-11-2024.
//

#ifndef NYX_RENDERER_H
#define NYX_RENDERER_H
#include "terminal_data_types.h"
#include <jni.h>
#include <android/asset_manager.h>
#include <android/native_window.h>


typedef struct {
    bool round_screen;
    i8 rows, columns;
} Screen_info;

void destroy_renderer(void);

#endif //NYX_RENDERER_H
