obj-m += apple-ibridge.o
obj-m += apple-ib-tb.o
obj-m += apple-ib-als.o

KVERSION := $(KERNELRELEASE)
ifeq ($(origin KERNELRELEASE), undefined)
KVERSION := $(shell uname -r)
endif
KDIR := ../../../kernel
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) O=out M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

