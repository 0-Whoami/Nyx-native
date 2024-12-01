#ifndef UTILS
#define UTILS

#include <android/log.h>

#define DEBUG


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, low, high) (x >= low && x <= high)
#define LIMIT(x, a, b) (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define CELI(a, b) ((a+b-1)/b)

#define LOG(...)  __android_log_print(ANDROID_LOG_INFO, "Native", __VA_ARGS__)

#endif
