//
// Created by Rohit Paul on 18-11-2024.
//

#ifndef NYX_RENDERER_H
#define NYX_RENDERER_H
#include "terminal_data_types.h"
#include <jni.h>
#include <android/asset_manager.h>
#include <android/native_window.h>
#include "ext/graphics/vulkan_layer/vk_wrapper.h"


typedef struct {
    bool round_screen;
    i8 rows, columns;
} Screen_info;

typedef struct {
    ANativeWindow *window;
    shader_info vertex_shader;
    shader_info fragment_shader;
    Screen_info screen_info;
} Renderer_info;

void create_renderer(const Renderer_info *info);

void destroy_renderer(void);

#endif //NYX_RENDERER_H
