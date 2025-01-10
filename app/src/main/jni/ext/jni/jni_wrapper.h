//
// Created by Rohit Paul on 13-12-2024.
//

#ifndef NYX_JNI_WRAPPER_H
#define NYX_JNI_WRAPPER_H

#include <jni.h>

JNIEnv *get_env(void);
jintArray get_int_array(JNIEnv *env,const int *c_array, int size);
void detach_env(void);
#endif //NYX_JNI_WRAPPER_H
