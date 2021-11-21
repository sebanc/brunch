SHELL := /bin/sh
KVER  ?= $(shell uname -r)
KSRC := ../kernel
FIRMWAREDIR := /lib/firmware/
PWD := $(shell pwd)
CLR_MODULE_FILES := *.mod.c *.mod *.o .*.cmd *.ko *~ .tmp_versions* modules.order Module.symvers
SYMBOL_FILE := Module.symvers
# Handle the move of the entire rtw88 tree
ifneq ("","$(wildcard /lib/modules/$(KVER)/kernel/drivers/net/wireless/realtek)")
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/realtek/rtw89
else
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/rtw89
endif
#Handle the compression option for modules in 3.18+
ifneq ("","$(wildcard $(MODDESTDIR)/*.ko.gz)")
COMPRESS_GZIP := y
endif
ifneq ("","$(wildcard $(MODDESTDIR)/*.ko.xz)")
COMPRESS_XZ := y
endif
MOK_KEY_DIR ?= /var/lib/shim-signed/mok

EXTRA_CFLAGS += -O2
EXTRA_CFLAGS += -DCONFIG_RTW89_DEBUGMSG
EXTRA_CFLAGS += -DCONFIG_RTW89_DEBUGFS

obj-m += rtl8852ae.o
rtl8852ae-y +=  core.o \
		debug.o \
		mac80211.o \
		mac.o \
		phy.o \
		fw.o \
		rtw8852a.o \
		rtw8852a_table.o \
		rtw8852a_rfk.o \
		rtw8852a_rfk_table.o \
		cam.o \
		efuse.o \
		regd.o \
		coex.o \
		ps.o \
		sar.o \
		ser.o \
		util.o \
		pci.o

ccflags-y += -D__CHECK_ENDIAN__

.PHONY: all install uninstall clean sign sign-install

all:
	$(MAKE) -C $(KSRC) O=out M=$(PWD) modules
install: all
	@rm -f $(MODDESTDIR)/rtw89*.ko

	@mkdir -p $(MODDESTDIR)
	@install -p -D -m 644 *.ko $(MODDESTDIR)
ifeq ($(COMPRESS_GZIP), y)
	@gzip -f $(MODDESTDIR)/*.ko
endif
ifeq ($(COMPRESS_XZ), y)
	@xz -f $(MODDESTDIR)/*.ko
endif
	@depmod -a $(KVER)

	@mkdir -p /lib/firmware/rtw89/
	cp *.bin /lib/firmware/rtw89/.

	@echo "Install rtw89 SUCCESS"

uninstall:
	@rm -f $(MODDESTDIR)/rtw89.ko

	@depmod -a

	@echo "Uninstall rtw89 SUCCESS"

clean:
	@rm -fr *.mod.c *.mod *.o .*.cmd .*.o.cmd *.ko *~ .*.o.d .cache.mk
	@rm -fr .tmp_versions
	@rm -fr Modules.symvers
	@rm -fr Module.symvers
	@rm -fr Module.markers
	@rm -fr modules.order

sign:
	kmodsign sha512 $(MOK_KEY_DIR)/MOK.priv $(MOK_KEY_DIR)/MOK.der rtw89core.ko
	kmodsign sha512 $(MOK_KEY_DIR)/MOK.priv $(MOK_KEY_DIR)/MOK.der rtw89pci.ko

sign-install: all sign install
