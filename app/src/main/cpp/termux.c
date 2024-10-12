#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#define LOG(...)  __android_log_print(ANDROID_LOG_INFO, "Native", __VA_ARGS__)


JNIEXPORT void JNICALL
Java_com_termux_Main_print(JNIEnv *env, jobject thiz) {
    for (long i = 0; i <= 1e10; ++i);
}
