# Intel Precise Touch & Stylus

This is the kernelspace part of IPTS (Intel Precise Touch & Stylus) for Linux.

With IPTS the Intel Management Engine acts as an interface for a touch
controller, returning raw capacitive touch data. This data is processed
outside of the ME and then relayed into the HID / input subsystem of the OS.

This driver relies on a userspace daemon that can be found here:
https://github.com/linux-surface/iptsd

The driver will establish and manage the connection to the IPTS hardware. It
will also set up an API that can be used by userspace to read the touch data
from IPTS.

The daemon will connect to the IPTS UAPI and start reading data. It will
parse the data, and generate input events using uinput devices. The reason for
doing this in userspace is that parsing the data requires floating points,
which are not allowed in the kernel.

### Original IPTS driver
The original driver for IPTS was released by Intel in late 2016
(https://github.com/ipts-linux-org/ipts-linux-new). It uses GuC submission
to process raw touch input data on the GPU, using OpenCL firmware extracted
from Windows.

With linux 5.3 the ability to use GuC submission was removed from the i915
graphics driver, rendering the old driver unuseable. This lead to this
new driver being written, without using GuC submission. It is loosely based
on the original driver, but has undergone significant refactoring and cleanup.

An updated version of the driver can be found here for reference purposes:
https://github.com/linux-surface/kernel/tree/v4.19-surface-devel/drivers/misc/ipts

### Building (in-tree)
* Apply the patches from `patches/` to your kernel
* Add the files in this repository to `drivers/misc/ipts`
* Add `source "drivers/misc/ipts/Kconfig"` to
  `drivers/misc/Kconfig`
* Add `obj-y += ipts/` to `drivers/misc/Makefile`
* Enable the driver by setting `CONFIG_MISC_IPTS=m` in the kernel config

### Building (out-of-tree)
* Clone this repository and `cd` into it
* Make sure you applied the patches from `patches/` to your kernel
* Run `make all`
* Run `sudo insmod ipts.ko`

### Building (DKMS)
* Clone this repository and `cd` into it
* Make sure you applied the patches from `patches/` to your kernel
* Run `sudo make dkms-install`
* Reboot
