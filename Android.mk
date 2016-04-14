LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := hello_led_service
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := weave_led_service.cpp
LOCAL_CFLAGS := -Wall -Werror
LOCAL_SHARED_LIBRARIES := \
    libbrillo \
    libc \
    libchrome \
    libhardware \
    libweaved
include $(BUILD_EXECUTABLE)
