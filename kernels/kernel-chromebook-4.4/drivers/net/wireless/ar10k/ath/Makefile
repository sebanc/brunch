
obj-$(CPTCFG_ATH_COMMON)	+= ar10k_ath.o

ath-objs :=	main.o \
		regd.o \
		hw.o \
		key.o \
		dfs_pattern_detector.o \
		dfs_pri_detector.o

ath-$(CPTCFG_ATH_DEBUG) += debug.o
ath-$(CPTCFG_ATH_TRACEPOINTS) += trace.o

ccflags-y += -D__CHECK_ENDIAN__

CFLAGS_trace.o := -I$(src)
obj-$(CPTCFG_ATH10K)           += ath10k/
ar10k_ath-objs := $(ath-objs) $(ath-y)
