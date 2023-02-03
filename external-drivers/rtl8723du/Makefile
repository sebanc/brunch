EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
EXTRA_CFLAGS += -O1

GCC_VER_49 := $(shell echo `$(CC) -dumpversion | cut -f1-2 -d.` \>= 4.9 | bc )
ifeq ($(GCC_VER_49),1)
EXTRA_CFLAGS += -Wno-date-time	# Fix compile error && warning on gcc 4.9 and later
endif

EXTRA_CFLAGS += -I$(src)/include

#EXTRA_LDFLAGS += --strip-debug

ifeq ("","$(wildcard MOK.der)")
NO_SKIP_SIGN := y
endif

########################## Features ###########################
CONFIG_EFUSE_CONFIG_FILE = y
CONFIG_LOAD_PHY_PARA_FROM_FILE = y
CONFIG_RTW_NAPI = y
CONFIG_RTW_WIFI_HAL = y
########################## Debug ###########################
# default log level is _DRV_INFO_ = 2,
# please refer to "How_to_set_driver_debug_log_level.doc" to set the available level.
CONFIG_RTW_LOG_LEVEL = 2
######################## Wake On Lan ##########################
CONFIG_WAKEUP_TYPE = 0x7 #bit2: deauth, bit1: unicast, bit0: magic pkt.
###############################################################

CONFIG_DRVEXT_MODULE = n

export TopDIR ?= $(shell pwd)

########### COMMON  #################################
HCI_NAME = usb

_OS_INTFS_FILES :=	os_dep/osdep_service.o \
			os_dep/os_intfs.o \
			os_dep/usb_intf.o \
			os_dep/usb_ops_linux.o \
			os_dep/ioctl_linux.o \
			os_dep/xmit_linux.o \
			os_dep/mlme_linux.o \
			os_dep/recv_linux.o \
			os_dep/ioctl_cfg80211.o \
			os_dep/rtw_cfgvendor.o \
			os_dep/wifi_regd.o \
			os_dep/rtw_android.o \
			os_dep/rtw_proc.o

_HAL_INTFS_FILES :=	hal/hal_intf.o \
			hal/hal_com.o \
			hal/hal_com_phycfg.o \
			hal/hal_phy.o \
			hal/hal_dm.o \
			hal/hal_btcoex_wifionly.o \
			hal/hal_btcoex.o \
			hal/hal_usb.o \
			hal/hal_usb_led.o


EXTRA_CFLAGS += -I$(src)/platform
_PLATFORM_FILES := platform/platform_ops.o

EXTRA_CFLAGS += -I$(src)/hal

include $(TopDIR)/hal/phydm/phydm.mk
########### HAL_RTL8723D #################################

MODULE_NAME = 8723du

_HAL_INTFS_FILES += hal/HalPwrSeqCmd.o hal/Hal8723DPwrSeq.o

_HAL_INTFS_FILES +=	hal/rtl8723d_hal_init.o \
			hal/rtl8723d_phycfg.o \
			hal/rtl8723d_rf6052.o \
			hal/rtl8723d_dm.o \
			hal/rtl8723d_rxdesc.o \
			hal/rtl8723d_cmd.o \
			hal/hal8723d_fw.o


_HAL_INTFS_FILES +=	\
			hal/usb_halinit.o \
			hal/rtl8723du_led.o \
			hal/rtl8723du_xmit.o \
			hal/rtl8723du_recv.o

_HAL_INTFS_FILES += hal/usb_ops.o

_HAL_INTFS_FILES +=hal/HalEfuseMask8723D_USB.o
_BTC_FILES += hal/halbtc8723d1ant.o \
				hal/halbtc8723d2ant.o

########### END OF PATH  #################################

ifeq ($(CONFIG_INTEL_WIDI), y)
EXTRA_CFLAGS += -DCONFIG_INTEL_WIDI
endif

ifeq ($(CONFIG_EFUSE_CONFIG_FILE), y)
EXTRA_CFLAGS += -DCONFIG_EFUSE_CONFIG_FILE

