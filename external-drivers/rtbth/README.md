This is a Linux kernel module for a Ralink RT3290 wireless device.
It enables [Bluez](http://www.bluez.org) software and driver support for RT3290.

This module has no official support by Mediatek. Support was discontinued.


### Usage: ###

```sh
# Init
sudo modprobe rtbth
sudo rfkill unblock bluetooth
hcitool dev # check

# Switch off
sudo rfkill block bluetooth

# Switch on
sudo rfkill unblock bluetooth

# Shutdown
sudo pkill -2 rtbt
sudo rmmod rtbth
```


### Installation: ###

[Ubuntu and derivatives](https://launchpad.net/~blaze/+archive/ubuntu/rtbth-dkms)
