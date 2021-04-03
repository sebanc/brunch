override CHROMIUMOS_KERNEL := 1
override CHROMIUMOS_KERNEL_HAS_DMA_FENCE := 1
override HOST_ALL_ARCH := host_x86_64 host_i386
override KERNEL_CC := aarch64-linux-gnu-gcc-5
override KERNEL_DRIVER_DIR := drivers/gpu/drm/img-rogue/1.13
override METAG_VERSION_NEEDED := 2.8.1.0.3
override MIPS_VERSION_NEEDED := 2014.07-1
override PDVFS_COM := PDVFS_COM_HOST
override PDVFS_COM_AP := 2
override PDVFS_COM_HOST := 1
override PDVFS_COM_IMG_CLKDIV := 4
override PDVFS_COM_PMC := 3
override PVRSRV_MODNAME := pvrsrvkm_1_13
override PVRSYNC_MODNAME := pvr_sync
override PVR_ARCH := rogue
override PVR_BUILD_DIR := mt8173_linux
override PVR_GPIO_MODE := PVR_GPIO_MODE_GENERAL
override PVR_GPIO_MODE_GENERAL := 1
override PVR_GPIO_MODE_POWMON_PIN := 2
override PVR_HANDLE_BACKEND := idr
override PVR_SYSTEM := mt8173
override RGX_NUM_OS_SUPPORTED := 1
override RGX_TIMECORR_CLOCK := mono
override SUPPORT_BUFFER_SYNC := 1
override SUPPORT_DMABUF_BRIDGE := 1
override SUPPORT_LINUX_DVFS := 1
override SUPPORT_NATIVE_FENCE_SYNC := 1
override SUPPORT_PHYSMEM_TEST := 1
override SUPPORT_POWMON_COMPONENT := 1
override SUPPORT_RGX := 1
override SUPPORT_USC_BREAKPOINT := 1
override VMM_TYPE := stub
override WINDOW_SYSTEM := lws-generic
ifeq ($(CONFIG_DRM_POWERVR_ROGUE_DEBUG),y)
override BUILD := debug
override PVRSRV_ENABLE_GPU_MEMORY_INFO := 1
override PVR_BUILD_TYPE := debug
override PVR_SERVICES_DEBUG := 1
else
override BUILD := release
override PVR_BUILD_TYPE := release
endif
ifeq ($(CONFIG_DRM_POWERVR_ROGUE_PDUMP),y)
override PDUMP := 1
endif
