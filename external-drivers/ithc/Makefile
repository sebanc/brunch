KDIR ?= ../kernel
DEST ?= build

default:
	mkdir -p $(DEST)
	find $(DEST)/ -type l -exec rm {} +
	ln -sr src/* $(DEST)/
	$(MAKE) -C $(KDIR) O=out M=$(abspath $(DEST))

install:
	$(MAKE) -C $(KDIR) M=$(abspath $(DEST)) modules_install
	depmod -a
	sync

PKG_NAME := `sed -n '/^PACKAGE_NAME="\(.*\)"$$/s//\1/p' dkms.conf`
PKG_VER := `sed -n '/^PACKAGE_VERSION="\(.*\)"$$/s//\1/p' dkms.conf`
DKMS_DIR = /usr/src/$(PKG_NAME)-$(PKG_VER)
DKMS_PKG = $(PKG_NAME)/$(PKG_VER)

dkms-install:
	mkdir -p $(DKMS_DIR)
	cp -r dkms.conf Makefile src $(DKMS_DIR)
	dkms add $(DKMS_PKG)
	dkms build $(DKMS_PKG)
	dkms install $(DKMS_PKG)
	sync

dkms-uninstall:
	-modprobe -r ithc
	-dkms uninstall $(DKMS_PKG)
	-dkms remove $(DKMS_PKG)
	-rm -rf $(DKMS_DIR)
	sync

set-nosid:
	echo 'GRUB_CMDLINE_LINUX_DEFAULT="$$GRUB_CMDLINE_LINUX_DEFAULT intremap=nosid"' > /etc/default/grub.d/intremap-nosid.cfg
	update-grub
	sync

clear-nosid:
	rm /etc/default/grub.d/intremap-nosid.cfg
	update-grub
	sync

clean:
	-rm -r $(DEST)

run:
	$(MAKE)
	-sudo modprobe -r ithc
	sudo $(MAKE) install
	sudo modprobe ithc

$(DEST)/%: src/%.c
	mkdir -p $(DEST)
	$(CC) $(CFLAGS) -W -Wall -Wextra -Werror -o $@ $<

dump: $(DEST)/ithc-dump

