//
// Created by Rohit Paul on 17-11-2024.
//
#include "ext/graphics/vulkan_layer/vk_wrapper.h"
#include <vulkan/vulkan_android.h>
#include <malloc.h>
#include <string.h>

#ifdef DEBUG

static inline VKAPI_ATTR VkBool32  VKAPI_CALL
vk_debug_report_callback(VkDebugReportFlagsEXT flags, __attribute__((unused)) VkDebugReportObjectTypeEXT objectType,
                         __attribute__((unused)) uint64_t object, __attribute__((unused)) size_t location,
                         __attribute__((unused)) int32_t messageCode, const char *pLayerPrefix, const char *pMessage,
                         __attribute__((unused)) void *pUserData) {
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
        __android_log_print(ANDROID_LOG_INFO, "Vulkan-Debug-Message:  ", "%s -- %s", pLayerPrefix, pMessage);
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        __android_log_print(ANDROID_LOG_WARN, "Vulkan-Debug-Message: ", "%s -- %s", pLayerPrefix, pMessage);
    } else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
        __android_log_print(ANDROID_LOG_WARN, "Vulkan-Debug-Message-(Perf): ", "%s -- %s", pLayerPrefix, pMessage);
    } else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        __android_log_print(ANDROID_LOG_ERROR, "Vulkan-Debug-Message: ", "%s -- %s", pLayerPrefix, pMessage);
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "Vulkan-Debug-Message: ", "%s -- %s", pLayerPrefix, pMessage);
    }
    return VK_FALSE;
}

#define VK_CHECK(fun) if(fun!=VK_SUCCESS){ LOG("VULKAN ERROR IN LINE %d : %s code : %d\n",__LINE__, #fun,fun);}
static VkDebugReportCallbackEXT debugMessenger;
#else
#define VK_CHECK(fun) fun;
#endif

static inline void create_vk_shader(VkDevice device, const uint32_t *code, size_t len, VkShaderModule *module) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize=len, .pCode=code};
    VK_CHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, module))
}

static inline void create_instance(VkInstance *vk_instance) {
    const VkApplicationInfo applicationInfo = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion=VK_API_VERSION_1_0};
    VkInstanceCreateInfo instanceCreateInfo = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo=&applicationInfo};
#ifdef DEBUG
    const char *extension_names[3] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
                                      VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
    const char *layer = "VK_LAYER_KHRONOS_validation";

    const VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfoExt = {.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT, .flags =
    VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
    VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT, .pfnCallback=vk_debug_report_callback};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = &layer;
#else
    const char *const extension_names[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
#endif

    instanceCreateInfo.enabledExtensionCount = LEN(extension_names);
    instanceCreateInfo.ppEnabledExtensionNames = extension_names;

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, NULL, vk_instance))
#ifdef DEBUG
    {
        PFN_vkCreateDebugReportCallbackEXT callbackExt = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(
                *vk_instance, "vkCreateDebugReportCallbackEXT");
        if (callbackExt)VK_CHECK(callbackExt(*vk_instance, &debugReportCallbackCreateInfoExt, NULL, &debugMessenger))
    }
#endif
}

static inline void create_gpu(VkInstance vk_instance, VkPhysicalDevice *vk_device) {
    uint count = 1;
    VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &count, vk_device))
}

static inline uint32_t
create_logical_device(VkPhysicalDevice physical_device, VkDevice *logical_device, VkQueueFlags queueFlags) {
    const float priority = 1.0f;
    uint32_t queFamily_count, que_family_index;
    VkQueueFamilyProperties *queueFamilyProperties;
    VkDeviceQueueCreateInfo graphics_queue = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount=1, .pQueuePriorities=&priority};
    const VkDeviceCreateInfo deviceCreateInfo = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount=1, .pQueueCreateInfos=&graphics_queue, .enabledExtensionCount=1, .ppEnabledExtensionNames=(char *[]) {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queFamily_count, NULL);
    queueFamilyProperties = malloc(queFamily_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queFamily_count, queueFamilyProperties);
    for (que_family_index = 0; que_family_index < queFamily_count; que_family_index++) {
        if ((queueFamilyProperties[que_family_index].queueFlags & queueFlags) == queueFlags) {
            graphics_queue.queueFamilyIndex = que_family_index;
            break;
        }
    }
    VK_CHECK(vkCreateDevice(physical_device, &deviceCreateInfo, NULL, logical_device))
    free(queueFamilyProperties);
    return que_family_index;
}

