//
// Created by Rohit Paul on 17-11-2024.
//

#ifndef NYX_VK_WRAPPER_H
#define NYX_VK_WRAPPER_H


#include "common/common.h"
#include <vulkan/vulkan.h>

typedef struct {
    uint32_t *code;
    size_t length;
} shader_info;

typedef struct {
    struct ANativeWindow *window;
    shader_info vertex_shader, fragment_shader;
} Vulkan_init_info;

void init_vulkan(Vulkan_init_info *);

void start_drawing(void);

void end_drawing(void);

void cleanup_vulkan(void);

#ifdef DEBUG

void test_drawing(void);

#endif

#endif //NYX_VK_WRAPPER_H
