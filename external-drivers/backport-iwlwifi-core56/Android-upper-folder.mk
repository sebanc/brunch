LOCAL_PATH := $(call my-dir)

ifneq ($(INTEL_IWL_MODULE_SUB_FOLDER),)
include $(LOCAL_PATH)/$(INTEL_IWL_MODULE_SUB_FOLDER)/Android.mk
endif
