//
// Created by Rohit Paul on 17-11-2024.
//
#include "ext/graphics/vulkan_layer/vk_wrapper.h"
#include <vulkan/vulkan_android.h>
#include <malloc.h>
#include <string.h>

#ifdef DEBUG

static VKAPI_ATTR VkBool32  VKAPI_CALL
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

static VkInstance instance;
static VkPhysicalDevice gpu;
static VkDevice device;
static VkSurfaceKHR surface;
static VkQueue queue;

static size_t swapchain_image_count;
static VkSwapchainKHR swapchain;
static VkImage *images;
static VkImageView *image_views;
static VkExtent2D display_size;

static VkRenderPass renderPass;

static VkPipelineLayout pipelineLayout;
static VkPipeline graphics_pipeline;
static VkFramebuffer *framebuffer;

static VkCommandPool commandPool;
static VkCommandBuffer commandBuffer;
static VkSemaphore image_acquire;

const static VkCommandBufferBeginInfo commandBufferBeginInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
static VkRenderPassBeginInfo renderPassBeginInfo = {.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
const static VkSubmitInfo submitInfo = {.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount=1, .pCommandBuffers=&commandBuffer};
static VkPresentInfoKHR presentInfo = {.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .pSwapchains=&swapchain, .swapchainCount=1, .waitSemaphoreCount=1, .pWaitSemaphores=&image_acquire};

static VkBuffer vertex_buffer, index_buffer;
static VkDeviceMemory vertex_buffer_memory, index_buffer_memory;


static void create_vk_shader(const uint32_t *code, size_t len, VkShaderModule *module) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize=len, .pCode=code};
    VK_CHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, module))
}

struct vertex {
    float x, y;
};

void create_instance(VkInstance *vk_instance) {
    const VkApplicationInfo applicationInfo = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion=VK_API_VERSION_1_0};
    VkInstanceCreateInfo instanceCreateInfo = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo=&applicationInfo};

#ifdef DEBUG
    const char *extension_names[3] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
                                      VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
    const char *layer = "VK_LAYER_KHRONOS_validation";
    PFN_vkCreateDebugReportCallbackEXT callbackExt = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(
            instance, "vkCreateDebugReportCallbackEXT");
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
    if (callbackExt)VK_CHECK(callbackExt(*vk_instance, &debugReportCallbackCreateInfoExt, NULL, &debugMessenger))
#endif
}

void create_surfece(VkInstance vk_instance, struct ANativeWindow *window, VkSurfaceKHR *vk_surface) {
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfoKhr = {.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window=window};
    VK_CHECK(vkCreateAndroidSurfaceKHR(vk_instance, &surfaceCreateInfoKhr, NULL, vk_surface))
}

void create_gpu(VkInstance vk_instance, VkPhysicalDevice *vk_device) {
    uint count = 1;
    VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &count, vk_device))
}

uint32_t create_logical_device(VkPhysicalDevice vk_device, VkDevice *logical_device) {
    const char *const extension_name[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    uint32_t queFamily_count, queue_family_index;
    VkQueueFamilyProperties *queueFamilyProperties;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo graphics_queue = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount=1, .pQueuePriorities=&priority};
    VkDeviceCreateInfo deviceCreateInfo = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount=1, .pQueueCreateInfos=&graphics_queue};
    vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &queFamily_count, NULL);
    queueFamilyProperties = malloc(queFamily_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &queFamily_count, queueFamilyProperties);
    for (queue_family_index = 0; queue_family_index < queFamily_count; queue_family_index++) {
        if (queueFamilyProperties[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)break;
    }
    graphics_queue.queueFamilyIndex = queue_family_index;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = extension_name;
    VK_CHECK(vkCreateDevice(vk_device, &deviceCreateInfo, NULL, logical_device))
    free(queueFamilyProperties);
    return queue_family_index;
}

void get_queue(VkDevice vk_device, uint32_t queue_family_index, VkQueue *vk_queue) {
    vkGetDeviceQueue(vk_device, queue_family_index, 0, vk_queue);
}

void create_swapchain(VkDevice vk_device, VkPhysicalDevice vk_physical_device, VkSurfaceKHR vk_surface,
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

void create_images_and_views(VkDevice vk_device, VkSwapchainKHR vk_swapchain, VkFormat vk_format, uint32_t *imageCount,
                             VkImage **vk_images, VkImageView **vk_image_views) {

    VkImageViewCreateInfo imageViewCreateInfo = {.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .viewType=VK_IMAGE_VIEW_TYPE_2D, .format=vk_format, .components={.r=VK_COMPONENT_SWIZZLE_IDENTITY, .g=VK_COMPONENT_SWIZZLE_IDENTITY, .b=VK_COMPONENT_SWIZZLE_IDENTITY, .a=VK_COMPONENT_SWIZZLE_IDENTITY}, .subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .layerCount=1, .levelCount=1}};
    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, imageCount, NULL))
    *vk_images = malloc(sizeof(VkImage) * *imageCount);
    *vk_image_views = malloc(sizeof(VkImageView) * *imageCount);

    VK_CHECK(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, imageCount, *vk_images))
    for (size_t i = 0; i < *imageCount; i++) {
        VkImageViewCreateInfo viewCreateInfo = imageViewCreateInfo;
        viewCreateInfo.image = (*vk_images)[i];
        VK_CHECK(vkCreateImageView(vk_device, &viewCreateInfo, NULL, *vk_image_views + i))
    }
}

