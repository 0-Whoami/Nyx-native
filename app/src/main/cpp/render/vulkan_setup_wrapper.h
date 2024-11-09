//
// Created by Rohit Paul on 13-10-2024.
//

#ifndef NYX_VULKAN_SETUP_WRAPPER_H
#define NYX_VULKAN_SETUP_WRAPPER_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include "common/utils.h"
#include <malloc.h>

#define DEBUG

#ifdef DEBUG

static VKAPI_ATTR VkBool32  VKAPI_CALL
vk_debug_report_callback(VkDebugReportFlagsEXT flags, __attribute__((unused)) VkDebugReportObjectTypeEXT objectType,
                         __attribute__((unused)) uint64_t object, __attribute__((unused)) size_t location,
                         __attribute__((unused)) int32_t messageCode, const char *pLayerPrefix, const char *pMessage,
                         __attribute__((unused)) void *pUserData) {
    if(flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
        __android_log_print(ANDROID_LOG_INFO, "Vulkan-Debug-Message:  ", "%s -- %s", pLayerPrefix, pMessage);
    } else if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        __android_log_print(ANDROID_LOG_WARN, "Vulkan-Debug-Message: ", "%s -- %s", pLayerPrefix, pMessage);
    } else if(flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
        __android_log_print(ANDROID_LOG_WARN, "Vulkan-Debug-Message-(Perf): ", "%s -- %s", pLayerPrefix, pMessage);
    } else if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
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
static VkSemaphore image_acquire, text_bg, text_fg;

const static VkCommandBufferBeginInfo commandBufferBeginInfo = { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
static VkRenderPassBeginInfo renderPassBeginInfo = { .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
const static VkSubmitInfo submitInfo = { .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount=1, .pCommandBuffers=&commandBuffer };
static VkPresentInfoKHR presentInfo = { .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .pSwapchains=&swapchain, .swapchainCount=1, .waitSemaphoreCount=1, .pWaitSemaphores=&image_acquire };

typedef struct {
    uint32_t *code;
    size_t length;
} shader_info;
typedef struct {
    struct ANativeWindow *window;
    shader_info vertex_shader, fragment_shader;
} Vulkan_init_info;

extern void init_vulkan(Vulkan_init_info *);

extern void start_drawing(void);

extern void end_drawing(void);

extern void cleanup_vulkan(void);

extern void test(void);

static void create_vk_shader(const uint32_t *code, size_t len, VkShaderModule *module) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo = { .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize=len, .pCode=code };
    VK_CHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, module))
}

void init_vulkan(Vulkan_init_info *info) {
    uint queue_family_index;
    VkFormat format;

// Instance
    {
        VkApplicationInfo applicationInfo = { .sType=VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion=VK_MAKE_VERSION(1, 1, 0) };
        VkInstanceCreateInfo instanceCreateInfo = { .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo=&applicationInfo };

#ifdef DEBUG
        const char *extension_names[3] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
        const char *layer = "VK_LAYER_KHRONOS_validation";
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = &layer;

#else
        const char *const extension_names[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
#endif

        instanceCreateInfo.enabledExtensionCount = LEN(extension_names);
        instanceCreateInfo.ppEnabledExtensionNames = extension_names;

        VK_CHECK(vkCreateInstance(&instanceCreateInfo, NULL, &instance))

#ifdef DEBUG
//Hooking validation layer
        {
            PFN_vkCreateDebugReportCallbackEXT debugReportCallbackExt = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance,"vkCreateDebugReportCallbackEXT");
            if(debugReportCallbackExt) {
                VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfoExt = { };
                debugReportCallbackCreateInfoExt.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                debugReportCallbackCreateInfoExt.flags =
                        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                        VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
                debugReportCallbackCreateInfoExt.pfnCallback = vk_debug_report_callback;
                VK_CHECK(debugReportCallbackExt(instance, &debugReportCallbackCreateInfoExt, NULL, &debugMessenger))
            }
        }
#endif

    }
// Surface
    {
        VkAndroidSurfaceCreateInfoKHR surfaceCreateInfoKhr = { .sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window=info->window };
        VK_CHECK(vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfoKhr, NULL, &surface))
    }
