#
# Copyright (c) 2015 - 2018 DisplayLink (UK) Ltd.
#
# This file is subject to the terms and conditions of the GNU General Public
# License v2. See the file COPYING in the main directory of this archive for
# more details.
#
ccflags-y := -Iinclude/drm

evdi-y := evdi_drv.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_main.o evdi_fb.o evdi_gem.o evdi_stats.o evdi_painter.o evdi_debug.o evdi_cursor.o

evdi-$(CONFIG_COMPAT) += evdi_ioc32.o

obj-$(CONFIG_DRM_EVDI) := evdi.o

