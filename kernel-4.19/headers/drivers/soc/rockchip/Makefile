#
# Rockchip Soc drivers
#
# TODO(mka): Remove when https://bugs.llvm.org/show_bug.cgi?id=10030 is fixed
subdir-ccflags-y := $(call cc-disable-warning, shift-count-negative)

obj-y += dmc-sync.o
obj-$(CONFIG_ROCKCHIP_GRF) += grf.o
obj-$(CONFIG_ROCKCHIP_PM_DOMAINS) += pm_domains.o
obj-$(CONFIG_ROCKCHIP_RK3288_PANIC_HANDLER) += rk3288_panic_handler.o
