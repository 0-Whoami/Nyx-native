//
// Created by Rohit Paul on 13-12-2024.
//
#include "jni_wrapper.h"

static JavaVM *vm;

jint JNI_OnLoad(JavaVM *javaVm, __attribute__((unused)) void *reserved) {
    vm = javaVm;
    return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM *javaVm, __attribute__((unused)) void *reserved) {
    (*javaVm)->DestroyJavaVM(vm);
}

jintArray get_int_array(JNIEnv *env,const int * const c_array,const int size) {
    jintArray cp_array = (*env)->NewIntArray(env, size);
    (*env)->SetIntArrayRegion(env, cp_array, 0, size, c_array);
    return cp_array;
}

JNIEnv *get_env(void) {
    JNIEnv *env;
    (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
    (*vm)->AttachCurrentThread(vm, &env, NULL);
    return env;
}

void detach_env(void) {
    (*vm)->DetachCurrentThread(vm);
}
