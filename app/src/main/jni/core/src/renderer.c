/*
//
// Created by Rohit Paul on 18-11-2024.
//

#include "core/renderer.h"
#include <malloc.h>
#include "ext/jni/jni_wrapper.h"
#include "ext/graphics/vulkan_layer/vk_wrapper.h"
#include <string.h>

typedef struct {
    vec2 offset;
    float zoom;
} uniform_data;

static Vk_Context context;
static Vk_display display;
static Vk_draw draw;

static Vk_buffer buffer;

static VkImage text_atlas = VK_NULL_HANDLE;
static VkDeviceMemory texture_memory = VK_NULL_HANDLE;
static VkImageView image_view = VK_NULL_HANDLE;

static VkSampler sampler;
static VkDescriptorSetLayout descriptor_set_layout;

static VkSemaphore image_acquire_semaphore;
static VkSubmitInfo submit_info = {.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount=1};

static uint32_t image_index;
static VkCommandBufferBeginInfo begin_info = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
const static VkPresentInfoKHR present_info = {.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .swapchainCount=1, .pSwapchains=&display.swapchain, .waitSemaphoreCount=1, .pWaitSemaphores=&image_acquire_semaphore, .pImageIndices=&image_index};
static float cell_width, cell_height;
static uint32_t total_cells_count;

static void read_shader(AAssetManager *aAssetManager, char *name, shader_info *shader_info) {
    AAsset *asset = AAssetManager_open(aAssetManager, name, AASSET_MODE_BUFFER);
    shader_info->length = (size_t) AAsset_getLength(asset);
    shader_info->code = malloc(shader_info->length);
    AAsset_read(asset, shader_info->code, shader_info->length);
    AAsset_close(asset);
}

static void end_cmd_buffer(VkCommandBuffer cmd_buffer) {
    submit_info.pCommandBuffers = &cmd_buffer;
    vkEndCommandBuffer(cmd_buffer);
    vkQueueSubmit(draw.queue, 1, &submit_info, VK_NULL_HANDLE);
}

static VkCommandBuffer single_time_cmd_buffer(void) {
    VkCommandBuffer commandBuffer;
    create_command_buffer(context.device, draw.command_pool, &commandBuffer);
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &begin_info);
    return commandBuffer;
}

static void end_single_time_cmd_buffer(VkCommandBuffer cmd_buffer) {
    end_cmd_buffer(cmd_buffer);
    vkQueueWaitIdle(draw.queue);
    vkFreeCommandBuffers(context.device, draw.command_pool, 1, &cmd_buffer);
}

static void setup_staging_buffer(Vk_buffer *vk_buffer, uint size) {
    const Vk_Buffer_Info staging_info = {
            .size=size, .buffer=vk_buffer,
            .usage_flags=VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory_property_flags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    create_buffer(&context, &staging_info);
}

static void free_buffer(const Vk_buffer *const vk_buffer) {
    vkDestroyBuffer(context.device, vk_buffer->buffer, NULL);
    vkFreeMemory(context.device, vk_buffer->memory, NULL);
}

static void free_textures(void) {
    vkDestroyImage(context.device, text_atlas, NULL);
    vkDestroyImageView(context.device, image_view, NULL);
    vkFreeMemory(context.device, texture_memory, NULL);
}

static void create_buffers(Screen_info info) {
    const uint8_t index_data[] = {0, 1, (uint8_t) (info.columns + 1), (uint8_t) (info.columns + 1), (uint8_t) (info.columns + 2), 1};
    const float cell_w = 2.0f / (float) info.columns, cell_h = 2.0f / (float) info.rows;
    vec2 *data;
    Vk_buffer staging_buffer;
    const Vk_Buffer_Info bufferInfo = {
            .buffer=&buffer,
            .size=(total_cells_count * sizeof(vec2)) + (sizeof(uint8_t) * 6),
            .usage_flags=VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .memory_property_flags=  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    VkCommandBuffer cmd_buffer = single_time_cmd_buffer();
    create_buffer(&context, &bufferInfo);
    setup_staging_buffer(&staging_buffer, bufferInfo.size);
    vkMapMemory(context.device, staging_buffer.memory, 0, bufferInfo.size, 0, (void **) &data);
    for (int y = 0; y < info.rows; y++) {
        for (int x = 0; x < info.columns; x++) {
            data->x = -1 + ((float) x * cell_w);
            data->y = -1 + ((float) y * cell_h);
            data++;
        }
    }
    memcpy(data, index_data, sizeof(uint8_t) * 6);
    vkUnmapMemory(context.device, staging_buffer.memory);
    vkCmdCopyBuffer(cmd_buffer, staging_buffer.buffer, buffer.buffer, 1, (VkBufferCopy[]) {{.size=bufferInfo.size}});
    end_single_time_cmd_buffer(cmd_buffer);
    free_buffer(&staging_buffer);
}

static void transition_image(VkCommandBuffer command_buffer, bool transfer_data) {
    VkPipelineStageFlags src_stage, dst_stage;
    VkImageMemoryBarrier barrier = {.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=text_atlas,
            .subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .levelCount=1, .layerCount=1},
    };
    if (transfer_data) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, 0, 0, 0, 1, &barrier);
}

//static void rebuild_texture(void) {
//    const uint width = (uint) (cell_width * (float) display.display_size.width / 2);
//    const uint height = (uint) (cell_height * (float) display.display_size.height / 2);
//    const uint total_chars = (uint) (95 + glyph_list.occupied_len);
//    const VkBufferImageCopy bufferImageCopy = {
//            .imageSubresource={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .layerCount=1},
//            .imageExtent={total_chars * width, height, 1}};
//    const VkImageCreateInfo imageCreateInfo = {
//            .sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//            .imageType=VK_IMAGE_TYPE_2D, .format=VK_FORMAT_R8_UNORM,
//            .extent=bufferImageCopy.imageExtent,
//            .mipLevels=1, .arrayLayers= 1,
//            .samples=VK_SAMPLE_COUNT_1_BIT,
//            .tiling=VK_IMAGE_TILING_OPTIMAL,
//            .usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
//            .sharingMode=VK_SHARING_MODE_EXCLUSIVE, .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED};
//    Vk_buffer staging_buffer;
//    VkMemoryRequirements memoryRequirements;
//    VkMemoryAllocateInfo allocateInfo = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
//
//    const JNIEnv *env = get_env();
//    const jclass class = (*env)->FindClass(env, "com/termux/JNI");
//    jmethodID method = (*env)->GetStaticMethodID(env, class, "get_text_atlas", "([III)Landroid/graphics/Bitmap;");
//    jintArray cp_array = get_int_array(env, glyph_list.codepoints, glyph_list.occupied_len);
//    jobject j_bitmap = (*env)->CallStaticObjectMethod(env, class, method, cp_array, (int) width, (int) height);
//    VkCommandBuffer command_buffer = single_time_cmd_buffer();
//    if (text_atlas != VK_NULL_HANDLE || texture_memory != VK_NULL_HANDLE) free_textures();
//    VK_CHECK(vkCreateImage(context.device, &imageCreateInfo, NULL, &text_atlas))
//    vkGetImageMemoryRequirements(context.device, text_atlas, &memoryRequirements);
//    allocateInfo.allocationSize = memoryRequirements.size;
//    allocateInfo.memoryTypeIndex = find_mem_index(context.gpu, memoryRequirements.memoryTypeBits,
//                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//    VK_CHECK(vkAllocateMemory(context.device, &allocateInfo, NULL, &texture_memory))
//    vkBindImageMemory(context.device, text_atlas, texture_memory, 0);
//    AndroidBitmap_lockPixels(env, j_bitmap, &src.data);
//    setup_staging_buffer(&staging_buffer, &src);
//    AndroidBitmap_unlockPixels(env, j_bitmap);
//    transition_image(command_buffer, true);
//    vkCmdCopyBufferToImage(command_buffer, staging_buffer.buffer, text_atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                           1,
//                           &bufferImageCopy);
//    transition_image(command_buffer, false);
//    end_single_time_cmd_buffer(command_buffer);
//    should_rebuild_texture = false;
//    (*env)->DeleteLocalRef(env, j_bitmap);
//    (*env)->DeleteLocalRef(env, cp_array);
//    free_buffer(&staging_buffer);
////    detach_env();
//}

static inline uint16_t get_mapped_glyph(const int codepoint) {
    for (uint16_t i = 0; i < glyph_list.occupied_len; i++)
        if (codepoint == glyph_list.codepoints[i]) return i;
    if (glyph_list.occupied_len == glyph_list.len) {
        glyph_list.len += EXTRA_SPACE;
        glyph_list.codepoints = realloc(glyph_list.codepoints, (size_t) glyph_list.len * sizeof(int));
    }
    glyph_list.codepoints[glyph_list.occupied_len] = codepoint;
    should_rebuild_texture = true;
    return glyph_list.occupied_len++;
}

//static void generate_text_atlas(Screen_info info) {
//    total_cells_count = (uint32_t) (info.rows * info.columns);
//    if (info.round_screen) {
//        drawing_area.x = drawing_area.y = 0.292993f; //top left is (-1,-1) and right bottom is (1,1) offset is 2*(1-1/root(2))/2
//    }
//    cell_width = 2.0f / (float) info.columns;
//    cell_height = 2.0f / (float) info.rows;
//    rebuild_texture();
//}

static inline float texture_x(const int codepoint) {
    return (float) ((BETWEEN(codepoint, ' ', '~')) ? codepoint - ' ' : (codepoint < ' ') ? 0 : 95 + get_mapped_glyph(codepoint)) * cell_width;
}

static void on_start_drawing(void) {
    const static VkClearValue clearValue = {{{1, 0, 0, 1}}};
    static VkRenderPassBeginInfo render_pass_begin_info = {.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .clearValueCount=1, .pClearValues=&clearValue};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    render_pass_begin_info.renderPass = draw.render_pass;
    vkAcquireNextImageKHR(context.device, display.swapchain, UINT64_MAX, image_acquire_semaphore, VK_NULL_HANDLE, &image_index);
    render_pass_begin_info.framebuffer = draw.frame_buffer[image_index];
    vkResetCommandBuffer(draw.command_buffer, 0);
    vkBeginCommandBuffer(draw.command_buffer, &begin_info);
    vkCmdBeginRenderPass(draw.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindVertexBuffers(draw.command_buffer, 0, 1, &buffer.buffer, (VkDeviceSize[]) {0});
    vkCmdBindIndexBuffer(draw.command_buffer, buffer.buffer, sizeof(vec2) * 4, VK_INDEX_TYPE_UINT8_EXT);
}

static void on_end_drawing(void) {
//    if (should_rebuild_texture)rebuild_texture();
    vkCmdEndRenderPass(draw.command_buffer);
    end_cmd_buffer(draw.command_buffer);
    vkQueuePresentKHR(draw.queue, &present_info);
}

void create_renderer(const Renderer_info *info) {
    const VkSamplerCreateInfo samplerCreateInfo = {.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,};
    const VkDescriptorSetLayoutBinding layout_binding = {.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount=1, .stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT, .pImmutableSamplers=&sampler};
    const VkDescriptorSetLayoutCreateInfo layout_create_info = {.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount=1, .pBindings=&layout_binding};
    const VkDescriptorImageInfo image_info = {.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,};
    const VkPushConstantRange push_const_range = {.size=sizeof(uniform_data), .stageFlags=VK_SHADER_STAGE_VERTEX_BIT};
    const VkVertexInputBindingDescription vertex_binding[] = {
            {0, sizeof(vec2), VK_VERTEX_INPUT_RATE_VERTEX},
            {0, sizeof(vec2), VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    const VkVertexInputAttributeDescription vertex_attribute = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    const VkSemaphoreCreateInfo semaphore_create_info = {.sType=VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
    const Vk_pipeline_info pipelineInfo = {
            .vertex_shader=info->vertex_shader,
            .fragment_shader=info->fragment_shader,
            .push_constant_ranges_len=1,
            .push_constant_ranges=&push_const_range,
            .binding_description_len=3,
            .binding_description=vertex_binding,
            .attribute_description_len=1,
            .attribute_description=&vertex_attribute,
            .layout_count=1,
            .layouts=&descriptor_set_layout};
    const Vk_Draw_Info drawInfo = {
            .pipelineInfo=&pipelineInfo,
            .display=&display,
            .queue_index=create_vk_context(&context, VK_QUEUE_GRAPHICS_BIT),
            .format=create_vk_display(&context, info->window, &display),
    };
//    read_shader(info->manager, "shaders/shader.vert.spv", &pipelineInfo.vertex_shader);
//    read_shader(info->manager, "shaders/shader.frag.spv", &pipelineInfo.fragment_shader);
//    vkCreateSampler(context.device, &samplerCreateInfo, NULL, &sampler);
//    VK_CHECK(vkCreateDescriptorSetLayout(context.device, &layout_create_info, NULL, &descriptor_set_layout))
//    create_vk_draw(&context, &drawInfo, &draw);
//    free(pipelineInfo.vertex_shader.code);
//    free(pipelineInfo.fragment_shader.code);
//    vkCreateSemaphore(context.device, &semaphore_create_info, NULL, &image_acquire_semaphore);
//    generate_text_atlas(info->screen_info);
//    create_buffers(info->screen_info);
}


void destroy_renderer(void) {
    vkDestroySemaphore(context.device, image_acquire_semaphore, NULL);
    vkDestroySampler(context.device, sampler, NULL);
    vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, NULL);
    free_textures();
    free_buffer(&buffer);
    destroy_vk_draw(&context, &draw, display.image_count);
    destroy_vk_display(&context, &display);
    destroy_vk_context(&context);
}
*/