#EFUSE_MAP_PATH
USER_EFUSE_MAP_PATH ?=
ifneq ($(USER_EFUSE_MAP_PATH),)
EXTRA_CFLAGS += -DEFUSE_MAP_PATH=\"$(USER_EFUSE_MAP_PATH)\"
else
EXTRA_CFLAGS += -DEFUSE_MAP_PATH=\"/system/etc/wifi/wifi_efuse_$(MODULE_NAME).map\"
endif

#WIFIMAC_PATH
USER_WIFIMAC_PATH ?=
ifneq ($(USER_WIFIMAC_PATH),)
EXTRA_CFLAGS += -DWIFIMAC_PATH=\"$(USER_WIFIMAC_PATH)\"
else
EXTRA_CFLAGS += -DWIFIMAC_PATH=\"/data/wifimac.txt\"
endif

endif

ifeq ($(CONFIG_LOAD_PHY_PARA_FROM_FILE), y)
EXTRA_CFLAGS += -DCONFIG_LOAD_PHY_PARA_FROM_FILE
#EXTRA_CFLAGS += -DREALTEK_CONFIG_PATH_WITH_IC_NAME_FOLDER
EXTRA_CFLAGS += -DREALTEK_CONFIG_PATH=\"/lib/firmware/\"
endif

ifeq ($(CONFIG_RTW_NAPI), y)
EXTRA_CFLAGS += -DCONFIG_RTW_NAPI
endif

ifeq ($(CONFIG_RTW_WIFI_HAL), y)
EXTRA_CFLAGS += -DCONFIG_RTW_WIFI_HAL
endif

EXTRA_CFLAGS += -DCONFIG_RTW_DEBUG
EXTRA_CFLAGS += -DRTW_LOG_LEVEL=$(CONFIG_RTW_LOG_LEVEL)

EXTRA_CFLAGS += -DDM_ODM_SUPPORT_TYPE=0x04

SUBARCH := $(shell uname -m | sed -e s/i.86/i386/ | sed -e s/ppc/powerpc/ | sed -e s/armv.l/arm/)
ARCH ?= $(SUBARCH)
CROSS_COMPILE ?=
KVER  := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/
INSTALL_PREFIX :=
STAGINGMODDIR := /lib/modules/$(KVER)/kernel/drivers/staging

USER_MODULE_NAME ?=
ifneq ($(USER_MODULE_NAME),)
MODULE_NAME := $(USER_MODULE_NAME)
endif

ifneq ($(KERNELRELEASE),)

rtk_core :=	core/rtw_cmd.o \
		core/rtw_security.o \
		core/rtw_debug.o \
		core/rtw_io.o \
		core/rtw_ioctl_set.o \
		core/rtw_ieee80211.o \
		core/rtw_mlme.o \
		core/rtw_mlme_ext.o \
		core/rtw_mi.o \
		core/rtw_wlan_util.o \
		core/rtw_pwrctrl.o \
		core/rtw_rf.o \
		core/rtw_recv.o \
		core/rtw_sta_mgt.o \
		core/rtw_ap.o \
		core/rtw_xmit.o	\
		core/rtw_p2p.o \
		core/rtw_br_ext.o \
		core/rtw_sreset.o \
		core/rtw_btcoex_wifionly.o \
		core/rtw_btcoex.o \
		core/rtw_odm.o \
		core/rtw_rm.o \
		core/rtw_efuse.o 

$(MODULE_NAME)-y += $(rtk_core)

$(MODULE_NAME)-$(CONFIG_INTEL_WIDI) += core/rtw_intel_widi.o

$(MODULE_NAME)-y += $(_OS_INTFS_FILES)
$(MODULE_NAME)-y += $(_HAL_INTFS_FILES)
$(MODULE_NAME)-y += $(_PHYDM_FILES)
$(MODULE_NAME)-y += $(_BTC_FILES)
$(MODULE_NAME)-y += $(_PLATFORM_FILES)

obj-m := $(MODULE_NAME).o

else

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C ../kernel O=out M=$(shell pwd)  modules

strip:
	$(CROSS_COMPILE)strip $(MODULE_NAME).ko --strip-unneeded

install:
	install -p -m 644 $(MODULE_NAME).ko  $(MODDESTDIR)
	/sbin/depmod -a ${KVER}

uninstall:
	rm -f $(MODDESTDIR)/$(MODULE_NAME).ko
	/sbin/depmod -a ${KVER}

