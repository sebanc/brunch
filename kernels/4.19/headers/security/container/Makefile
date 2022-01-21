PB_SYSTEM_HEADER := "<pbsystem.h>"
export PB_SYSTEM_HEADER

subdir-$(CONFIG_SECURITY_CONTAINER_MONITOR) += protos

obj-$(CONFIG_SECURITY_CONTAINER_MONITOR) += protos/
obj-$(CONFIG_SECURITY_CONTAINER_MONITOR) += monitor.o pb.o process.o vsock.o

ccflags-y := -I$(srctree)/security/container/protos \
	-I$(srctree)/security/container/protos/nanopb \
	-DPB_SYSTEM_HEADER=$(PB_SYSTEM_HEADER) \
	-I$(srctree)/fs
ccflags-$(CONFIG_SECURITY_CONTAINER_MONITOR_DEBUG) += -DDEBUG
