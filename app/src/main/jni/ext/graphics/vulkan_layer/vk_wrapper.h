//
// Created by Rohit Paul on 17-11-2024.
//

#ifndef NYX_VK_WRAPPER_H
#define NYX_VK_WRAPPER_H


#include "common/common.h"
#include <vulkan/vulkan.h>
#include <android/native_window.h>

typedef struct {
    uint32_t *code;
    size_t length;
} shader_info;

typedef struct {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice gpu;
} Vk_Context;

typedef struct {
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkExtent2D display_size;
    uint32_t image_count;
    VkImageView *image_views;
} Vk_display;

typedef struct {
    VkQueue queue;
    VkRenderPass render_pass;
    VkFramebuffer *frame_buffer;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
} Vk_draw;

typedef struct {
    shader_info vertex_shader, fragment_shader;
    VkVertexInputBindingDescription *binding_description;
    uint32_t binding_description_len;
    VkVertexInputAttributeDescription *attribute_description;
    uint32_t attribute_description_len;
    VkDescriptorSetLayout *layouts;
    uint32_t layout_count;
    VkPushConstantRange *push_constant_ranges;
    uint32_t push_constant_ranges_len;
} Vk_pipeline_info;

typedef struct {
    Vk_pipeline_info *pipelineInfo;
    uint32_t queue_index;
    VkFormat format;
    Vk_display *display;
} Vk_Draw_Info;
typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
} Vk_buffer;
typedef struct {
    Vk_buffer *buffer;
    size_t size;
    VkBufferUsageFlags usage_flags;
    VkMemoryPropertyFlags memory_property_flags;
} Vk_Buffer_Info;

/**
 * @brief Create Vulkan Context.
 *
 * @param context pointer to object.
 * @param queue_flags Vulkan Queue flags (VK_QUEUE_GRAPHICS_BIT etc)
 *
 * @return The suitable queue index.
 */
uint32_t create_vk_context(Vk_Context *context, VkQueueFlags queue_flags);

VkFormat create_vk_display(Vk_Context *, ANativeWindow *, Vk_display *);

void create_vk_draw(Vk_Context *, Vk_Draw_Info *, Vk_draw *);

uint32_t find_mem_index(VkPhysicalDevice gpu, uint32_t mem_type, VkMemoryPropertyFlags req_properties);

void create_command_buffer(VkDevice device, VkCommandPool command_pool, VkCommandBuffer *command_buffer);

void create_buffer(Vk_Context *, Vk_Buffer_Info *);

void create_image_view(VkDevice vk_device, VkImageView *image_view, const VkImage *image, VkFormat vk_format);

void destroy_vk_context(Vk_Context *);

void destroy_vk_display(Vk_Context *, Vk_display *);

void destroy_vk_draw(Vk_Context *, Vk_draw *, uint32_t image_count);

#ifdef DEBUG
#define VK_CHECK(fun) if(fun!=VK_SUCCESS){ LOG("VULKAN ERROR IN LINE %d : %s code : %d\n",__LINE__, #fun,fun);}
#else
#define VK_CHECK(fun) fun;
#endif

#endif //NYX_VK_WRAPPER_H
