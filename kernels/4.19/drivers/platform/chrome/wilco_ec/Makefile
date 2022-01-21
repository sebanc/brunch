# SPDX-License-Identifier: GPL-2.0

wilco_ec-objs				:= core.o keyboard_leds.o mailbox.o \
					   properties.o sysfs.o
obj-$(CONFIG_WILCO_EC)			+= wilco_ec.o
wilco_ec_debugfs-objs			:= debugfs.o
obj-$(CONFIG_WILCO_EC_DEBUGFS)		+= wilco_ec_debugfs.o
wilco_ec_events-objs			:= event.o
obj-$(CONFIG_WILCO_EC_EVENTS)		+= wilco_ec_events.o
wilco_charge_schedule-objs		:= charge_schedule.o \
					   charge_schedule_sysfs.o
obj-$(CONFIG_WILCO_EC_CHARGE_SCHEDULE)	+= wilco_charge_schedule.o
wilco_ec_telem-objs			:= telemetry.o
obj-$(CONFIG_WILCO_EC_TELEMETRY)	+= wilco_ec_telem.o
