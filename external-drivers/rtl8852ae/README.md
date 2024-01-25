rtw89
===========
### A repo for the newest Realtek rtw89 codes.

This repo is current with wireless-next up to Nov. 2, 2023.

This branch was created from the version merged into the wireless-drivers-next
repo, which is in the 5.16 kernel. IF YOU USE DRIVERS FROM THIS REPO FOR KERNELS
5.16+, YOU MUST BLACKLIST THE KERNEL VERSIONS!!!! FAILING TO DO THIS WILL RESULT
IN ALL MANNER OF STRANGE ERRORS.

This code will build on any kernel 5.6 and newer as long as the distro has not modified
any of the kernel APIs. IF YOU RUN UBUNTU, YOU CAN BE ASSURED THAT THE APIs HAVE CHANGED.
NO, I WILL NOT MODIFY THE SOURCE FOR YOU. YOU ARE ON YOUR OWN!!!!!

Note that if you use this driver on kernels older than 5.15, the enhanced fetures
of  wifi 5 and wifi 6 are greatly crippled as the kernel does hot have the capability
to support the new packet widths and speeds. If you use such a kernel, you might
as well have an 802.11n (wifi 4) device.

This repository includes drivers for the following card:

Realtek 8852AE, 8852BE, and 8852CE

If you are looking for a driver for chips such as
RTL8188EE, RTL8192CE, RTL8192CU, RTL8192DE, RTL8192EE, RTL8192SE, RTL8723AE, RTL8723BE, or RTL8821AE,
these should be provided by your kernel. If not, then you should go to the Backports Project
(https://backports.wiki.kernel.org/index.php/Main_Page) to obtain the necessary code.

### Installation instruction
##### Requirements
You will need to install "make", "gcc", "kernel headers", "kernel build essentials", and "git".

For **Ubuntu**: You can install them with the following command
```bash
sudo apt-get update
sudo apt-get install make gcc linux-headers-$(uname -r) build-essential git
```
Users of Debian, Ubuntu, and similar (Mint etc) may want to scroll down and follow the DKMS instructions at the end of this document instead.

For **Fedora**: You can install them with the following command
```bash
sudo dnf install kernel-headers kernel-devel
sudo dnf group install "C Development Tools and Libraries"
```
For **openSUSE**: Install necessary headers with
```bash
sudo zypper install make gcc kernel-devel kernel-default-devel git libopenssl-devel
```
For **Arch**: After installing the necessary kernel headers and base-devel,
```bash
git clone https://aur.archlinux.org/rtw89-dkms-git.git
cd rtw89-dkms-git
makepkg -sri
```
If any of the packages above are not found check if your distro installs them like that.

##### Installation
For all distros:
```bash
git clone https://github.com/lwfinger/rtw89.git
cd rtw89
make
sudo make install
```

##### Installation with module signing for SecureBoot
For all distros:
```bash
git clone https://github.com/lwfinger/rtw89.git
cd rtw89
make
sudo make sign-install
```
You will be promted a password, please keep it in mind and use it in next steps.
Reboot to activate the new installed module.
In the MOK managerment screen:
1. Select "Enroll key" and enroll the key created by above sign-install step
2. When promted, enter the password you entered when create sign key. 
3. If you enter wrong password, your computer won't not bebootable. In this case,
   use the BOOT menu from your BIOS, to boot into your OS then do below steps:
```bash
sudo mokutil --reset
```
Restart your computer
Use BOOT menu from BIOS to boot into your OS
In the MOK managerment screen, select reset MOK list
Reboot then retry from the step make sign-install

##### How to unload/reload a Kernel module
 ```bash
sudo modprobe -rv rtw_8852ae
sudo modprobe -rv rtw89core	     #These two statements unload the module

Due to the behavior of the modprobe utility, it takes both to unload.

sudo modprobe -v rtw_8852ae          #This loads the module

A single modprobe call will reload the module.
```

##### Uninstall drivers
For all distros:
 ```bash
sudo make uninstall
```

##### Problem with recovery after sleep or hibernation
Some BIOSs have trouble changing power state from D3hot to D0. If you have this problem, then

sudo cp suspend_rtw89 /usr/lib/systemd/system-sleep/.

That script will unload the driver before sleep or hibernation, and reload it following resumption.

##### Option configuration
IMPORTANT: If you have an HP or Lenovo laptop, Their BIOS does not handle the PCIe interface correctly.
To compensate, run the following command:
sudo cp 70-rtw89.conf /etc/modprobe.d/.
Then unload the drivers and reload. You should see the options appended to the end of the rtw89_pci
or rtw89pci load line.

If it turns out that your system needs one of the other configuration options, then do the following:
```bash
sudo nano /etc/modprobe.d/<dev_name>.conf
```
There, enter the line below:
```bash
options <driver_name> <<driver_option_name>>=<value>
```
The available options for rtw89pci are disable_clkreq, disable_aspm_l1, and disable_aspm_l1ss.
The available options for rtw89core are debug_mask, and disable_ps_mode

Normally, none of these will be needed; however, if you are getting firmware errors, one or both
of the disable_aspm_* options may help. Thay are needed when a buggy BIOS fails to implement the
PCI specs correctly.

***********************************************************************************************

When your kernel changes, then you need to do the following:
```bash
cd ~/rtw89
git pull
make clean
make
sudo make install
;or
sudo make sign-install
```

Remember, this MUST be done whenever you get a new kernel - no exceptions.

These drivers will not build for kernels older than 5.6. If you must use an older kernel,
submit a GitHub issue with a listing of the build errors, but be aware that doing so will
cripple your device. Without the errors, the issue will be ignored. I am not a mind reader.

When you have problems where the driver builds and loads correctly, but fails to work, a GitHub
issue is NOT the best place to report it. I have no idea of the internal workings of any of the
chips, and the Realtek engineers who do will not read these issues. To reach them, send E-mail to
linux-wireless@vger.kernel.org. Include a detailed description of any messages in the kernel
logs and any steps that you have taken to analyze or fix the problem. If your description is
not complete, you are unlikely to get any satisfaction. One other thing - your mail MUST be plain test.
HTML mail is rejected.

# DKMS packaging for debian and derivatives

DKMS is commonly used on debian and derivatives, like ubuntu, to streamline building extra kernel modules.  
By following the instructions below and installing the resulting package, the rtw89 driver will automatically rebuild on kernel updates. Secure boot signing will happen automatically as well, 
as long as the dkms signing key (usually located at /var/lib/dkms/mok.key) is enrolled. See your distro's secure boot documentation for more details. 

Prerequisites:

``` bash
sudo apt install dh-sequence-dkms debhelper build-essential devscripts
```

This workflow uses devscripts, which has quite a few perl dependencies.  
You may wish to build inside a chroot to avoid unnecessary clutter on your system. The debian wiki page for [chroot](https://wiki.debian.org/chroot) has simple instructions for debian, which you can adapt to other distros as needed by changing the release codename and mirror url.  
If you do, make sure to install the package on your host system, as it will fail if you try to install inside the chroot. 

Build and installation

```bash
# If you've already built as above clean up your workspace or check one out specially (otherwise some temp files can end up in your package)
git clean -xfd

git deborig HEAD
dpkg-buildpackage -us -uc
sudo apt install ../rtw89-dkms_1.0.2-3_all.deb 
```

This will install the package, and build the module for your
currently active kernel.  You should then be able to `modprobe` as
above. It will also load automatically on boot.

##### A note regarding firmware

Firmware from userspace is required to use this driver. This package will attempt to pull the firmware in automatically as a Recommends.
However, if your distro does not provide one of firmware-realtek >= 20230117-1 or linux-firmware >= 20220329.git681281e4-0ubuntu3.10, 
the driver will fail to load, and dmesg will show an error about a specific missing firmware file. In this case, you can download the firmware files 
directly from https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/rtw89.