void create_renderpass(VkDevice vk_device, VkFormat vk_format, VkRenderPass *vk_render_pass) {
    const VkAttachmentDescription attachmentDescription = {.format=vk_format, .samples=VK_SAMPLE_COUNT_1_BIT, .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp=VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    const VkAttachmentReference attachmentReference = {.attachment=0, .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription vkSubpassDescription = {.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount=1, .pColorAttachments=&attachmentReference};
    const VkRenderPassCreateInfo renderPassCreateInfo = {.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount=1, .pAttachments=&attachmentDescription, .subpassCount=1, .pSubpasses=&vkSubpassDescription};
    VK_CHECK(vkCreateRenderPass(vk_device, &renderPassCreateInfo, NULL, vk_render_pass))
}

uint32_t find_mem_index(uint32_t mem_type, VkMemoryPropertyFlags req_properties) {
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

void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property, VkBuffer *buffer,
                   VkDeviceMemory *buffer_memory) {
    const VkBufferCreateInfo bufferCreateInfo = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size=size, .usage=usage, .sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryAllocateInfo allocateInfo = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryRequirements memoryRequirements;

    VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, NULL, buffer))

    vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = find_mem_index(memoryRequirements.memoryTypeBits, property);

    VK_CHECK(vkAllocateMemory(device, &allocateInfo, NULL, buffer_memory))
    vkBindBufferMemory(device, *buffer, *buffer_memory, 0);
}

void init_vulkan(Vulkan_init_info *info) {
    uint queue_family_index;
    VkFormat format;
    create_instance(&instance);
    create_surfece(instance, info->window, &surface);
    create_gpu(instance, &gpu);
    queue_family_index = create_logical_device(gpu, &device);
    get_queue(device, queue_family_index, &queue);
    create_swapchain(device, gpu, surface, &display_size, &format, &swapchain);
    create_images_and_views(device, swapchain, format, &swapchain_image_count, &images, &image_views);
    create_renderpass(device, format, &renderPass);
//Renderpass begin Info
    {
        VkClearValue clearValue = {.color={{0, 0, 0, 1}}};
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.extent = display_size;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;
    }

//PipeLine
    {
        VkShaderModule vertex_shader, fragment_shader;
        VkPipelineShaderStageCreateInfo vertex_info, fragment_info;
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
        VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        //Shader
        {
            create_vk_shader(info->vertex_shader.code, info->vertex_shader.length, &vertex_shader);
            create_vk_shader(info->fragment_shader.code, info->fragment_shader.length, &fragment_shader);
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
        //Creating Pipeline DynamicState Info
        {
            dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCreateInfo.pDynamicStates = (VkDynamicState[]) {VK_DYNAMIC_STATE_VIEWPORT,
                                                                        VK_DYNAMIC_STATE_SCISSOR};
            dynamicStateCreateInfo.dynamicStateCount = 2;
        }

        //Vertex Input
        {
            VkVertexInputBindingDescription bindingDescription = {.binding=0, .stride=sizeof(struct vertex), .inputRate=VK_VERTEX_INPUT_RATE_VERTEX};
            VkVertexInputAttributeDescription attributeDescription = {.binding=0, .location=0, .format=VK_FORMAT_R32G32_SFLOAT, .offset=0};

            vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
            vertexInputCreateInfo.vertexAttributeDescriptionCount = 1;
            vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputCreateInfo.pVertexAttributeDescriptions = &attributeDescription;
        }
        //Input Assembly
        {
            inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
        }

        //Viewport and Scissors
        {
            VkViewport viewport;
            VkRect2D scissor;

            viewport.x = 0;
            viewport.y = 0;
            viewport.width = (float) display_size.width;
            viewport.height = (float) display_size.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            scissor.offset = (VkOffset2D) {0, 0};
            scissor.extent = display_size;

            viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportStateCreateInfo.viewportCount = 1;
            viewportStateCreateInfo.pViewports = &viewport;
            viewportStateCreateInfo.scissorCount = 1;
            viewportStateCreateInfo.pScissors = &scissor;

        }
        //Rasterization
        {
            rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
            rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
            rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizationStateCreateInfo.lineWidth = 1;
            rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        }
        //MultiSample
        {
            multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
            multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        }

        //Color Blending
        {
            VkPipelineColorBlendAttachmentState colorBlendAttachment = {.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT, .blendEnable=VK_FALSE};
            colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
            colorBlendStateCreateInfo.attachmentCount = 1;
            colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
        }

        //Pipeline Layout
        {
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pSetLayouts=NULL, .setLayoutCount=0, .pPushConstantRanges=NULL, .pushConstantRangeCount=0};
            VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout))
        }

        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = (VkPipelineShaderStageCreateInfo[]) {vertex_info, fragment_info};
        pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = renderPass;
        pipelineCreateInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &graphics_pipeline))
        vkDestroyShaderModule(device, fragment_shader, NULL);
        vkDestroyShaderModule(device, vertex_shader, NULL);
    }