static inline void create_surface(VkInstance vk_instance, struct ANativeWindow *window, VkSurfaceKHR *vk_surface) {
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfoKhr = {.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window=window};
    VK_CHECK(vkCreateAndroidSurfaceKHR(vk_instance, &surfaceCreateInfoKhr, NULL, vk_surface))
}

static inline void create_swapchain(VkDevice vk_device, VkPhysicalDevice vk_physical_device, VkSurfaceKHR vk_surface,
                                    VkExtent2D *display_extent, VkFormat *format, VkSwapchainKHR *vk_swapchain) {
    VkSurfaceCapabilitiesKHR capabilities;
    uint format_count;
    VkSurfaceFormatKHR *formats;
    uint format_index;
    VkSwapchainCreateInfoKHR createInfoKhr = {.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface=vk_surface, .imageArrayLayers=1, .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE, .preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, .presentMode=VK_PRESENT_MODE_FIFO_KHR, .clipped=VK_TRUE};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, NULL);
    formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, formats);
    for (format_index = 0; format_index < format_count; format_index++)
        if (formats[format_index].format == VK_FORMAT_R8G8B8A8_UNORM)break;
    createInfoKhr.minImageCount = capabilities.minImageCount;
    *display_extent = createInfoKhr.imageExtent = capabilities.currentExtent;
    *format = createInfoKhr.imageFormat = formats[format_index].format;
    createInfoKhr.imageColorSpace = formats[format_index].colorSpace;
    createInfoKhr.compositeAlpha = capabilities.supportedCompositeAlpha;
    VK_CHECK(vkCreateSwapchainKHR(vk_device, &createInfoKhr, NULL, vk_swapchain))
    free(formats);
}

void create_image_view(VkDevice vk_device, VkImageView *image_view, const VkImage *image, VkFormat vk_format) {
    const VkImageViewCreateInfo imageViewCreateInfo = {.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image=*image, .viewType=VK_IMAGE_VIEW_TYPE_2D, .format=vk_format, .subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .layerCount=1, .levelCount=1}};
    VK_CHECK(vkCreateImageView(vk_device, &imageViewCreateInfo, NULL, image_view))
}

static inline void
create_images_and_views(VkDevice vk_device, VkSwapchainKHR vk_swapchain, VkFormat vk_format, uint32_t *imageCount,
                        VkImageView **vk_image_views) {
    VkImage *vk_images;

    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, imageCount, NULL))
    vk_images = malloc(sizeof(VkImage) * *imageCount);
    *vk_image_views = malloc(sizeof(VkImageView) * *imageCount);

    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, imageCount, vk_images))
    for (size_t i = 0; i < *imageCount; i++) create_image_view(vk_device, *vk_image_views + i, vk_images + i, vk_format);
    free(vk_images);
}

static inline void create_renderpass(VkDevice vk_device, VkFormat vk_format, VkRenderPass *vk_render_pass) {
    const VkAttachmentDescription attachmentDescription = {.format=vk_format, .samples=VK_SAMPLE_COUNT_1_BIT, .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp=VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    const VkAttachmentReference attachmentReference = {.attachment=0, .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription vkSubpassDescription = {.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount=1, .pColorAttachments=&attachmentReference};
    const VkRenderPassCreateInfo renderPassCreateInfo = {.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount=1, .pAttachments=&attachmentDescription, .subpassCount=1, .pSubpasses=&vkSubpassDescription};
    VK_CHECK(vkCreateRenderPass(vk_device, &renderPassCreateInfo, NULL, vk_render_pass))
}

static inline void
create_framebuffer(VkDevice device, uint swapchain_image_count, VkExtent2D extend, const VkImageView *image_views,
                   VkRenderPass render_pass, VkFramebuffer **frame_buffer) {
    *frame_buffer = malloc(swapchain_image_count * sizeof(VkFramebuffer));
    for (size_t i = 0; i < swapchain_image_count; i++) {
        const VkFramebufferCreateInfo framebufferCreateInfo = {.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass=render_pass, .attachmentCount=1, .pAttachments=
        image_views + i, .width=extend.width, .height=extend.height, .layers=1};
        VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, NULL, *frame_buffer + i))
    }
}

