ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_utils
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_req_mgr
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_core
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_cpas/include
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_cpas/cpas_top
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_cpas/camss_top

obj-$(CONFIG_SPECTRA_CAMERA) += cpas_top/
obj-$(CONFIG_SPECTRA_CAMERA) += camss_top/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cpas_soc.o cam_cpas_intf.o cam_cpas_hw.o
