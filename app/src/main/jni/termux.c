#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>
#include <malloc.h>
#include "core/color.h"
#include "ext/graphics/vulkan_layer/vk_wrapper.h"

static size_t read_asset(AAssetManager *aAssetManager, char *name, void **buffer) {
    AAsset *asset = AAssetManager_open(aAssetManager, name, AASSET_MODE_BUFFER);
    size_t file_length = AAsset_getLength(asset);
    *buffer = malloc(file_length);
    AAsset_read(asset, *buffer, file_length);
    AAsset_close(asset);
    return file_length;
}

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_init(JNIEnv *env, __attribute__((unused)) jobject thiz, jobject jni_surface, jobject asset_manager,
                                  __attribute__((unused)) jobject atlas) {
    Vulkan_init_info initInfo;
    AAssetManager *manager = AAssetManager_fromJava(env, asset_manager);
    initInfo.window = ANativeWindow_fromSurface(env, jni_surface);
    initInfo.vertex_shader.length = read_asset(manager, "shaders/shader.vert.spv", (void **) &initInfo.vertex_shader.code);
    initInfo.fragment_shader.length = read_asset(manager, "shaders/shader.frag.spv", (void **) &initInfo.fragment_shader.code);
    init_vulkan(&initInfo);
    free(initInfo.fragment_shader.code);
    free(initInfo.vertex_shader.code);
    ANativeWindow_release(initInfo.window);
    test_drawing();
}

JNIEXPORT void JNICALL
Java_com_termux_MainActivity_destroy(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jobject thiz) {
    cleanup_vulkan();
}
