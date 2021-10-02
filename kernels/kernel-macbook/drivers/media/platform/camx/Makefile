subdir-ccflags-y := \
	-I$(srctree)/$(src)/include \
	-I$(srctree)/$(src)/include/uapi

obj-$(CONFIG_SPECTRA_CAMERA) += \
	cam_core/ \
	cam_utils/

obj-$(CONFIG_QTI_CAMERA_SYNC) += cam_sync/
obj-$(CONFIG_QTI_CAMERA_SMMU) += cam_smmu/
obj-$(CONFIG_QTI_CAMERA_CRM) += cam_req_mgr/
obj-$(CONFIG_QTI_CPAS) += cam_cpas/ cam_cdm/
obj-$(CONFIG_QTI_ISP) += cam_isp/
obj-$(CONFIG_QTI_ICP_A5) += cam_icp/
obj-$(CONFIG_QTI_JPEG) += cam_jpeg/
obj-$(CONFIG_QTI_LRME) += cam_lrme/
obj-$(CONFIG_QTI_SENSORS) += cam_sensor_module/
