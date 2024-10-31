//
// Created by Rohit Paul on 13-10-2024.
//

#ifndef NYX_VULKAN_SETUP_WRAPPER_H
#define NYX_VULKAN_SETUP_WRAPPER_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include "common/utils.h"
#include <android/asset_manager.h>
#include <malloc.h>
#include <string.h>

#define DEBUG

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

VkInstance instance;
VkPhysicalDevice gpu;
VkDevice device;
VkSurfaceKHR surface;
VkQueue queue;


VkSwapchainKHR swapchain;
VkExtent2D display_size;
VkImage images[2];
VkImageView imageView[2];
VkFramebuffer framebuffer[2];


VkRenderPass renderPass;
VkCommandPool commandPool;
VkCommandBuffer cmd_buff[2];
VkSemaphore semaphore;
VkFence fence;


void init_vulkan(struct ANativeWindow *window) {
    uint queue_family_index;

    VkExtent2D display_size;
    VkFormat format;

    // Instance
    {
        VkApplicationInfo applicationInfo = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion=VK_MAKE_VERSION(1, 1, 0)};
        VkInstanceCreateInfo instanceCreateInfo = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo=&applicationInfo};

#ifdef DEBUG
        const char *extension_names[3] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
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
            PFN_vkCreateDebugReportCallbackEXT debugReportCallbackExt = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance,
                                                                                                                                   "vkCreateDebugReportCallbackEXT");
            if (debugReportCallbackExt) {
                VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfoExt = {};
                debugReportCallbackCreateInfoExt.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                debugReportCallbackCreateInfoExt.flags = UINT32_MAX;
                debugReportCallbackCreateInfoExt.pfnCallback = vk_debug_report_callback;
                VK_CHECK(debugReportCallbackExt(instance, &debugReportCallbackCreateInfoExt, NULL, &debugMessenger))
            }
        }
#endif

    }
    // Surface
    {
        VkAndroidSurfaceCreateInfoKHR surfaceCreateInfoKhr = {.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window=window};
        VK_CHECK(vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfoKhr, NULL, &surface))
    }
    // Physical Device
    {
        unsigned int count = 1;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, &gpu))
    }
    // Logical Device
    {
        const char *const extension_name[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        uint32_t queFamily_count;
        VkQueueFamilyProperties *queueFamilyProperties;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo graphics_queue = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount=1, .pQueuePriorities=&priority};
        VkDeviceCreateInfo deviceCreateInfo = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount=1, .pQueueCreateInfos=&graphics_queue};
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queFamily_count, NULL);
        queueFamilyProperties = malloc(queFamily_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queFamily_count, queueFamilyProperties);
        for (queue_family_index = 0; queue_family_index < queFamily_count; queue_family_index++) {
            if (queueFamilyProperties[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)break;
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
        VkSwapchainCreateInfoKHR createInfoKhr = {.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface=surface, .minImageCount=2, .imageArrayLayers=1, .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE, .preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, .presentMode=VK_PRESENT_MODE_FIFO_KHR, .clipped=VK_TRUE};

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, NULL);
        formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, formats);

        for (format_index = 0; format_index < format_count; format_index++)
            if (formats[format_index].format == VK_FORMAT_R8G8B8A8_UNORM)break;

        display_size = capabilities.currentExtent;
        format = formats[format_index].format;

        createInfoKhr.imageExtent = display_size;
        createInfoKhr.imageColorSpace = formats[format_index].colorSpace;
        createInfoKhr.imageFormat = format;
        createInfoKhr.compositeAlpha = capabilities.supportedCompositeAlpha;
        VK_CHECK(vkCreateSwapchainKHR(device, &createInfoKhr, NULL, &swapchain))
        free(formats);
    }
//Getting Images and creating ImageView
    {
        uint image_count = 2;
        VkImageViewCreateInfo imageViewCreateInfo = {.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .viewType=VK_IMAGE_VIEW_TYPE_2D, .format=format, .components={.r=VK_COMPONENT_SWIZZLE_IDENTITY, .g=VK_COMPONENT_SWIZZLE_IDENTITY, .b=VK_COMPONENT_SWIZZLE_IDENTITY, .a=VK_COMPONENT_SWIZZLE_IDENTITY}, .subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .layerCount=1,.levelCount=1}};
        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images))
        for (int i = 0; i < 2; i++) {
            VkImageViewCreateInfo info = imageViewCreateInfo;
            info.image = images[i];
            VK_CHECK(vkCreateImageView(device, &info, NULL, imageView + i))
        }
    }
//Command Pool
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex=queue_family_index};
        VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool))
    }
//Command Buffer
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool=commandPool, .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount=2};
        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, cmd_buff))
    }
//Semaphore
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &semaphore))
    }
//RenderPass
    {

    }
}
void load_shader(char *code){}
void test_render() {
    uint index;
    VkSubmitInfo submitInfo = {.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount=1, .pCommandBuffers=cmd_buff, .signalSemaphoreCount=1, .pSignalSemaphores=&semaphore};
    VkPresentInfoKHR presentInfoKhr = {.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount=1, .pWaitSemaphores=&semaphore, .swapchainCount=1, .pSwapchains=&swapchain};
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 10, semaphore, 0, &index))
    presentInfoKhr.pImageIndices = &index;
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0))
    VK_CHECK(vkQueuePresentKHR(queue, &presentInfoKhr))
}

#endif //NYX_VULKAN_SETUP_WRAPPER_H
