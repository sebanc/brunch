obj-$(CPTCFG_IWLXVT)   += iwlxvt.o
iwlxvt-y += xvt.o user-infc.o utils.o fw.o nvm.o rx.o
iwlxvt-$(CPTCFG_IWLWIFI_DEBUGFS) += debugfs.o

ccflags-y += -D__CHECK_ENDIAN__ -I$(srctree)/$(src)/../