backup_rtlwifi:
	@echo "Making backup rtlwifi drivers"
ifneq (,$(wildcard $(STAGINGMODDIR)/rtl*))
	@tar cPf $(wildcard $(STAGINGMODDIR))/backup_rtlwifi_driver.tar $(wildcard $(STAGINGMODDIR)/rtl*)
	@rm -rf $(wildcard $(STAGINGMODDIR)/rtl*)
endif
ifneq (,$(wildcard $(MODDESTDIR)realtek))
	@tar cPf $(MODDESTDIR)backup_rtlwifi_driver.tar $(MODDESTDIR)realtek
	@rm -fr $(MODDESTDIR)realtek
endif
ifneq (,$(wildcard $(MODDESTDIR)rtl*))
	@tar cPf $(MODDESTDIR)../backup_rtlwifi_driver.tar $(wildcard $(MODDESTDIR)rtl*)
	@rm -fr $(wildcard $(MODDESTDIR)rtl*)
endif
	@/sbin/depmod -a ${KVER}
	@echo "Please reboot your system"

restore_rtlwifi:
	@echo "Restoring backups"
ifneq (,$(wildcard $(STAGINGMODDIR)/backup_rtlwifi_driver.tar))
	@tar xPf $(STAGINGMODDIR)/backup_rtlwifi_driver.tar
	@rm $(STAGINGMODDIR)/backup_rtlwifi_driver.tar
endif
ifneq (,$(wildcard $(MODDESTDIR)backup_rtlwifi_driver.tar))
	@tar xPf $(MODDESTDIR)backup_rtlwifi_driver.tar
	@rm $(MODDESTDIR)backup_rtlwifi_driver.tar
endif
ifneq (,$(wildcard $(MODDESTDIR)../backup_rtlwifi_driver.tar))
	@tar xPf $(MODDESTDIR)../backup_rtlwifi_driver.tar
	@rm $(MODDESTDIR)../backup_rtlwifi_driver.tar
endif
	@/sbin/depmod -a ${KVER}
	@echo "Please reboot your system"

DRIVER_VERSION = $(shell grep "\#define DRIVERVERSION" include/drv_types.h | awk '{print $$3}' | tr -d v\")

dkms_install:
	@mkdir -vp /usr/src/$(MODULE_NAME)-$(DRIVER_VERSION)
	cp -r * /usr/src/$(MODULE_NAME)-$(DRIVER_VERSION)
	dkms add -m $(MODULE_NAME) -v $(DRIVER_VERSION)
	+ dkms build -m $(MODULE_NAME) -v $(DRIVER_VERSION)
	dkms install -m $(MODULE_NAME) -v $(DRIVER_VERSION)
	dkms status -m $(MODULE_NAME)

dkms_remove:
	dkms remove $(MODULE_NAME)/$(DRIVER_VERSION) --all
	rm -rf /usr/src/$(MODULE_NAME)-$(DRIVER_VERSION)

config_r:
	@echo "make config"
	/bin/bash script/Configure script/config.in


.PHONY: modules clean

clean:
	#$(MAKE) -C $(KSRC) M=$(shell pwd) clean
	@cd hal ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/phydm/halrf ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/phydm/halrf/rtl8723d ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/phydm/txbf ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/efuse/rtl8723d ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/phydm/rtl8723d ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/rtl8723d/usb ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko *.d .*.cmd
	@cd hal/phydm ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d .*.cmd
	@cd core ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd core/crypto ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd core/monitor ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd core/mesh ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd core/wds ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd core/efuse ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd os_dep ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd os_dep/linux ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@cd platform ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko .*.d
	@rm -fr Module.symvers ; rm -fr Module.markers ; rm -fr modules.order
	@rm -fr *.mod.c *.mod *.o .*.cmd *.ko *~ .*.d
	@rm -fr .tmp_versions
endif

sign:
ifeq ($(NO_SKIP_SIGN), y)
	@openssl req -new -x509 -newkey rsa:2048 -keyout MOK.priv -outform DER -out MOK.der -nodes -days 36500 -subj "/CN=Custom MOK/"
	@mokutil --import MOK.der
else
	echo "Skipping key creation"
endif
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der 8723du.ko

sign-install: all sign install

