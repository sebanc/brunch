EXTRA_CFLAGS += -DCONFIG_RTL8821C

ifeq ($(CONFIG_USB_HCI), y)
FILE_NAME = 8821cu
endif
ifeq ($(CONFIG_PCI_HCI), y)
FILE_NAME = 8821ce
endif
ifeq ($(CONFIG_SDIO_HCI), y)
FILE_NAME = 8821cs
endif

_HAL_HALMAC_FILES +=	hal/halmac/halmac_api.o

_HAL_HALMAC_FILES +=	hal/halmac/halmac_88xx/halmac_bb_rf_88xx.o \
			hal/halmac/halmac_88xx/halmac_cfg_wmac_88xx.o \
			hal/halmac/halmac_88xx/halmac_common_88xx.o \
			hal/halmac/halmac_88xx/halmac_efuse_88xx.o \
			hal/halmac/halmac_88xx/halmac_flash_88xx.o \
			hal/halmac/halmac_88xx/halmac_fw_88xx.o \
			hal/halmac/halmac_88xx/halmac_gpio_88xx.o \
			hal/halmac/halmac_88xx/halmac_init_88xx.o \
			hal/halmac/halmac_88xx/halmac_mimo_88xx.o \
			hal/halmac/halmac_88xx/halmac_pcie_88xx.o \
			hal/halmac/halmac_88xx/halmac_sdio_88xx.o \
			hal/halmac/halmac_88xx/halmac_usb_88xx.o

_HAL_HALMAC_FILES +=	hal/halmac/halmac_88xx/halmac_8821c/halmac_cfg_wmac_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_common_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_gpio_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_init_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_pcie_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_phy_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_pwr_seq_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_sdio_8821c.o \
			hal/halmac/halmac_88xx/halmac_8821c/halmac_usb_8821c.o

_HAL_INTFS_FILES +=	hal/hal_halmac.o

_HAL_INTFS_FILES +=	hal/rtl8821c/rtl8821c_halinit.o \
			hal/rtl8821c/rtl8821c_mac.o \
			hal/rtl8821c/rtl8821c_cmd.o \
			hal/rtl8821c/rtl8821c_phy.o \
			hal/rtl8821c/rtl8821c_dm.o \
			hal/rtl8821c/rtl8821c_ops.o \
			hal/rtl8821c/hal8821c_fw.o

_HAL_INTFS_FILES +=	hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_halinit.o \
			hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_halmac.o \
			hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_io.o \
			hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_xmit.o \
			hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_recv.o \
			hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_led.o \
			hal/rtl8821c/$(HCI_NAME)/rtl$(FILE_NAME)_ops.o

ifeq ($(CONFIG_SDIO_HCI), y)
_HAL_INTFS_FILES +=hal/efuse/$(RTL871X)/HalEfuseMask8821C_SDIO.o
endif
ifeq ($(CONFIG_USB_HCI), y)
_HAL_INTFS_FILES +=hal/efuse/$(RTL871X)/HalEfuseMask8821C_USB.o
endif
ifeq ($(CONFIG_PCI_HCI), y)
_HAL_INTFS_FILES +=hal/efuse/$(RTL871X)/HalEfuseMask8821C_PCIE.o
endif

_HAL_INTFS_FILES += $(_HAL_HALMAC_FILES)

_BTC_FILES += hal/btc/halbtc8821cwifionly.o
ifeq ($(CONFIG_BT_COEXIST), y)
_BTC_FILES += hal/btc/halbtc8821c1ant.o \
				hal/btc/halbtc8821c2ant.o
endif