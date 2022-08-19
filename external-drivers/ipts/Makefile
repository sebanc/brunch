# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makefile for the IPTS touchscreen driver
#

MODULE_NAME    := ipts
MODULE_VERSION := 1.0.0

sources += dkms.conf
sources := Makefile
sources += src/cmd.c
sources += src/cmd.h
sources += src/context.h
sources += src/control.c
sources += src/control.h
sources += src/desc.h
sources += src/hid.c
sources += src/hid.h
sources += src/Kconfig
sources := src/Makefile
sources += src/mei.c
sources += src/receiver.c
sources += src/receiver.h
sources += src/resources.c
sources += src/resources.h
sources += src/spec-data.h
sources += src/spec-device.h
sources += src/spec-hid.h

KVERSION := $(shell uname -r)
KDIR := ../kernel
MDIR := /usr/src/$(MODULE_NAME)-$(MODULE_VERSION)

all:
	$(MAKE) -C $(KDIR) O=out M=$(PWD)/src CONFIG_HID_IPTS=m modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/src CONFIG_HID_IPTS=m clean

check:
	$(KDIR)/scripts/checkpatch.pl -f -q --no-tree --strict --ignore EMBEDDED_FILENAME $(sources)

dkms-install: $(sources)
	cp -r $(shell pwd) $(MDIR)
	dkms add $(MODULE_NAME)/$(MODULE_VERSION)
	dkms build $(MODULE_NAME)/$(MODULE_VERSION)
	dkms install $(MODULE_NAME)/$(MODULE_VERSION)

dkms-uninstall:
	modprobe -r $(MODULE_NAME) || true
	dkms uninstall $(MODULE_NAME)/$(MODULE_VERSION) || true
	dkms remove $(MODULE_NAME)/$(MODULE_VERSION) || true
	rm -rf $(MDIR)
