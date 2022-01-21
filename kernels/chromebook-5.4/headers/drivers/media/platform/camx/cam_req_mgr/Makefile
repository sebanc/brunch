ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_core
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_smmu
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_utils
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_req_mgr

obj-$(CONFIG_SPECTRA_CAMERA) += cam_req_mgr_core.o\
				cam_req_mgr_dev.o \
				cam_req_mgr_util.o \
				cam_req_mgr_workq.o \
				cam_mem_mgr.o \
				cam_req_mgr_timer.o \
				cam_req_mgr_debug.o \
				cam_buf_mgr.o
