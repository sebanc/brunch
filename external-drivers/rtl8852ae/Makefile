SHELL := /bin/bash
KVER  ?= $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
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
ifeq ("","$(wildcard MOK.der)")
NO_SKIP_SIGN := y
endif

EXTRA_CFLAGS += -O2
EXTRA_CFLAGS += -DCONFIG_RTW89_DEBUGMSG
EXTRA_CFLAGS += -DCONFIG_RTW89_DEBUGFS
KEY_FILE ?= MOK.der

obj-m += rtw89core.o
rtw89core-y +=  core.o \
		chan.o \
		mac80211.o \
		mac.o \
		phy.o \
		fw.o \
		cam.o \
		efuse.o \
		regd.o \
		sar.o \
		coex.o \
		ps.o \
		debug.o \
		ser.o \
		wow.o

obj-m += rtw_8852a.o
rtw_8852a-y := rtw8852a.o \
		    rtw8852a_table.o \
		    rtw8852a_rfk.o \
		    rtw8852a_rfk_table.o

obj-m += rtw_8852ae.o
rtw_8852ae-y := rtw8852ae.o

obj-m += rtw_8852b.o
rtw_8852b-y := rtw8852b.o \
		    rtw8852b_table.o \
		    rtw8852b_rfk.o \
		    rtw8852b_rfk_table.o

obj-m += rtw_8852be.o
rtw_8852be-y := rtw8852be.o

obj-m += rtw_8852c.o
rtw_8852c-y := rtw8852c.o \
	       rtw8852c_table.o \
	       rtw8852c_rfk.o \
	       rtw8852c_rfk_table.o

obj-m += rtw_8852ce.o
rtw_8852ce-y := rtw8852ce.o

#obj-m += rtw89debug.o
#rtw89debug-y := debug.o

obj-m += rtw89pci.o
rtw89pci-y := pci.o

ccflags-y += -D__CHECK_ENDIAN__

.PHONY: all install uninstall clean sign sign-install

all:
	$(MAKE) -C ../kernel O=out M=$(PWD) modules
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

	@echo "Install rtw89 SUCCESS"

uninstall:
	@rm -f $(MODDESTDIR)/rtw89*.ko

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
ifeq ($(NO_SKIP_SIGN), y)
	@openssl req -new -x509 -newkey rsa:2048 -keyout MOK.priv -outform DER -out MOK.der -nodes -days 36500 -subj "/CN=Custom MOK/"
	@mokutil --import MOK.der
else
	echo "Skipping key creation"
endif
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw89core.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw89pci.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw_8852a.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw_8852ae.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw_8852b.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw_8852be.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw_8852c.ko
	@$(KSRC)/scripts/sign-file sha256 MOK.priv MOK.der rtw_8852ce.ko

sign-install: all sign install

