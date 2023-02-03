## 8821cu ( 8821cu.ko ) :rocket:

Notice: Myself and others have been working to bring a new
version of this driver online. It is based on new source code from
Realtek and it appears to fix a few things. We are also taking the
opportunity to greatly enhance the installation and removal scripts as
well as the README. This work will then be used on all Realtek drivers
located at this site so it is important that we test. Testing is
currently being done in a private repo with an expected release during
the first quarter of next year.

If you are interested in testing, please see issue #73.

## Linux Driver for USB WiFi Adapters that are based on the RTL8811CU, RTL8821CU and RTL8731AU Chipsets

- v5.12.0 (Realtek) (20210118) plus updates from the Linux community

Note: Please read "supported-device-IDs" for information about how to
confirm that this is the correct driver for your adapter.

### Features

- IEEE 802.11 b/g/n/ac WiFi compliant
- 802.1x, WEP, WPA TKIP and WPA2 AES/Mixed mode for PSK and TLS (Radius)
- WPA3 (see FAQ)
- IEEE 802.11b/g/n/ac Client mode
  * Supports wireless security for WEP, WPA TKIP and WPA2 AES PSK
  * Supports site survey scan and manual connect
  * Supports WPA/WPA2 TLS client
- Power saving modes
- Wireshark compatible
- Aircrack-ng compatible
- Packet injection
- hostapd compatible
- AP mode DFS channel support
- Miracast
- Supported interface modes
  * Managed
  * Monitor (see FAQ)
  * AP
  * P2P-client
  * P2P-GO
- Log level control
- LED control
- Power saving control
- VHT control (allows 80 MHz channel width in AP mode)
- AP mode DFS channel control

### A FAQ is available at the end of this document.

### Additional documentation is the file `8821cu.conf`.

### Compatible CPU Architectures

- x86, i686
- x86-64, amd64
- armv7l, armv6l (arm)
- aarch64 (arm64)

### Compatible Kernels

- Kernels: 4.19 - 5.11 (Realtek)
- Kernels: 5.12 - 6.2  (community support)

### Tested Compilers

- gcc 9, 10, 11 and 12

### Tested Linux Distributions

Note: The information in this section depends largely on user reports which can
be provided via PR or message in Issues.

- Arch Linux (kernels 5.4 and 5.11)

- Armbian_22.11.1 (kernel 5.15) (Rock 4 SE (Rock 4b image with xfce))

- Debian 11 (kernels 5.10 and 5.15)

- Fedora (kernel 5.11)

- Kali Linux (kernel 5.10)

- Manjaro 21.1 (kernel 5.13)

- openSUSE Tumbleweed (rolling) (kernel 5.15)

- Raspberry Pi OS (2022-09-22) (ARM 32 bit and 64 bit) (kernel 5.15)

- Raspberry Pi Desktop (2022-07-01) (x86 32 bit) (kernel 5.10)

- SkiffOS for Odroid XU4 (ARM 32 bit) (kernel 6.0.7)

- Ubuntu 22.04 (kernel 5.15) and 22.10 (kernel 5.19) (kernel 6.2)

- Void Linux (kernel 5.18)

Note: Red Hat Enterprise Linux (RHEL) and distros based on RHEL are not
supported due to the way kernel patches are handled. I will support
knowledgable RHEL developers if they want to merge the required
support and keep it current.

Note: Android is supported in the driver according to Realtek. I will support
knowledgable Android developers if they want to merge and keep current the
required support (most likely just instructions about how to compile and maybe
a modification or two to the Makefile).


### Download Locations for Tested Linux Distributions

