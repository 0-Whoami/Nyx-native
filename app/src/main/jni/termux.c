#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>
#include "core/renderer.h"

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_init(JNIEnv *env, __attribute__((unused)) jobject thiz, jobject jni_surface,
                                  jobject asset_manager) {
//    Renderer_info rendererInfo={ANativeWindow_fromSurface(env,jni_surface), AAssetManager_fromJava(env,asset_manager),{1,24,45}};
    Vk_Context context;
    VkPhysicalDeviceProperties properties;
    VkPipeline pipelines[16];
    VkResult result;
    uint32_t i;
    create_vk_context(&context, VK_QUEUE_GRAPHICS_BIT);
    vkGetPhysicalDeviceProperties(context.gpu, &properties);

    for (i = 0; i < 16; i++) {
        result = vkCreateGraphicsPipelines(context.device, pipelineCache, 1, &pipelineCreateInfo, NULL, &pipelines[i]);
        if (result != VK_SUCCESS) {
            LOG("Pipeline creation failed at %u pipelines\n", i);
            break;
        }
//    }

    LOG("Max bound descriptor sets: %u\n", properties.limits.maxBoundDescriptorSets);

}

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_destroy(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jobject thiz) {

}
