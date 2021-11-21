ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_utils
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_cpas/include
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_sensor_module/cam_sensor_utils
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_sensor_module/cam_sensor_io
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_sensor_module/cam_cci
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_sensor_module/cam_eeprom
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_req_mgr
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_smmu/
ccflags-y += -I$(srctree)/drivers/media/platform/camx/cam_core/

obj-$(CONFIG_QTI_SENSORS) += \
	cam_sensor_io.o

obj-$(CONFIG_QTI_CCI) += \
	cam_sensor_qup_i2c.o

obj-$(CONFIG_QTI_I2C) += \
	cam_sensor_cci_i2c.o

obj-$(CONFIG_QTI_SPI) += \
	cam_sensor_spi.o
