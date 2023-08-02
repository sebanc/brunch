# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makefile for the IPTS touchscreen driver
#

sources := dkms.conf
sources += Makefile
sources += src/cmd.c
sources += src/cmd.h
sources += src/context.h
sources += src/control.c
sources += src/control.h
sources += src/desc.h
sources += src/hid.c
sources += src/hid.h
sources += src/Kconfig
sources += src/Makefile
sources += src/main.c
sources += src/mei.c
sources += src/mei.h
sources += src/receiver.c
sources += src/receiver.h
sources += src/resources.c
sources += src/resources.h
sources += src/spec-data.h
sources += src/spec-device.h
sources += src/spec-hid.h
sources += src/thread.c
sources += src/thread.h

KVERSION ?= $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build

all:
	$(MAKE) -C ../kernel O=out M=$(PWD)/src CONFIG_HID_IPTS=m modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/src CONFIG_HID_IPTS=m clean

check:
	$(KDIR)/scripts/checkpatch.pl -f -q --no-tree --strict --ignore EMBEDDED_FILENAME $(sources)

dkms-install: $(sources)
	./dkms.sh install

dkms-uninstall:
	./dkms.sh uninstall