- [Arch Linux](https://www.archlinux.org)
- [Armbian](https://www.armbian.com/)
- [Debian](https://www.debian.org/)
- [Fedora](https://getfedora.org)
- [Kali Linux](https://www.kali.org/)
- [Manjaro](https://manjaro.org)
- [openSUSE](https://www.opensuse.org/)
- [Raspberry Pi OS](https://www.raspberrypi.org)
- [SkiffOS](https://github.com/skiffos/skiffos/)
- [Ubuntu](https://www.ubuntu.com)
- [Void Linux](https://voidlinux.org/)

### Tested Hardware

- EDUP EP-AC1651 USB WiFi Adapter AC650 Dual Band USB 2.0 (nano)
- EDUP EP-AC1635 USB WiFi Adapter AC600 Dual Band USB 2.0

### Compatible Devices

* EDUP EP-AC1651 (nano) (single-state, single-function)
* EDUP EP-AC1635 (single-state, single-function)
* Numerous adapters that are based on the supported chipset.

Note: If you are looking for information about what adapter to buy,
click [here](https://github.com/morrownr/USB-WiFi) and look for Main Menu
item 2 which will show information about and links to recommended adapters.

Note: If you decide to buy an adapter that is supported by this driver, I
recommend you search for an adapter that is `single-state and single-function`.
Multi-function adapters, wifi and bluetooth, can be problematic. The rtl8821cu
chipset is multi-fuction. The rtl8811cu chipset is single-function. For advice
about single-state and multi-state adapters. click
[here](https://github.com/morrownr/USB-WiFi) and look for Main Menu item 1.

### Installation Information

Note: As of Linux kernel 6.2, an in-kernel driver for the chipsets supported by
this driver has been included in the Linux kernel. The installation and removal
scripts for the driver in this repo automatically deactivate the in-kernel
driver on installation and reactivate the in-kernel driver on removal. No
special action needs to be taken by users.

Warning: Installing multiple out-of-kernel drivers for the same hardware
usually does not end well. If a previous attempt to install this driver failed
or if you have previously installed another driver for chipsets supported by
this driver, you MUST remove anything that the previous attempt
installed BEFORE attempting to install this driver. This driver can be
removed with the script called `./remove-driver.sh`. Information is
available in the section called `Removal of the Driver.` You can get a
good idea as to whether you need to remove a previously installed
driver by running the following command:

```
sudo dkms status
```

Warning: If you decide to upgrade to a new version of kernel such as
5.15 to 5.19, you need to remove the driver you have installed and
install the newest available before installing the new kernel. Use the
following commands in the driver directory:

```
$ sudo ./remove-driver.sh
$ git pull
$ sudo ./install-driver.sh
```

Temporary internet access is required for installation. There are numerous ways
to enable temporary internet access depending on your hardware and situation.
[One method is to use tethering from a phone.](https://www.makeuseof.com/tag/how-to-tether-your-smartphone-in-linux).
Another method is to keep a [WiFi adapter that uses an in-kernel driver](https://github.com/morrownr/USB-WiFi) in your toolkit.

You will need to use the terminal interface. The quick way to open a terminal:
Ctrl+Alt+T (hold down on the Ctrl and Alt keys then press the T key).

An alternative terminal is to use SSH (Secure Shell) from the same or from
another computer, in which case you will be in a suitable terminal after logging
in, but this step requires that an SSH daemon/server has already been
configured. (There are lots of SSH guides available, e.g., for the [Raspberry Pi](https://www.raspberrypi.com/documentation/computers/remote-access.html#setting-up-an-ssh-server) and for [Ubuntu](https://linuxconfig.org/ubuntu-20-04-ssh-server). Do not forget [to secure the SSH server](https://www.howtogeek.com/443156/the-best-ways-to-secure-your-ssh-server/).)

You will need to have sufficient access rights to use `sudo` so that commands
can be executed as the `root` user. (If the command `sudo echo Yes` returns
"Yes", with or without having to enter your password, you do have sufficient
access rights.)

DKMS is used for the installation if available. DKMS is a system utility
which will automatically recompile and reinstall this driver when a new
kernel is installed. DKMS is provided by and maintained by Dell.

It is recommended that you do not delete the driver directory after
installation as the directory contains information and scripts that you
may need in the future.

Secure Boot: The installation script, `install-driver.sh`, will
automatically support secure boot... if your distro supports the method
dkms uses. I regularly test the installation script on systems with
secure boot on. It works seemlessly on modern Ubuntu based distros as
long as secure boot was set up properly during the installation of the
operating system. Some distros, such as the Raspberry Pi OS, do not
support secure boot because the hardware they support does not support
secure boot making it unnecessary to attempt to support it. There are
distros that may require additional steps to sign the driver for secure
boot operation. Fedora is an example. In installation Step 3, note that
`openssl` must be installed as Fedora does not install it by default.
There will also be another step for Fedora after `install-driver.sh`
script is completed. This will be explained in the instructions at the
appropriate time. Overall, secure boot requires that
`openssl` and `mokutil` be installed and that additional steps be
performed if necessary. To test if secure boot is the problem:  If you
install this driver and, after a reboot, the driver is not working, you
can go into the BIOS and temporarily turn secure boot off to see if
secure boot is the problem.

### Installation Steps

Note: The installation instructions are for the novice user. Experienced users are
welcome to alter the installation to meet their needs. Support will be provided,
on a best effort basis, based on the steps below.

#### Step 1: Open a terminal (e.g. Ctrl+Alt+T)

#### Step 2: Update and upgrade system packages (select the option for the distro you are using)

Note: If your Linux distro does not fall into one of options listed
below, you will need to research how to update and upgrade your system
packages.

- Option for Debian based distributions such as Ubuntu, Kali, Armbian and Raspberry Pi OS

```
sudo apt update && sudo apt upgrade
```

- Option for Arch based distributions such as Manjaro

```
sudo pacman -Syu
```

- Option for Fedora based distributions

```
sudo dnf upgrade
```

- Option for openSUSE based distributions

```
sudo zypper update
```

- Option for Void Linux

```
sudo xbps-install -Syu
```

Note: It is recommended that you reboot your system at this point. The
rest of the installation will appreciate having a fully up to date
system to work with. The installation can then be continued with Step 3.

```
sudo reboot
```

#### Step 3: Install the required packages (select the option for the OS you are using)

Note: If your Linux distro does not fall into one of options listed
below, you will need to research how to properly setup up the development
environment for your system.

- Option for Armbian (arm64)

```
sudo apt install -y build-essential
```

- Option for Raspberry Pi OS (arm/arm64)

```
sudo apt install -y raspberrypi-kernel-headers build-essential bc dkms git
```

- Option for Debian, Kali, and Raspberry Pi Desktop (x86)

```
sudo apt install -y linux-headers-$(uname -r) build-essential bc dkms git libelf-dev rfkill iw
```

- Option for Ubuntu (all official flavors) and the numerous Ubuntu based distros

```
sudo apt install -y build-essential dkms git iw
```

- Option for Fedora

Note: Installing `openssl` is only necessary for secure boot support.

```
sudo dnf -y install git dkms kernel-devel openssl
```

- Option for openSUSE

```
sudo zypper install -t pattern devel_kernel dkms
```

- Option for Alpine

```
sudo apk add linux-lts-dev make gcc
```

- Option for Void Linux

```
sudo xbps-install linux-headers dkms git make
```

- Options for Arch and Manjaro (if using Manjaro for RasPi4B, see note)

If using pacman

```
sudo pacman -S --noconfirm linux-headers dkms git bc
```

Note: The following is needed if using Manjaro for RasPi4B.

```
sudo pacman -S --noconfirm linux-rpi4-headers dkms git bc
```

Note: If you are asked to choose a provider, make sure to choose the one
that corresponds to your version of the linux kernel (for example,
"linux510-headers" for Linux kernel version 5.10). If you install the
incorrect version, you'll have to uninstall it and install the correct
version.

If using other methods, please follow the instructions provided by those
methods.

#### Step 4: Create a directory to hold the downloaded driver

```
mkdir -p ~/src
```

#### Step 5: Move to the newly created directory

```
cd ~/src
```

#### Step 6: Download the driver

```
git clone https://github.com/morrownr/8821cu-20210118.git
```

#### Step 7: Move to the newly created driver directory

```
cd ~/src/8821cu-20210118
```

#### Step 8: Run the installation script ( install-driver.sh )

Note: For automated builds (non-interactive), use _NoPrompt_ as an option.

```
sudo ./install-driver.sh
```

Note: If you elect to skip the reboot at the end of the installation
script, the driver may not load immediately and the driver options will
not be applied. Rebooting is strongly recommended.

Note: Fedora users that have secure boot turned on should run the following to
enroll the key:

$ sudo mokutil --import /var/lib/dkms/mok.pub

Manual build instructions: The above script automates the installation
process, however, if you want to or need to do a command line
installation, use the following:

```
make clean
make
sudo make install
sudo reboot
```

Note: If you use the manual build instructions, you will need to repeat
the process each time a new kernel is installed in your distro.

-----

### Driver Options ( edit-options.sh )

A file called `8821cu.conf` will be installed in `/etc/modprobe.d` by
default if you use the `./install-driver.sh` script.

Note: The installation script will prompt you to edit the options.

Location: `/etc/modprobe.d/8821cu.conf`

This file will be read and applied to the driver on each system boot.

To edit the driver options file, run the `edit-options.sh` script

```
sudo ./edit-options.sh
```

Note: Documentation for Driver Options is included in the file `8821cu.conf`.

-----

### Upgrading the Driver

Note: Linux development is continuous therefore work on this driver is continuous.

Note: Upgrading the driver is advised in the following situations:

- if a new or updated version of the driver needs to be installed
- if a distro version upgrade is going to be installed (i.e. going from kernel 5.10 to kernel 5.15)

#### Step 1: Move to the driver directory

```
cd ~/src/8821cu-20210916
```

#### Step 2: Remove the currently installed driver

```
sudo ./remove-driver.sh
```

#### Step 3: Pull updated code from this repo

```
git pull
```

#### Step 4: Install the driver

```
sudo ./install-driver.sh
```

-----
### Removal of the Driver ( remove-driver.sh  )

Note: Removing the driver is advised in the following situations:

- if driver installation fails
- if the driver is no longer needed

Note: The following removes everything that has been installed, with the
exception of the packages installed in Step 3 and the driver directory.
The driver directory can be deleted after running this script.

#### Step 1: Open a terminal (e.g. Ctrl+Alt+T)

#### Step 2: Move to the driver directory

```
cd ~/src/8821cu-20210118
```

#### Step 3: Run the removal script

Note: For automated builds (non-interactive), use _NoPrompt_ as an option.

```
sudo ./remove-driver.sh
```

-----

### Recommended WiFi Router/ Access Point Settings

Note: These are general recommendations, some of which may not apply to your specific situation.

- Security: Set WPA2-AES or WPA2/WPA3 mixed or WPA3. Do not set WPA2 mixed mode or WPA or TKIP.

- Channel width for 2.4 GHz: Set 20 MHz fixed width. Do not use 40 MHz or 20/40 automatic.

- Channels for 2.4 GHz: Set channel 1 or 6 or 11 depending on the congestion at your location. Do not set automatic channel selection. As time passes, if you notice poor performance, recheck congestion and set channel appropriately. The environment around you can and does change over time.

- Mode for 2.4 GHz: For best performance, set "N only" if you no longer use B or G capable devices.

- Network names: Do not set the 2.4 GHz Network and the 5 GHz Network to the same name. Note: Unfortunately many routers come with both networks set to the same name. You need to be able to control which network that is in use so changing the name of one of the networks is recommended. Since many IoT devices use the 2.4 GHz network, it may be better to change the name of the 5 GHz network.

- Channels for 5 GHz: Not all devices are capable of using DFS channels (I'm looking at you Roku.) It may be necessary to set a fixed channel in the range of 36 to 48 or 149 to 165 in order for all of your devices to work on 5 GHz. (For US, other countries may vary.)

- Best location for the WiFi router/access point: Near center of apartment or house, at least a couple of feet away from walls, in an elevated location. You may have to test to see what the best location is in your environment.

- Check congestion: There are apps available for smart phones that allow you to get an idea of the congestion levels on WiFi channels. The apps generally go by the name of ```WiFi Analyzer``` or something similar.

After making and saving changes, reboot the router.

-----

### Recommendations regarding USB

- Moving your USB WiFi adapter to a different USB port has been known to fix a variety of problems.

- If connecting your USB WiFi adapter to a desktop computer, use the USB ports on the rear of the computer. Why? The ports on the rear are directly connected to the motherboard which will reduce problems with interference and disconnection.

- If your USB WiFi adapter is USB 3 capable and you want it to operate in USB3 mode, plug it into a USB 3 port.

- Avoid USB 3.1 Gen 2 ports if possible as almost all currently available adapters have been tested with USB 3.1 Gen 1 (aka USB 3) and not with USB 3.1 Gen 2.

- If you use an extension cable and your adapter is USB 3 capable, the cable needs to be USB 3 capable (if not, you will be limited to USB 2 speeds).

- Extention cables can be problematic. A way to check if the extension cable is the problem is to plug the adapter temporarily into a USB port on the computer.

- Some USB WiFi adapters require considerable electrical current and push the capabilities of the power available via USB port. One example is adapters that use the Realtek 8814au chipset. Using a powered multiport USB extension can be a good idea in cases like this.

-----

### How to disable onboard WiFi on Raspberry Pi 3B, 3B+, 3A+, 4B and Zero W

Add the following line to /boot/config.txt

```
dtoverlay=disable-wifi
```

-----

### How to forget a saved WiFi network on a Raspberry Pi

#### Step 1: Edit wpa_supplicant.conf

```
sudo nano /etc/wpa_supplicant/wpa_supplicant.conf
```

#### Step 2: Delete the relevant WiFi network block (including the 'network=' and opening/closing braces.

#### Step 3: Press ctrl-x followed by 'y' and enter to save the file.

#### Step 4: Reboot

-----

### FAQ:

Question: Is WPA3 supported?

Answer: WPA3-SAE is supported. It works well on most modern Linux distros but
not all. Generally the reason for WPA3 not working on Linux distros is that the
distro has an old version of wpa_supplicant or Network Manager. Your options
are to upgrade to a more modern distro (distros released after mid 2022) or
compile and install new versions of wpa_supplicant and/or Network Manager.

-----

Question: I bought two usb wifi adapters based on this chipset and am planning
to use both in the same computer. How do I set that up?

Answer: Realtek drivers do not support more than one adapter with the
same chipset in the same computer. You can have multiple Realtek based
adapters in the same computer as long as the adapters are based on
different chipsets.

-----

Question: Why do you recommend Mediatek based adapters when you maintain
this repo for a Realtek driver?

Answer: Many new and existing Linux users already have adapters based on
Realtek chipsets. This repo is for Linux users to support their existing
adapters but my STRONG recommendation is for Linux users to seek out USB
WiFi solutions based on Mediatek chipsets:

https://github.com/morrownr/USB-WiFi

-----

Question: Will you put volunteers to work?

Answer: Yes. Post a message in `Issues` or `Discussions` if interested.

-----

Question: I am having problems with my adapter and I use Virtualbox?

Answer: This [article](https://null-byte.wonderhowto.com/forum/wifi-hacking-attach-usb-wireless-adapter-with-virtual-box-0324433/) may help.

-----

Question: The driver installation script completed successfully and the
driver is installed but does not seem to be working. What is wrong?

Answer: Turn secure boot off to see if that allows the driver to work.
This driver is primarily tested on Debian based distros such as Ubuntu,
Raspberry Pi OS and Kali. In an attempt to make this driver work well on
many Linux distros, other distros, including the Arch based Manjaro is
used for testing. Currently I do not have installations of Fedora or
OpenSUSE available for testing and reply on user reports of success or
failure. I have two test systems with secure boot on so as to test secure
boot. I have not seen any secure boot problems with Debian based systems
and I don't remember problems with Manjaro.

dkms is used in the installation script. It helps with a lot of issues that
will come up if a simple manual installation is used. dkms has the
capability to handle the needs of secure boot. dkms was written by and is
maintained by Dell. Dell has been offering some Ubuntu pre-loaded systems
for years so their devs likely test on Ubuntu. I suspect Fedora and
OpenSUSE may be handing their secure boot support differently than Debian
based systems and this is leading to problems. This and the other repos
I have are VERY heavily used and I am sure there are plenty of non-Debian
users that use this driver. Are they all turning off secure boot and not
reporting the problem? I don't know. What I do know is that reports like
this are rare.

For the driver to compile and install correctly but not be available
tells me there is likely a key issue. Here is an interesting link
regarding Debian systems and secure boot:

https://wiki.debian.org/SecureBoot

That document contains a lot of information that can help an investigation
into what the real problem is and I invite you and other Fedora, OpemSUSE
and users of other distros that show this problem to investigate and
present what you know to the devs of your distro via their problem
reporting system. Turning off secure boot is NOT a fix. A real fix needs
to happen.

-----

Question: Can you provide additional information about monitor mode?

Answer: I have a repo that is setup to help with monitor mode:

https://github.com/morrownr/Monitor_Mode

Work to improve monitor mode is ongoing with this driver. Your
reports of success or failure are needed. If you have yet to buy an
adapter to use with monitor mode, there are adapters available that are
known to work very well with monitor mode. My recommendation for those
looking to buy an adapter for monitor mode is to buy adapters based on
the following chipsets: mt7921au, mt7612u, mt7610u, rtl8812au and
rtl8811au. My specific recommendations for adapters in order of
preference are:

ALFA AWUS036ACHM - long range - in-kernel driver

ALFA AWUS036ACM - in-kernel driver

ALFA AWUS036ACH - long range - [driver](https://github.com/morrownr/8812au-20210629)

ALFA AWUS036ACS - [driver](https://github.com/morrownr/8821au-20210708)

To ask questions, go to [USB-WiFi](https://github.com/morrownr/USB-WiFi)
and post in `Discussions` or `Issues`.

-----

Question: I have an adapter with the 8821cu chipset and it supports
bluetooth. The bluetooth works but the wifi does not. What is wrong?

Answer: There appears to be an issue where adapters can be set up differently
by makers. The fix is to set the driver option ( `rtw_RFE_type` ) in 8821cu.conf.
The easiest way to edit 8821cu.conf is to run the following from the driver
directory:

```
sudo ./edit-options.sh
```

Once in the document, you can scroll down to the documentation about
`rtw_RFE_type`. You will likely have to experiment to find out what setting
works best for your adapter but a good place to start is probably...

```
rtw_RFE_type=7
```

Simply add that option to the end of the `options` line, save and reboot.

-----

#### [Go to Main Menu](https://github.com/morrownr/USB-WiFi)

-----

