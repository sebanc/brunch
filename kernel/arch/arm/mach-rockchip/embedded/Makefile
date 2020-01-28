# Makefile for embedded code blobs for Rockchip SoCs
#
# These code blobs are emedded into vmlinux and copied into SRAM
# at times when SDRAM is not available.  Each blob is self contained.
#
# Some blobs may be linked to expect to run at a very specific address.
# A good example is resume code blobs that always expect to run in a
# very specific bit of SRAM that keeps power during sleep.  This code
# is also running with the cache off so it can predict the address it
# will be at.
#
# Other blobs may be linked with -fpic (by adding CFLAGS_file.o := -fpic).
# These can be located anywhere.  I believe gcc will support this by
# assuming that the .text and .data sections are relative to each other.
#
# That brings up the point that all blobs here:
# - Are generally very small
# - Generally have code and data jammed together in one blob.
# - Generally have "parameters" at the beginning that are filled in by
#   the kernel.

obj-$(CONFIG_PM_SLEEP) += rk3288_resume.bin.o

targets := rk3288_resume.o rk3288_ddr_resume.o \
	rk3288_resume.elf rk3288_resume.lds \
	rk3288_resume.bin rk3288_resume.bin.o

obj-y += rk3288_sram.bin.o

targets += rk3288-dmc-sram.o \
	rk3288_sram.elf rk3288_sram.lds \
	rk3288_sram.bin rk3288_sram.bin.o

# Reset objcopy flags, ARM puts "-O binary" here.
OBJCOPYFLAGS :=

# Prevents link failures: __sanitizer_cov_trace_pc() is not linked in.
KCOV_INSTRUMENT := n

# Our embedded code can't handle this flag.
CFLAGS_REMOVE_rk3288_resume.o = -pg
CFLAGS_REMOVE_rk3288_ddr_resume.o = -pg
CFLAGS_REMOVE_rk3288-dmc-sram.o = -pg

CFLAGS_rk3288_resume.o = $(call cc-disable-warning, inline-asm)

KBUILD_CFLAGS += -fno-stack-protector -fPIC

# This is the ELF for the embedded binary
LDFLAGS_rk3288_resume.elf := -Bstatic -nostdlib -T
$(obj)/rk3288_resume.elf: $(obj)/rk3288_resume.lds \
			  $(obj)/rk3288_resume.o \
			  $(obj)/rk3288_ddr_resume.o \
			  FORCE
	$(call if_changed,ld)

LDFLAGS_rk3288_sram.elf := -Bstatic -nostdlib --check-sections -T
$(obj)/rk3288_sram.elf: $(obj)/rk3288_sram.lds $(obj)/rk3288-dmc-sram.o \
			  FORCE
	$(call if_changed,ld)

# Create binary data for the kernel
OBJCOPYFLAGS_rk3288_resume.bin := -O binary
$(obj)/rk3288_resume.bin: $(obj)/rk3288_resume.elf FORCE
	$(call if_changed,objcopy)

OBJCOPYFLAGS_rk3288_sram.bin := -O binary
$(obj)/rk3288_sram.bin: $(obj)/rk3288_sram.elf FORCE
	$(call if_changed,objcopy)

# Import the data into the kernel
OBJCOPYFLAGS_rk3288_resume.bin.o += -B $(ARCH) -I binary
OBJCOPYFLAGS_rk3288_sram.bin.o += -B $(ARCH) -I binary

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
# Compile test only.
# Big endian support is untested and is not expected to work.
OBJCOPYFLAGS_rk3288_resume.bin.o += -O elf32-bigarm
OBJCOPYFLAGS_rk3288_sram.bin.o += -O elf32-bigarm
else
OBJCOPYFLAGS_rk3288_resume.bin.o += -O elf32-littlearm
OBJCOPYFLAGS_rk3288_sram.bin.o += -O elf32-littlearm
endif

$(obj)/rk3288_resume.bin.o: $(obj)/rk3288_resume.bin FORCE
	$(call if_changed,objcopy)

$(obj)/rk3288_sram.bin.o: $(obj)/rk3288_sram.bin FORCE
	$(call if_changed,objcopy)
