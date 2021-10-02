# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2019 MediaTek Inc.
#

$(info $(srctree))
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-mdp3

mtk_dip-objs := \
mtk_dip-v4l2.o \
mtk_dip-dev.o \
mtk_dip-sys.o

obj-$(CONFIG_VIDEO_MEDIATEK_ISP_DIP) += mtk_dip.o
