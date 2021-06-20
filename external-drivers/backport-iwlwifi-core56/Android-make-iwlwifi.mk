
# Run only this build if variant define the needed configuration
# e.g. Enabling iwlwifi for XMM6321
# BOARD_USING_INTEL_IWL := true      - this will enable iwlwifi building
# INTEL_IWL_BOARD_CONFIG := xmm6321  - the configuration, defconfig-xmm6321
# INTEL_IWL_USE_COMPAT_INSTALL := y  - this will use kernel modules installation
# INTEL_IWL_COMPAT_INSTALL_DIR := updates - the folder that the modules will be installed in
# INTEL_IWL_COMPAT_INSTALL_PATH ?= $(ANDROID_BUILD_TOP)/$(TARGET_OUT) - the install path for the modules
.PHONY: iwlwifi

INTEL_IWL_SRC_DIR := $(call my-dir)
INTEL_IWL_OUT_DIR := $(abspath $(PRODUCT_OUT)/iwlwifi)
INTEL_IWL_COMPAT_INSTALL_PATH ?= $(abspath $(TARGET_OUT))

ifeq ($(CROSS_COMPILE),)
ifneq ($(TARGET_TOOLS_PREFIX),)
CROSS_COMPILE := CROSS_COMPILE=$(notdir $(TARGET_TOOLS_PREFIX))
endif
endif

ifeq ($(CROSS_COMPILE),)
ifeq ($(TARGET_ARCH),arm)
$(warning You are building for an ARM platform, but no CROSS_COMPILE is set. This is likely an error.)
else
$(warning CROSS_COMPILE variant is not set; Defaulting to host gcc.)
endif
endif

ifeq ($(INTEL_IWL_USE_COMPAT_INSTALL),y)
INTEL_IWL_COMPAT_INSTALL := iwlwifi_install
INTEL_IWL_KERNEL_DEPEND := $(INSTALLED_KERNEL_TARGET)
else
# use system install
copy_modules_to_root: iwlwifi
ALL_KERNEL_MODULES += $(INTEL_IWL_OUT_DIR)
INTEL_IWL_KERNEL_DEPEND := build_bzImage
endif

ifeq ($(INTEL_IWL_USE_RM_MAC_CFG),y)
copy_modules_to_root: iwlwifi
INTEL_IWL_COMPAT_INSTALL_PATH := $(abspath $(KERNEL_OUT_MODINSTALL))
INTEL_IWL_KERNEL_DEPEND := modules_install
INTEL_IWL_RM_MAC_CFG_DEPEND := iwlwifi_rm_mac_cfg
INTEL_IWL_INSTALL_MOD_STRIP := INSTALL_MOD_STRIP=1
endif

# IWLWIFI_CONFIGURE is a rule to use a defconfig for iwlwifi
IWLWIFI_CONFIGURE := $(INTEL_IWL_OUT_DIR)/.config

KERNEL_OUT_ABS_DIR := $(abspath $(KERNEL_OUT_DIR))

iwlwifi: iwlwifi_build $(INTEL_IWL_COMPAT_INSTALL) $(INTEL_IWL_MOD_DEP)

INTEL_IWL_SRC_FILES := $(shell find $(INTEL_IWL_SRC_DIR) -path $(INTEL_IWL_SRC_DIR)/.git -prune -o -print)
$(INTEL_IWL_OUT_DIR): $(INTEL_IWL_SRC_FILES)
	@echo Syncing directory $(INTEL_IWL_SRC_DIR) to $(INTEL_IWL_OUT_DIR)
	@rsync -rt --del $(INTEL_IWL_SRC_DIR)/ $(INTEL_IWL_OUT_DIR)

$(IWLWIFI_CONFIGURE): $(INTEL_IWL_KERNEL_DEPEND) $(INTEL_IWL_OUT_DIR)
	@echo Configuring kernel module iwlwifi with defconfig-$(INTEL_IWL_BOARD_CONFIG)
	@$(MAKE) -C $(INTEL_IWL_OUT_DIR)/ ARCH=$(TARGET_ARCH) $(CROSS_COMPILE) KLIB_BUILD=$(KERNEL_OUT_ABS_DIR) defconfig-$(INTEL_IWL_BOARD_CONFIG)

iwlwifi_build: $(IWLWIFI_CONFIGURE)
	@$(info Building kernel module iwlwifi in $(INTEL_IWL_OUT_DIR))
	@$(MAKE) -C $(INTEL_IWL_OUT_DIR)/ ARCH=$(TARGET_ARCH) $(CROSS_COMPILE) KLIB_BUILD=$(KERNEL_OUT_ABS_DIR)

iwlwifi_install: iwlwifi_build $(INTEL_IWL_RM_MAC_CFG_DEPEND)
	@$(info Installing kernel modules in $(INTEL_IWL_COMPAT_INSTALL_PATH))
	@$(MAKE) -C $(KERNEL_OUT_ABS_DIR) ARCH=$(TARGET_ARCH) M=$(INTEL_IWL_OUT_DIR)/ INSTALL_MOD_DIR=$(INTEL_IWL_COMPAT_INSTALL_DIR) INSTALL_MOD_PATH=$(INTEL_IWL_COMPAT_INSTALL_PATH) $(INTEL_IWL_INSTALL_MOD_STRIP) modules_install

iwlwifi_rm_mac_cfg: iwlwifi_build
	$(info Remove kernel cfg80211.ko and mac80211.ko)
	@find $(KERNEL_OUT_MODINSTALL)/lib/modules/ -name "mac80211.ko" | xargs rm -f
	@find $(KERNEL_OUT_MODINSTALL)/lib/modules/ -name "cfg80211.ko" | xargs rm -f

.PHONY: iwlwifi_clean
iwlwifi_clean:
	rm -rf $(INTEL_IWL_OUT_DIR)
