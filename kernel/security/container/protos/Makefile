subdir-$(CONFIG_SECURITY_CONTAINER_MONITOR) += nanopb

obj-$(CONFIG_SECURITY_CONTAINER_MONITOR) += nanopb/
obj-$(CONFIG_SECURITY_CONTAINER_MONITOR) += protos.o

protos-y := config.pb.o event.pb.o

ccflags-y := -I$(srctree)/security/container/protos \
	-I$(srctree)/security/container/protos/nanopb \
	-DPB_SYSTEM_HEADER=$(PB_SYSTEM_HEADER)
