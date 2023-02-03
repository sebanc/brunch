rtw89
===========
### A repo for the newest Realtek rtlwifi codes.

This branch was created from the version merged into the wireless-drivers-next
repo, which is in the 5.16 kernel. IF YOU USE DRIVERS FROM THIS REPO FOR KERNELS
5.16+, YOU MUST BLACKLIST THE KERNEL VERSIONS!!!! FAILING TO DO THIS WILL RESULT
IN ALL MANNER OF STRANGE ERRORS.

This code will build on any kernel 5.7 and newer as long as the distro has not modified
any of the kernel APIs. IF YOU RUN UBUNTU, YOU CAN BE ASSURED THAT THE APIs HAVE CHANGED.
NO, I WILL NOT MODIFY THE SOURCE FOR YOU. YOU ARE ON YOUR OWN!!!!!

I am working on fixing builds on older kernels.

This repository includes drivers for the following card:

Realtek 8852AE, 8852BE, and 8853CE

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
sudo modprobe -rv rtw_core	     #These two statements unload the module

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
If it turns out that your system needs one of the configuration options, then do the following:
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

These drivers will not build for kernels older than 5.4. If you must use an older kernel,
submit a GitHub issue with a listing of the build errors. Without the errors, the issue
will be ignored. I am not a mind reader.

When you have problems where the driver builds and loads correctly, but fails to work, a GitHub
issue is NOT the best place to report it. I have no idea of the internal workings of any of the
chips, and the Realtek engineers who do will not read these issues. To reach them, send E-mail to
linux-wireless@vger.kernel.org. Include a detailed description of any messages in the kernel
logs and any steps that you have taken to analyze or fix the problem. If your description is
not complete, you are unlikely to get any satisfaction. One other thing - your mail MUST be plain test.
HTML mail is rejected.

# DKMS packaging for ubuntu/debian

DKMS on debian/ubuntu simplifies the secure-boot issues, signing is
taken care of through the same mechanisms as nVidia and drivers.  You
won't need special reboot and MOK registration.

Additionally DKMS ensures new kernel installations will automatically
rebuild the driver, so you can accept normal kernel updates.

Prerequisites:

A few packages are required to build the debs from source:

``` bash
sudo apt install dkms debhelper dh-modaliases
```

Build and installation

```bash
# If you've already built as above clean up your workspace or check one out specially (otherwise some temp files can end up in your package)
git clean -xfd

dpkg-buildpackage -us -uc
sudo apt install ../rtw89-dkms_1.0.2-2_all.deb ../rtw89-firmware_1.0.2-2_all.deb
```

That should install the package, and build the module for your
currently active kernel.  You should then be able to `modprobe` as
above.

```bash
Missing firmware for RTW8852BE

Thae necessary firmwar3e file should be in package firmware-realtek of linux-firmware-realtek;
however some versions of some distros have been extremely slow to pick up this firmware file,
even though it has been in the official linux-firmware repo at vger.kernel.org since
Oct. 27, 2022. If your distro is one of these, you can download the firmware from
https://lwfinger.com/download/rtw8852b_fw.bin, and copy it to /lib/firmware/rtw89/.
```
