modname := apple-bce
obj-m += $(modname).o

apple-bce-objs := apple_bce.o mailbox.o queue.o queue_dma.o vhci/vhci.o vhci/queue.o vhci/transfer.o audio/audio.o audio/protocol.o audio/protocol_bce.o audio/pcm.o

ccflags-y += -DWITHOUT_NVME_PATCH