static inline void create_command_pool(VkDevice device, uint32_t queue_family_index, VkCommandPool *command_pool) {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex=queue_family_index};
    VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, command_pool))
}

void create_command_buffer(VkDevice device, VkCommandPool command_pool, VkCommandBuffer *command_buffer) {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool=command_pool, .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount=1};
    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, command_buffer))
}

static inline void
create_pipeline(VkDevice device, VkExtent2D display_size, VkRenderPass renderPass, Vk_pipeline_info *pipeline_info,
                VkPipeline *pipeline, VkPipelineLayout *pipelineLayout) {
    VkShaderModule vertex_shader, fragment_shader;
    VkPipelineShaderStageCreateInfo vertex_info, fragment_info;
    const VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .pVertexAttributeDescriptions=pipeline_info->attribute_description, .vertexAttributeDescriptionCount=pipeline_info->attribute_description_len, .pVertexBindingDescriptions=pipeline_info->binding_description, .vertexBindingDescriptionCount=pipeline_info->binding_description_len};
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount=1, .pViewports=(VkViewport[]) {
            {.width=(float) display_size.width, .height=(float) display_size.height}}, .pScissors=(VkRect2D[]) {
            {.extent=display_size}}};
    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .lineWidth = 1, .cullMode = VK_CULL_MODE_BACK_BIT, .frontFace = VK_FRONT_FACE_CLOCKWISE};
    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount=1, .pAttachments=(VkPipelineColorBlendAttachmentState[]) {
            {.colorWriteMask=VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                             VK_COLOR_COMPONENT_A_BIT}}};

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount=2, .pVertexInputState = &vertexInputCreateInfo, .pInputAssemblyState = &inputAssemblyStateCreateInfo, .pViewportState = &viewportStateCreateInfo, .pRasterizationState = &rasterizationStateCreateInfo, .pMultisampleState = &multisampleStateCreateInfo, .pColorBlendState = &colorBlendStateCreateInfo};
    //Shader
    {
        create_vk_shader(device, pipeline_info->vertex_shader.code, pipeline_info->vertex_shader.length,
                         &vertex_shader);
        create_vk_shader(device, pipeline_info->fragment_shader.code, pipeline_info->fragment_shader.length,
                         &fragment_shader);
    }
    //Creating ShaderStage Info
    {
        const VkPipelineShaderStageCreateInfo temp = {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pName="main"};
        vertex_info = fragment_info = temp;
        vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        vertex_info.module = vertex_shader;
        fragment_info.module = fragment_shader;
    }

    //Pipeline Layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                                     .pSetLayouts=pipeline_info->layouts,
                                                                     .setLayoutCount=pipeline_info->layout_count,
                                                                     .pPushConstantRanges=pipeline_info->push_constant_ranges,
                                                                     .pushConstantRangeCount=pipeline_info->push_constant_ranges_len};
        VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pipelineLayout))
    }

    pipelineCreateInfo.pStages = (VkPipelineShaderStageCreateInfo[]) {vertex_info, fragment_info};
    pipelineCreateInfo.layout = *pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, pipeline))
    vkDestroyShaderModule(device, fragment_shader, NULL);
    vkDestroyShaderModule(device, vertex_shader, NULL);
}

uint32_t create_vk_context(Vk_Context *context, VkQueueFlags queue_flags) {
    create_instance(&context->instance);
    create_gpu(context->instance, &context->gpu);
    return create_logical_device(context->gpu, &context->device, queue_flags);
}

