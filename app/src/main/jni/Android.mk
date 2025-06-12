LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE:= libtermux
LOCAL_SRC_FILES:= termux.c $(wildcard $(LOCAL_PATH)/*/*.c) $(wildcard $(LOCAL_PATH)/*/*/*.c)  $(wildcard $(LOCAL_PATH)/*/*/*/*.c)
LOCAL_CFLAGS := -std=c11
LOCAL_LDLIBS := -llog -lvulkan -landroid
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_CFLAGS += -march=armv7-a -mtune=cortex-a55 -mfpu=neon-vfpv4 -mfloat-abi=softfp -mthumb
endif
include $(BUILD_SHARED_LIBRARY)