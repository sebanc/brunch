obj-$(CONFIG_SECURITY_CONTAINER_MONITOR) += nanopb.o

nanopb-y := pb_encode.o pb_decode.o pb_common.o

ccflags-y := -I$(srctree)/security/container/protos \
	-I$(srctree)/security/container/protos/nanopb \
	-DPB_SYSTEM_HEADER=$(PB_SYSTEM_HEADER)
