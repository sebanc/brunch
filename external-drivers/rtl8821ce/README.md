# Realtek RTL8821CE Driver

## Intent
This repository hosts the code for the [Arch Linux AUR Package](https://aur.archlinux.org/packages/rtl8821ce-dkms-git/). It's targeting Linux > 4.14 and is being developed for Arch Linux and Ubuntu 18.10. No support will be provided for other Linux distributions or Linux Kernel versions outside of that range.

## Disclaimer
The maintainers of this repository are not Realtek employees and are maintaining this repository for their own usage. Further feature development (such as proper power saving, etc.) will not be pursued here, but will be gladly integrated if newer driver sources are provided by Realtek. Use at your own risk.

## DKMS
This driver can be installed using [DKMS](http://linux.dell.com/dkms/). This is a system which will automatically recompile and install a kernel module when a new kernel gets installed or updated. To make use of DKMS, install the `dkms` package.


## Installation of Driver
Make sure you have a proper build environment and `dkms` installed.

### Ubuntu & Debian
The following steps are required prior to building the driver on Ubuntu/Debian:
```
sudo apt install bc module-assistant build-essential dkms
sudo m-a prepare
```
Ubuntu users may also install the prebuilt [rtl8821ce-dkms](https://packages.ubuntu.com/bionic-updates/rtl8821ce-dkms) package, an older version of the driver maintained by the Ubuntu MOTU Developers group for bionic, eoan and focal. It has been known to work in cases where the newer driver available here does not. Bugs and issues with that package should be reported at [Launchpad](https://launchpad.net/ubuntu/+source/rtl8821ce/+bugs) rather than here.

### Arch Linux
Make sure you have the `base-devel` package group installed before you proceed for the necessary compilation tools.

#### Installing from AUR

Install [rtl8821ce-dkms-git](https://aur.archlinux.org/packages/rtl8821ce-dkms-git/) from the [AUR](https://wiki.archlinux.org/index.php/Arch_User_Repository).

#### Dependencies for manual installation on Arch Linux
```
sudo pacman -Syu linux-headers dkms bc
```
If you are running a non-vanilla kernel then install the headers to match the kernel package. Proceed to the section below.

### Gentoo Linux
An unofficial Gentoo package is available, using this repository as upstream. It is available from the [trolltoo](https://github.com/dallenwilson/trolltoo) overlay. Gentoo does not use or require dkms for packaged drivers.
```
# layman -a trolltoo
# emerge --ask net-wireless/rtl8821ce-driver
```

### Manual installation of driver
In order to install the driver open a terminal in the directory with the source code and execute the following command:
```
sudo ./dkms-install.sh
```

## Removal of Driver
Open a terminal window and git clone the repository to your local disk

```
git clone https://github.com/tomaspinho/rtl8821ce.git
cd rtl8821ce
```

Then run the removal script:
```
sudo ./dkms-remove.sh
```

## Upgrading driver
Remove the driver:
```
sudo ./dkms-remove.sh
```

Make sure you have your local copy of this repository fully updated:
```
git pull
```

Clean any stale binaries:
```
make clean
```

Install again:
```
sudo ./dkms-install.sh
```

## Reporting issues
When reporting issues, please make sure that debugging is enabled. To enable debugging either set `MAKEFLAGS="CONFIG_RTW_DEBUG = y"` before compilation or edit Makefile:
```
CONFIG_RTW_DEBUG = y
```
This will enable verbose debug logging, helpful to developers.

## Possible issues

### PCIe Active State Power Management
Your distribution may come with PCIe Active State Power Management enabled by default. That may conflict with this driver. To disable:

```
sudo $EDITOR /etc/default/grub
```
Add pcie_aspm=off at the end of GRUB_CMDLINE_LINUX_DEFAULT. Line should look like this:

```
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash pcie_aspm=off"
```

Then update your GRUB configuration:
```
sudo update-grub
```

On systems that doesn't have `update-grub` but have `grubby` like Fedora, you can directly execute instead:
```
sudo grubby --update-kernel=ALL --args=pcie_aspm=off
```
Reboot.

### Lenovo Yoga laptops

Some new Yoga laptops (like the Yoga 530) come with `rtl8821ce` as the Wi-Fi/Bluetooth chip. But the `ideapad-laptop` module, which may come included in your distribution, may conflict with this driver. To disable:

```
sudo modprobe -r ideapad_laptop
```

### BlueTooth is not working

This may be due to the Kernel loading up the wrong firmware file for this card. Please take a look at [@wahsot](https://github.com/wahsot)'s tutorial at https://github.com/tomaspinho/rtl8821ce/issues/19#issuecomment-452196840 to see if that helps you out.

### Secure Boot

If your system uses Secure Boot, the kernel will not accept user-supplied modules. There are two ways to solve this issue:
1. Disable Secure Boot via BIOS/UEFI settings.
2. Create or use an existing MOK (Machine Owner Key) to sign the compiled `.ko` linux kernel object produced by DKMS.

### Unstable connection - slowdowns or dropouts

The problem may be due to the periodic scanning of access points by the network applet.

> This fix worked helpful on Pop! _OS/Ubuntu 20.10 and Fedora 33. Both with GNOME and NetworkManager. [#179](https://github.com/tomaspinho/rtl8821ce/issues/179)

Set the BSSID from your network applet. In GNOME this can be done in `WiFi Settings > Your profile > Identity > BSSID`.

We are going to disable the *Connectivity Check* option in NetworkManager. This by editing the file in `/var/lib/NetworkManager/NetworkManager-intern.conf` and adding the following instructions at the end:

```
[connectivity]
.set.enabled=false
```

Then, just reboot or restart the NetworkManager unit to fix the problem.

### Wi-Fi not working for kernel >= 5.9
The Linux Kernel 5.9 version comes with a broken `rtw88` module developed by Realtek that has poor compatibility with most revisions of the 8821ce chip.

You must disable it by adding the following to your module blacklists (`/etc/modprobe.d/blacklist.conf`):

```
blacklist rtw88_8821ce
``` 

Then, make sure you have the rtl8821ce module correctly installed. 

Turn off your computer, wait a few seconds (to force firmware reload) and then turn it on again.

### Wi-Fi and Bluetooth don't work after suspend

This is a bug that won't be fixed until/if Realtek implements proper power management themselves.
Given they are now only working in `rtw88`, this driver will most likely never be fixed in this regard.
Please avoid opening issues about this.

### Monitor (promiscuous) mode doesn't work
This driver doesn't support it and never will.
Please avoid opening issues about this.
