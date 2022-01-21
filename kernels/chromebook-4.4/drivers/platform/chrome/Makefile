
obj-$(CONFIG_CHROMEOS)		+= chromeos.o
obj-$(CONFIG_CHROMEOS_OF_FIRMWARE) += chromeos_arm.o
obj-$(CONFIG_CHROMEOS_LAPTOP)	+= chromeos_laptop.o
obj-$(CONFIG_CHROMEOS_PSTORE)	+= chromeos_pstore.o
obj-$(CONFIG_CHROMEOS_TBMC)	+= chromeos_tbmc.o
cros_ec_ctl-objs		:= cros_ec_dev.o cros_ec_sysfs.o \
				   cros_ec_lightbar.o cros_ec_vbc.o \
				   cros_ec_debugfs.o
obj-$(CONFIG_CROS_EC_CHARDEV)   += cros_ec_ctl.o
cros_ec_lpcs-y		+= cros_ec_lpc.o cros_ec_lpc_reg.o
cros_ec_lpcs-$(CONFIG_CROS_EC_LPC_MEC)	+= cros_ec_lpc_mec.o
cros_ec_lpcs-objs		:= $(cros_ec_lpcs-y)
obj-$(CONFIG_CROS_EC_LPC)       += cros_ec_lpcs.o
obj-$(CONFIG_CROS_EC_PROTO)	+= cros_ec_proto.o
cros-ec-sensorhub-objs			:= cros_ec_sensorhub.o cros_ec_sensorhub_ring.o
obj-$(CONFIG_CROS_EC_SENSORHUB)		+= cros-ec-sensorhub.o
obj-$(CONFIG_CROS_KBD_LED_BACKLIGHT)	+= cros_kbd_led_backlight.o
