LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE:= libtermux
LOCAL_SRC_FILES:= termux.c
LOCAL_LDLIBS := -llog -lvulkan -landroid
LOCAL_LDFLAGS += -s
include $(BUILD_SHARED_LIBRARY)