// Physical Device
    {
        unsigned int count = 1;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, &gpu))
    }
// Logical Device
    {
        const char *const extension_name[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        uint32_t queFamily_count;
        VkQueueFamilyProperties *queueFamilyProperties;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo graphics_queue = { .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount=1, .pQueuePriorities=&priority };
        VkDeviceCreateInfo deviceCreateInfo = { .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount=1, .pQueueCreateInfos=&graphics_queue };
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queFamily_count, NULL);
        queueFamilyProperties = malloc(queFamily_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queFamily_count, queueFamilyProperties);
        for(queue_family_index = 0; queue_family_index < queFamily_count; queue_family_index++) {
            if(queueFamilyProperties[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)break;
        }
        graphics_queue.queueFamilyIndex = queue_family_index;
        deviceCreateInfo.enabledExtensionCount = 1;
        deviceCreateInfo.ppEnabledExtensionNames = extension_name;
        VK_CHECK(vkCreateDevice(gpu, &deviceCreateInfo, NULL, &device))
        free(queueFamilyProperties);
    }
// Queue
    {
        vkGetDeviceQueue(device, queue_family_index, 0, &queue);
    }

// Swap Chain
    {
        VkSurfaceCapabilitiesKHR capabilities;
        uint format_count;
        VkSurfaceFormatKHR *formats;
        uint format_index;
        VkSwapchainCreateInfoKHR createInfoKhr = { .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface=surface, .imageArrayLayers=1, .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE, .preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, .presentMode=VK_PRESENT_MODE_FIFO_KHR, .clipped=VK_TRUE };

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, NULL);
        formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, formats);

        for(format_index = 0; format_index < format_count; format_index++)
            if(formats[format_index].format == VK_FORMAT_R8G8B8A8_UNORM)break;

        display_size = capabilities.currentExtent;
        format = formats[format_index].format;
        createInfoKhr.minImageCount = capabilities.minImageCount;
        createInfoKhr.imageExtent = display_size;
        createInfoKhr.imageColorSpace = formats[format_index].colorSpace;
        createInfoKhr.imageFormat = format;
        createInfoKhr.compositeAlpha = capabilities.supportedCompositeAlpha;
        VK_CHECK(vkCreateSwapchainKHR(device, &createInfoKhr, NULL, &swapchain))
        free(formats);
    }
//Getting Images and creating ImageView
    {

        VkImageViewCreateInfo imageViewCreateInfo = { .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .viewType=VK_IMAGE_VIEW_TYPE_2D, .format=format, .components={ .r=VK_COMPONENT_SWIZZLE_IDENTITY, .g=VK_COMPONENT_SWIZZLE_IDENTITY, .b=VK_COMPONENT_SWIZZLE_IDENTITY, .a=VK_COMPONENT_SWIZZLE_IDENTITY }, .subresourceRange={ .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .layerCount=1, .levelCount=1 }};
        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL))
        images = malloc(sizeof(VkImage) * swapchain_image_count);
        image_views = malloc(sizeof(VkImageView) * swapchain_image_count);

        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, images))
        for(size_t i = 0; i < swapchain_image_count; i++) {
            VkImageViewCreateInfo viewCreateInfo = imageViewCreateInfo;
            viewCreateInfo.image = images[i];
            VK_CHECK(vkCreateImageView(device, &viewCreateInfo, NULL, image_views + i))
        }
    }

//RenderPass
    {
        const VkAttachmentDescription attachmentDescription = { .format=format, .samples=VK_SAMPLE_COUNT_1_BIT, .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp=VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
        const VkAttachmentReference attachmentReference = { .attachment=0, .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        const VkSubpassDescription vkSubpassDescription = { .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount=1, .pColorAttachments=&attachmentReference };
        const VkRenderPassCreateInfo renderPassCreateInfo = { .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount=1, .pAttachments=&attachmentDescription, .subpassCount=1, .pSubpasses=&vkSubpassDescription };
        VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, NULL, &renderPass))
    }
