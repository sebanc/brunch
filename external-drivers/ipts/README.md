# Intel Precise Touch & Stylus

This is the Linux driver for IPTS (Intel Precise Touch and Stylus).

With IPTS the Intel Management Engine acts as an interface for a touch controller.
This module provides a HID transport driver that forwards the data into the userspace.

IPTS devices can return raw capacitive heatmap data instead of HID reports. This data is
wrapped in a HID report and forwarded to userspace too, where it needs to be processed by
a separate program and then passed back to the kernel using uinput.

The userspace daemon for Microsoft Surface devices can be found here:
https://github.com/linux-surface/iptsd

### Building (in-tree)
* Apply the patches from `patches/` to your kernel
* Copy the folder `src` to `drivers/hid/ipts`
* Add `source "drivers/hid/ipts/Kconfig"` to `drivers/hid/Kconfig`
* Add `obj-y += ipts/` to `drivers/hid/Makefile`
* Enable the driver by setting `CONFIG_HID_IPTS=m` in the kernel config

### Building (out-of-tree)
* Make sure you applied the patches from `patches/` to your kernel (if you are not using linux-surface builds)
* Run `make`
* Run `sudo insmod src/ipts.ko`

### Building (DKMS)
* Make sure you applied the patches from `patches/` to your kernel (if you are not using linux-surface builds)
* Run `sudo make dkms-install`
* Reboot

### Original IPTS driver
The original driver for IPTS was released by Intel in late 2016 (https://github.com/ipts-linux-org/ipts-linux-new).
It uses GuC submission to process raw touch input data on the GPU, using firmware blobs extracted from Windows.

With linux 5.3 the ability to use GuC submission was removed from the i915 graphics driver, rendering the old driver
unusable. This lead to this new driver being written, without using GuC submission. It is loosely based on the original
driver, but has undergone significant refactoring and cleanup.

An updated version of the driver can be found here for reference purposes:
https://github.com/linux-surface/kernel/tree/v4.19-surface-devel/drivers/misc/ipts
