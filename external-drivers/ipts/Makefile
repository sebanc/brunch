# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makefile for the IPTS touchscreen driver
#

obj-$(CONFIG_MISC_IPTS) += ipts.o
ipts-objs := control.o
ipts-objs += mei.o
ipts-objs += receiver.o
ipts-objs += resources.o
ipts-objs += uapi.o

MODULE_NAME    := ipts
MODULE_VERSION := 2020-09-25

sources := Makefile
sources += Kconfig
sources += dkms.conf
sources += context.h
sources += control.c
sources += control.h
sources += mei.c
sources += protocol.h
sources += receiver.c
sources += receiver.h
sources += resources.c
sources += resources.h
sources += uapi.c
sources += uapi.h

KVERSION := $(shell uname -r)
KDIR := ../kernel
MDIR := /usr/src/$(MODULE_NAME)-$(MODULE_VERSION)

all:
	$(MAKE) -C $(KDIR) O=out M=$(PWD) CONFIG_MISC_IPTS=m modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_MISC_IPTS=m clean

check:
	$(KDIR)/scripts/checkpatch.pl -f -q --no-tree --ignore EMBEDDED_FILENAME $(sources)

check_strict:
	$(KDIR)/scripts/checkpatch.pl -f -q --no-tree --strict --ignore EMBEDDED_FILENAME $(sources)

dkms-install: $(sources)
	mkdir -p $(MDIR)
	cp -t $(MDIR) $(sources)
	dkms add $(MODULE_NAME)/$(MODULE_VERSION)
	dkms build $(MODULE_NAME)/$(MODULE_VERSION)
	dkms install $(MODULE_NAME)/$(MODULE_VERSION)

dkms-uninstall:
	modprobe -r $(MODULE_NAME) || true
	dkms uninstall $(MODULE_NAME)/$(MODULE_VERSION) || true
	dkms remove $(MODULE_NAME)/$(MODULE_VERSION) || true
	rm -rf $(MDIR)
