Linux driver for Intel Touch Host Controller
============================================

NOTE: This driver is included in the [Linux Surface](https://github.com/linux-surface/linux-surface) kernel.

The ithc kernel module provides support for the Intel Touch Host Controller,
which is used for the touchscreens in some newer Intel-based devices, such
as the Surface Pro 7+, Surface Pro 8, X1 Fold, etc.

The module works as an HID transport driver. For Surface devices, you will
also need to install [IPTSD](https://github.com/linux-surface/iptsd) to
enable multi-touch and pen support. Without IPTSD, only single touch will
work. Non-Microsoft devices use standard HID data, and don't need IPTSD.

Installation with DKMS
----------------------

- Install prerequisites (e.g. Debian packages: `dkms` and `linux-headers-amd64`)
- `sudo make dkms-install`
- `sudo modprobe ithc` (or reboot)
- Check dmesg for ithc messages/errors.

Installation without DKMS
-------------------------

- Install prerequisites (e.g. Debian packages: `build-essential` and `linux-headers-amd64`)
- `make && sudo make install`
- `sudo modprobe ithc` (or reboot)
- Check dmesg for ithc messages/errors.

Known issues
------------

On Lakefield and Tiger Lake devices (SP7+/SP8/SLS/SL4/X1Fold) the driver
may fail to start correctly, and you will see the following error in dmesg:
"Blocked an interrupt request due to source-id verification failure".

To fix this, apply one of the following workarounds:
1. Add the kernel parameter `intremap=nosid` and reboot.
   If you're using GRUB on Debian/Ubuntu, you can do this with
   `sudo make set-nosid`.
2. Apply `intel-iommu.patch` to your kernel.
3. Use the driver in polling mode by setting the `poll` module parameter.
   Run `echo options ithc poll | sudo tee /etc/modprobe.d/ithc-poll.conf`
   and reload the module or reboot.

License
-------

Public domain/CC0.
(Files are marked GPL/BSD since that is preferred for the kernel.)

