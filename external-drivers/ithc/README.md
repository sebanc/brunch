Linux driver for Intel Touch Host Controller
============================================

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

Notes
-----

If you get an error in dmesg saying "Blocked an interrupt request due to
source-id verification failure", you will need to add the kernel parameter
`intremap=nosid` and reboot. If you're using GRUB, you can do this with
`sudo make set-nosid`. Alternatively, use the driver in polling mode by
setting the `poll` module parameter (e.g. by running
`echo options ithc poll | sudo tee /etc/modprobe.d/ithc-poll.conf`
and reloading the module or rebooting).

The driver works in single-touch mode by default. Multitouch and pen data
is made available to userspace through /dev/ithc and the HID subsystem.
To enable multitouch functionality, you will need to install iptsd.

To enable debug logging use the module parameter `dyndbg=+pflmt`.


License: Public domain/CC0 (or GPL2 or BSD-2-Clause if you prefer).

