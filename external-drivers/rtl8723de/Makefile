# SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
KERNELDIR ?= ../kernel
PWD       := $(shell pwd)

CONFIG_RTW88_CORE=m
CONFIG_RTW88_PCI=m
CONFIG_RTW88_8723DE=y
ccflags-y += -DCONFIG_RTW88_8723DE=y

obj-$(CONFIG_RTW88_CORE)	+= rtl8723de.o
rtl8723de-y += main.o \
	   mac80211.o \
	   util.o \
	   debug.o \
	   tx.o \
	   rx.o \
	   mac.o \
	   phy.o \
	   coex.o \
	   efuse.o \
	   fw.o \
	   ps.o \
	   sec.o \
	   bf.o \
	   wow.o \
	   sar.o \
	   regd.o \
	   rtw8822b.o \
	   rtw8822b_table.o \
	   rtw8822c.o \
	   rtw8822c_table.o \
	   rtw8723d.o \
	   rtw8723d_table.o \
	   pci.o

all:
	$(MAKE) -C $(KERNELDIR) O=out M=$(PWD)
