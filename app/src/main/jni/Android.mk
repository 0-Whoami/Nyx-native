LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE:= libtermux
LOCAL_SRC_FILES:= termux.c $(wildcard $(LOCAL_PATH)/*/*.c) $(wildcard $(LOCAL_PATH)/*/*/*.c)  $(wildcard $(LOCAL_PATH)/*/*/*/*.c)
LOCAL_CFLAGS := -std=c11
LOCAL_LDLIBS := -llog -lvulkan -landroid
include $(BUILD_SHARED_LIBRARY)