#include "render/vulkan_setup_wrapper.h"
#include "interface/term_interface.h"
#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>

AAssetManager *aAssetManager;

JNIEXPORT extern void JNICALL
Java_com_termux_Main_init(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jobject thiz, __attribute__((unused)) jobject jsurface) {
    struct ANativeWindow *window = ANativeWindow_fromSurface(env, jsurface);
    AAsset *file = AAssetManager_open(aAssetManager, "shaders/shader.vert.spv", AASSET_MODE_BUFFER);
    size_t len = AAsset_getLength(file);
    char *file_content= malloc(len*sizeof(char)),;
    AAsset_read(file, file_content, len);
    init_vulkan(window);
    test_render();
    init_terminal_session_manager();
}

JNIEXPORT void JNICALL
Java_com_termux_Main_load_1asset_1manager(JNIEnv *env, __attribute__((unused)) jobject thiz, jobject asset_manager) {
    aAssetManager = AAssetManager_fromJava(env, asset_manager);
}