VkFormat create_vk_display(Vk_Context *context, ANativeWindow *window, Vk_display *display) {
    VkFormat format;
    create_surface(context->instance, window, &display->surface);
    create_swapchain(context->device, context->gpu, display->surface, &display->display_size, &format,
                     &display->swapchain);
    create_images_and_views(context->device, display->swapchain, format, &display->image_count, &display->image_views);
    return format;
}

void create_vk_draw(Vk_Context *context, Vk_Draw_Info *drawInfo, Vk_draw *draw) {
    vkGetDeviceQueue(context->device, drawInfo->queue_index, 0, &draw->queue);
    create_renderpass(context->device, drawInfo->format, &draw->render_pass);
    create_framebuffer(context->device, drawInfo->display->image_count, drawInfo->display->display_size,
                       drawInfo->display->image_views, draw->render_pass, &draw->frame_buffer);
    create_command_pool(context->device, drawInfo->queue_index, &draw->command_pool);
    create_command_buffer(context->device, draw->command_pool, &draw->command_buffer);
    create_pipeline(context->device, drawInfo->display->display_size, draw->render_pass, drawInfo->pipelineInfo,
                    &draw->pipeline, &draw->pipeline_layout);
}

uint32_t find_mem_index(VkPhysicalDevice gpu, uint32_t mem_type, VkMemoryPropertyFlags req_properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((mem_type & (1 << i)) &&
            ((memoryProperties.memoryTypes[i].propertyFlags & req_properties) == req_properties)) {
            return i;
        }
    }
    return 0;
}

void create_buffer(Vk_Context *context, Vk_Buffer_Info *bufferInfo) {
    const VkBufferCreateInfo bufferCreateInfo = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size=bufferInfo->size, .usage=bufferInfo->usage_flags, .sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryRequirements memoryRequirements;
    VkMemoryAllocateInfo allocateInfo = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VK_CHECK(vkCreateBuffer(context->device, &bufferCreateInfo, NULL, &bufferInfo->buffer->buffer))
    vkGetBufferMemoryRequirements(context->device, bufferInfo->buffer->buffer, &memoryRequirements);
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = find_mem_index(context->gpu, memoryRequirements.memoryTypeBits, bufferInfo->memory_property_flags);
    VK_CHECK(vkAllocateMemory(context->device, &allocateInfo, NULL, &bufferInfo->buffer->memory))

    vkBindBufferMemory(context->device, bufferInfo->buffer->buffer, bufferInfo->buffer->memory, 0);
}

void destroy_vk_context(Vk_Context *context) {
    vkDestroyDevice(context->device, NULL);
#ifdef DEBUG
    {
        PFN_vkDestroyDebugReportCallbackEXT debugReportCallbackExt = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(
                context->instance, "vkDestroyDebugReportCallbackEXT");
        if (debugReportCallbackExt) {
            debugReportCallbackExt(context->instance, debugMessenger, NULL);
        }
    }
#endif
    vkDestroyInstance(context->instance, NULL);
}

void destroy_vk_display(Vk_Context *context, Vk_display *display) {
    for (size_t i = 0; i < display->image_count; i++)
        vkDestroyImageView(context->device, display->image_views[i], NULL);
    vkDestroySwapchainKHR(context->device, display->swapchain, NULL);
    vkDestroySurfaceKHR(context->instance, display->surface, NULL);
}

void destroy_vk_draw(Vk_Context *context, Vk_draw *draw, uint32_t image_count) {
    vkQueueWaitIdle(draw->queue);
    vkFreeCommandBuffers(context->device, draw->command_pool, 1, &draw->command_buffer);
    vkDestroyCommandPool(context->device, draw->command_pool, NULL);
    for (size_t i = 0; i < image_count; i++)
        vkDestroyFramebuffer(context->device, draw->frame_buffer[i], NULL);

    vkDestroyPipeline(context->device, draw->pipeline, NULL);
    vkDestroyPipelineLayout(context->device, draw->pipeline_layout, NULL);
    vkDestroyRenderPass(context->device, draw->render_pass, NULL);
}