//Framebuffer
    {
        framebuffer = malloc(swapchain_image_count * sizeof(VkFramebuffer));
        for (size_t i = 0; i < swapchain_image_count; i++) {
            const VkFramebufferCreateInfo framebufferCreateInfo = {.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass=renderPass, .attachmentCount=1, .pAttachments=
            image_views + i, .width=display_size.width, .height=display_size.height, .layers=1};
            VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, NULL, framebuffer + i))
        }
    }

//Command Pool
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex=queue_family_index};
        VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool))
    }
//Command Buffer
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool=commandPool, .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount=1};
        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer))
    }
//Vertex buffer
    {
        const struct vertex ver[] = {{-0.5f, -0.5f},
                                     {0.5f,  -0.5f},
                                     {0.5f,  0.5f},
                                     {-0.5f, 0.5f}};
        void *data;
        VkBuffer staging;
        VkDeviceMemory staging_mem;
        const size_t size = sizeof(struct vertex) * 4;
        const VkBufferCopy copyRegion = {.size=size};
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging,
                      &staging_mem);
        vkMapMemory(device, staging_mem, 0, size, 0, &data);
        memcpy(data, ver, size);
        vkUnmapMemory(device, staging_mem);
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer, &vertex_buffer_memory);

        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        vkCmdCopyBuffer(commandBuffer, staging, vertex_buffer, 1, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkDestroyBuffer(device, staging, NULL);
        vkFreeMemory(device, staging_mem, NULL);
    }
//index buffer
    {
        const uint8_t ind[] = {0, 1, 2, 2, 3, 0};
        void *data;
        VkBuffer staging;
        VkDeviceMemory staging_mem;
        const size_t size = sizeof(ind);
        const VkBufferCopy copyRegion = {.size=size};
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging,
                      &staging_mem);
        vkMapMemory(device, staging_mem, 0, size, 0, &data);
        memcpy(data, ind, size);
        vkUnmapMemory(device, staging_mem);
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &index_buffer, &index_buffer_memory);

        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        vkCmdCopyBuffer(commandBuffer, staging, index_buffer, 1, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkDestroyBuffer(device, staging, NULL);
        vkFreeMemory(device, staging_mem, NULL);
    }
//Creating sync objects
    {
        const VkSemaphoreCreateInfo semaphoreCreateInfo = {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &image_acquire))
    }
}

void start_drawing(void) {
    uint32_t image_index;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_acquire, VK_NULL_HANDLE, &image_index);
    renderPassBeginInfo.framebuffer = framebuffer[image_index];
    presentInfo.pImageIndices = &image_index;
    vkResetCommandBuffer(commandBuffer, 0);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo))
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
}

void end_drawing(void) {
    vkCmdEndRenderPass(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer))
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE))
    VK_CHECK(vkQueuePresentKHR(queue, &presentInfo))
}

void cleanup_vulkan(void) {
    vkQueueWaitIdle(queue);
    vkDestroySemaphore(device, image_acquire, NULL);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, NULL);
    for (size_t i = 0; i < swapchain_image_count; i++)
        vkDestroyFramebuffer(device, framebuffer[i], NULL);

    vkDestroyPipeline(device, graphics_pipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyRenderPass(device, renderPass, NULL);

    for (size_t i = 0; i < swapchain_image_count; i++)
        vkDestroyImageView(device, image_views[i], NULL);

    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyBuffer(device, vertex_buffer, NULL);
    vkFreeMemory(device, vertex_buffer_memory, NULL);
    vkDestroyBuffer(device, index_buffer, NULL);
    vkFreeMemory(device, index_buffer_memory, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
#ifdef DEBUG
    {
        PFN_vkDestroyDebugReportCallbackEXT debugReportCallbackExt = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(
                instance, "vkDestroyDebugReportCallbackEXT");
        if (debugReportCallbackExt) {
            debugReportCallbackExt(instance, debugMessenger, NULL);
        }
    }
#endif
    vkDestroyInstance(instance, NULL);
}

#ifdef DEBUG


void test_drawing(void) {
    start_drawing();
    {
        VkDeviceSize offset = 0;
        float scale = 1.0f;
        VkViewport viewport = {.width=(float) display_size.width * scale, .height=
        (float) display_size.height * scale};
        VkRect2D scissor = {.extent=display_size};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex_buffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer,index_buffer,0,VK_INDEX_TYPE_UINT8_EXT);
        vkCmdDrawIndexed(commandBuffer,6,1,0,0,0);
    }
    end_drawing();
}

#endif
