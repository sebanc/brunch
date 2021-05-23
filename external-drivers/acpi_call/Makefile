obj-m := acpi_call.o

KVERSION := $(shell uname -r)
KDIR := ../kernel
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) O=out M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

load:
	-/sbin/rmmod acpi_call
	/sbin/insmod acpi_call.ko
