# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makefile for the IPTS touchscreen driver
#

sources := dkms.conf
sources += Makefile
sources += src/context.h
sources += src/control.c
sources += src/control.h
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
sources += src/spec-dma.h
sources += src/spec-hid.h
sources += src/spec-mei.h
sources += src/thread.c
sources += src/thread.h

KVERSION ?= $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build

DEBUG ?= y

all:
	$(MAKE) -C ../kernel O=out M=$(PWD)/src CONFIG_HID_IPTS=m IPTS_DEBUG=$(DEBUG) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/src CONFIG_HID_IPTS=m IPTS_DEBUG=$(DEBUG) clean

check:
	$(KDIR)/scripts/checkpatch.pl -f -q --no-tree --strict --ignore EMBEDDED_FILENAME,UNCOMMENTED_DEFINITION $(sources)

dkms-install: $(sources)
	./dkms.sh install

dkms-uninstall:
	./dkms.sh uninstall
