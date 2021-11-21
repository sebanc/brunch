# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2017 - 2020 Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ccflags-y += -DIPU_TPG_FRAME_SYNC -DIPU_PSYS_GPC \
		-DIPU_ISYS_GPC

intel-ipu6-objs				+= ../ipu.o \
					   ../ipu-bus.o \
					   ../ipu-dma.o \
					   ../ipu-mmu.o \
					   ../ipu-buttress.o \
					   ../ipu-trace.o \
					   ../ipu-cpd.o \
					   ipu6.o \
					   ../ipu-fw-com.o

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6.o

intel-ipu6-isys-objs			+= ../ipu-isys.o \
					   ../ipu-isys-csi2.o \
					   ipu6-isys.o \
					   ipu6-isys-phy.o \
					   ipu6-isys-csi2.o \
					   ipu6-isys-gpc.o \
					   ../ipu-isys-csi2-be-soc.o \
					   ../ipu-fw-isys.o \
					   ../ipu-isys-video.o \
					   ../ipu-isys-queue.o \
					   ../ipu-isys-subdev.o
intel-ipu6-isys-objs			+= ../ipu-isys-csi2-be.o

ifdef CONFIG_VIDEO_INTEL_IPU_TPG
intel-ipu6-isys-objs                    += ../ipu-isys-tpg.o
endif

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6-isys.o

intel-ipu6-psys-objs			+= ../ipu-psys.o \
					   ipu6-psys.o \
					   ipu-resources.o \
					   ipu6-psys-gpc.o \
					   ipu6-l-scheduler.o \
					   ipu6-ppg.o

intel-ipu6-psys-objs			+= ipu-fw-resources.o \
					   ipu6-fw-resources.o \
					   ipu6se-fw-resources.o \
					   ipu6ep-fw-resources.o \
					   ../ipu-fw-psys.o

ifeq ($(CONFIG_COMPAT),y)
intel-ipu6-psys-objs			+= ../ipu-psys-compat32.o
endif

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6-psys.o

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -I$(srcpath)/$(src)/