//Renderpass begin Info
    {
        VkClearValue clearValue = { .color={{ 0, 0, 0, 1 }}};
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.extent = display_size;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;
    }

//PipeLine
    {
        VkShaderModule vertex_shader, fragment_shader;
        VkPipelineShaderStageCreateInfo vertex_info, fragment_info;
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { };
        VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { };
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = { };
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { };
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = { };
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = { };
        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = { };

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = { .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        //Shader
        {
            create_vk_shader(info->vertex_shader.code, info->vertex_shader.length, &vertex_shader);
            create_vk_shader(info->fragment_shader.code, info->fragment_shader.length, &fragment_shader);
        }
        //Creating ShaderStage Info
        {
            const VkPipelineShaderStageCreateInfo temp = { .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pName="main" };
            vertex_info = fragment_info = temp;
            vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            vertex_info.module = vertex_shader;
            fragment_info.module = fragment_shader;
        }
        //Creating Pipeline DynamicState Info
        {
            dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCreateInfo.pDynamicStates = (VkDynamicState[]) { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            dynamicStateCreateInfo.dynamicStateCount = 2;
        }

        //Vertex Input
        {
            vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
            vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
            vertexInputCreateInfo.pVertexBindingDescriptions = NULL;
            vertexInputCreateInfo.pVertexAttributeDescriptions = NULL;
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

            scissor.offset = (VkOffset2D) { 0, 0 };
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
            VkPipelineColorBlendAttachmentState colorBlendAttachment = { .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                                                           VK_COLOR_COMPONENT_B_BIT |
                                                                                           VK_COLOR_COMPONENT_A_BIT, .blendEnable=VK_FALSE };
            colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
            colorBlendStateCreateInfo.attachmentCount = 1;
            colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
        }

        //Pipeline Layout
        {
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pSetLayouts=NULL, .setLayoutCount=0, .pPushConstantRanges=NULL, .pushConstantRangeCount=0 };
            VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout))
        }

        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = (VkPipelineShaderStageCreateInfo[]) { vertex_info, fragment_info };
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
        for(size_t i = 0; i < swapchain_image_count; i++) {
            const VkFramebufferCreateInfo framebufferCreateInfo = { .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass=renderPass, .attachmentCount=1, .pAttachments=
            image_views + i, .width=display_size.width, .height=display_size.height, .layers=1 };
            VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, NULL, framebuffer + i))
        }
    }

//Command Pool
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = { .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex=queue_family_index };
        VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool))
    }
//Command Buffer
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool=commandPool, .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount=1 };
        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer))
    }

//Creating sync objects
    {
        const VkSemaphoreCreateInfo semaphoreCreateInfo = { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &image_acquire))
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &text_bg))
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &text_fg))
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
    vkDestroySemaphore(device, text_bg, NULL);
    vkDestroySemaphore(device, text_fg, NULL);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, NULL);
    for(size_t i = 0; i < swapchain_image_count; i++)vkDestroyFramebuffer(device, framebuffer[i], NULL);

    vkDestroyPipeline(device, graphics_pipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyRenderPass(device, renderPass, NULL);

    for(size_t i = 0; i < swapchain_image_count; i++)
        vkDestroyImageView(device, image_views[i], NULL);

    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
#ifdef DEBUG
    {
        PFN_vkDestroyDebugReportCallbackEXT debugReportCallbackExt = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance,
                                                                                                                                 "vkDestroyDebugReportCallbackEXT");
        if(debugReportCallbackExt) {
            debugReportCallbackExt(instance, debugMessenger, NULL);
        }
    }
#endif
    vkDestroyInstance(instance, NULL);
}

void test(void) {
    start_drawing();
    {
        float scale = 1.0f;
        VkViewport viewport = { .width=(float) display_size.width * scale, .height= (float) display_size.height * scale };
        VkRect2D scissor = { .extent=display_size };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    end_drawing();
}

#endif //NYX_VULKAN_SETUP_WRAPPER_H
