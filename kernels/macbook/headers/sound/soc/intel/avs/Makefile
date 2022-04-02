# SPDX-License-Identifier: GPL-2.0-only

snd-soc-avs-objs := dsp.o ipc.o messages.o utils.o topology.o path.o \
		    core.o loader.o pcm.o board_selection.o \
		    kcontrol.o
snd-soc-avs-objs += cldma.o
snd-soc-avs-objs += skl.o apl.o

snd-soc-avs-objs += trace.o
# tell define_trace.h where to find the trace header
CFLAGS_trace.o := -I$(src)

obj-$(CONFIG_SND_SOC_INTEL_AVS) += snd-soc-avs.o

# Machine support
obj-$(CONFIG_SND_SOC) += boards/
