ifneq ($(TARGET_SIMULATOR),true)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES += p2p_api_test_linux.c p2p_ui_test_linux.c
LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_MODULE = P2P_UI
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

endif
