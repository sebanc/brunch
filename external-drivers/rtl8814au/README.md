### All-Points Bulletin

I need your help. We need to find a newer version of the source code for
this driver. While working on source code for other more recent drivers I
have noticed indications that Realtek may still working on this 8814au
driver. I have not been able to locate a newer version. This driver is
v5.8.5.1 dated 20191029. It is getting hard to maintain and does not have
many of the features that other, more modern drivers do. If we can find a
driver for the 8814au that is up to date, we can make better use of our
8814au adapters. A newer driver should have version numbers of 5.12.x,
5.13.x or higher. Please search all locations that might have a new version
available. Ask retailers. Please help.

-----

### 8814au ( 8814au.ko ) :rocket:

### Linux Driver for USB WiFi Adapters that are based on the RTL8814AU Chipset

- v5.8.5.1 (Realtek) (20191029)
- Plus updates from the Linux community

### Features

- IEEE 802.11 b/g/n/ac WiFi compliant
- 802.1x, WEP, WPA TKIP and WPA2 AES/Mixed mode for PSK and TLS (Radius)
- IEEE 802.11b/g/n/ac Client mode
  * Supports wireless security for WEP, WPA TKIP and WPA2 AES PSK
  * Supports site survey scan and manual connect
  * Supports power saving mode
- Supported interface modes
  * IBSS
  * Managed
  * Monitor (see FAQ)
  * AP (see FAQ)
- USB mode control
- Log level control
- LED control
- Power saving control
- VHT control (allows 80 MHz channel width in AP mode)

### A FAQ is available at the end of this document

### Compatible CPUs

- x86, amd64
- ARM, ARM64

### Compatible Kernels

- Kernels: 4.19 - 5.2 (Realtek)
- Kernels: 5.3  - 6.1 (community support)

### Tested Linux Distributions

Note: One of the goals of this project is to provide driver support that
is easy to install and works reliably on many distros. Meeting this goal
depends on you to report your recommendations and updated information. 
If you see information that needs to be updated, please report the
updated information and if you do not see adequate support for
items such as Installation Steps 2, 3 and 9, and you know what updates 
need to added or you can get that information, please provide it so that
the Installation Steps can be improved.

- Arch Linux (kernels 5.4 and 5.11)

- Debian 11 (kernels 5.10 and 5.15)

- Fedora (kernel 5.11)

- Kali Linux (kernel 5.10)

- Manjaro 21.1 (kernel 5.13)

- openSUSE Tumbleweed (rolling) (kernel 5.15)

- Raspberry Pi OS (2022-09-22) (ARM 32 bit and 64 bit) (kernel 5.15)

- Raspberry Pi Desktop (2022-07-01) (x86 32 bit) (kernel 5.10)

- Ubuntu 22.04 (kernel 5.15)

- Void Linux (kernel 5.18)

### Download Locations for Tested Linux Distributions

- [Arch Linux](https://www.archlinux.org)
- [Debian](https://www.debian.org/)
- [Fedora](https://getfedora.org)
- [Kali Linux](https://www.kali.org/)
- [Manjaro](https://manjaro.org)
- [openSUSE](https://www.opensuse.org/)
- [Raspberry Pi OS](https://www.raspberrypi.org)
- [Ubuntu](https://www.ubuntu.com)
- [Void Linux](https://voidlinux.org/)

### Tested Hardware

- ASUS USB-AC68 AC1900 Dual-Band USB 3.0 WiFi Adapter
- COMFAST CF-958AC

### Compatible Devices

Warning: Adapters listed here are not recommended for purchase as I do
not recommend Linux users buy Realtek based USB WiFi adapters due to the
lack of mac80211 technology drivers that are supported in-kernel as
called for by Linux Wireless Standards. This repo is supported for the
benefit of Linux users who already have adapters based on the supported
chipsets. If you are looking for information about what adapter to buy,
click [here](https://github.com/morrownr/USB-WiFi) for information about
and links to recommended adapters.

* ALFA AWUS1900
* ASUS USB-AC68 AC1900 Dual-Band USB 3.0 WiFi Adapter
* Edimax EW-7833 UAC AC1750 Dual-Band Wi-Fi USB 3.0 Adapter
* COMFAST CF-958AC
* Numerous adapters that are based on the supported chipset.

Note: Please read "supported-device-IDs" for information about how to confirm the correct driver for your adapter.

### Installation Information

Warning: Installing multiple drivers for the same hardware usually does
not end well. If a previous attempt to install this driver failed or if
you have previously installed another driver for chipsets supported by
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
5.18 to 5.19, you need to remove the driver you have installed and
install the newest available before installing the new kernel. Use the
following commands in the driver directory:

```
$ git pull
$ sudo ./remove-driver.sh
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

DKMS is used for the installation. DKMS is a system utility which will
automatically recompile and reinstall this driver when a new kernel is
installed. DKMS is provided by and maintained by Dell.

It is recommended that you do not delete the driver directory after installation
as the directory contains information and scripts that you may need in the future.

Secure mode: The primary installation script, `install-driver.sh`, will support
secure mode... if your distro supports the method dkms uses. I regularly test the
installation script on systems with secure mode on. It works very well on Ubuntu based
distros. Some distros, such as Raspberry Pi OS, do not support secure mode because the
hardware they support does not support secure mode making it unnecessary. There are
distros that do not work with the support currently in use. If you install this driver
and, after a reboot, the driver is not working, you can go into the BIOS and temporarily
turn secure mode off to see if secure mode is the problem.

### Installation Steps

Note: The installation instructions are for the novice user. Experienced users are
welcome to alter the installation to meet their needs. Support will be provided based
on the steps below.

#### Step 1: Open a terminal (e.g. Ctrl+Alt+T)

#### Step 2: Update and upgrade system packages (select the option for the OS you are using)

Note: If your Linux distro does not fall into one of options listed
below, you will need to research how to update and upgrade your system
packages.

- Option for Debian based distributions such as Ubuntu, Kali and Raspberry Pi OS

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
sudo zypper up
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

- Option for Raspberry Pi OS (ARM/ARM64)

```
sudo apt install -y raspberrypi-kernel-headers build-essential bc dkms git
```

- Option for Debian, Kali, and Raspberry Pi Desktop (x86)

```
sudo apt install -y linux-headers-$(uname -r) build-essential bc dkms git libelf-dev
```

- Option for Ubuntu (all official flavors) and the numerous Ubuntu based distros

```
sudo apt install -y build-essential dkms git 
```

- Option for Fedora

```
sudo dnf -y install git dkms kernel-devel kernel-debug-devel
```

- Option for openSUSE

```
sudo zypper in -t pattern devel_kernel
sudo zypper in dkms git
```

- Option for Void Linux

```
sudo xbps-install linux-headers dkms git make
```

- Options for Arch and Manjaro

If using pacman

```
sudo pacman -S --noconfirm linux-headers dkms git
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
git clone https://github.com/morrownr/8814au.git
```

#### Step 7: Move to the newly created driver directory

```
cd ~/src/8814au
```

#### Step 8: Run a script to reconfigure for ARM or ARM64 based systems

Warning: This driver defaults to supporting x86 and amd64 based systems
and this step should be `skipped` if your system is powered by an x86, 
amd64 or compatible CPU.

Note: If your system is powered by an ARM or ARM64 based Raspberry Pi,
then one of the following scripts should be executed:

- Option for the following listed operating systems to be installed to
Raspberry Pi hardware

```
       * Raspberry Pi OS (32 bit)
```

```
./ARM_RPI.sh
```

- Option for the following listed operating systems to be installed to
Raspberry Pi hardware

```
       * Raspberry Pi OS (64 bit)
       * Kali Linux RPI ARM64
       * Ubuntu for Raspberry Pi
```

```
./ARM64_RPI.sh
```

Note: ARM or ARM64 based systems not listed above will likely require
modifications similar to those provided in the above scripts but the
number and variety of different ARM and ARM64 based systems makes
supporting each system unpractical so you will need to research the
needs of your system and make the appropriate modifications. If you
discover the settings and make a new script that works with your ARM or
ARM64 based system, you are welcome to submit the script and information
to be included here.

#### Step 10: Run the installation script ( install-driver.sh )

Note: For automated builds (non-interactive), use _NoPrompt_ as an option.

```
sudo ./install-driver.sh
```

Note: If you elect to skip the reboot at the end of the installation
script, the driver may not load immediately and the driver options will
not be applied. Rebooting is strongly recommended.

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

A file called `8814au.conf` will be installed in `/etc/modprobe.d` by
default if you use the `./install-driver.sh` script.

Note: The installation script will prompt you to edit the options.

Location: `/etc/modprobe.d/8814au.conf`

This file will be read and applied to the driver on each system boot.

To edit the driver options file, run the `edit-options.sh` script

```
sudo ./edit-options.sh
```

Note: Documentation for Driver Options is included in the file `8814au.conf`.

-----

### Removal of the Driver ( remove-driver.sh )

Note: Removing the driver is advised in the following situations:

- if driver installation fails
- if the driver is no longer needed
- if a new or updated version of the driver needs to be installed
- if a distro version upgrade is going to be installed (i.e. going from kernel 5.10 to kernel 5.15)

Note: The following removes everything that has been installed, with the
exception of the packages installed in Step 3 and the driver directory.
The driver directory can be deleted after running this script.

#### Step 1: Open a terminal (e.g. Ctrl+Alt+T)

#### Step 2: Move to the driver directory

```
cd ~/src/8814au
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

- Check congestion: There are apps available for smart phones that allow you to check the congestion levels on WiFi channels. The apps generally go by the name of ```WiFi Analyzer``` or something similar.

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

Question: What extended features does this driver support?

Answer: None. For extended features, you need an adapter that uses Mediatek or
Atheros drivers.

-----

Question: I bought two rtl8814au based adapters and am planning to use both in
the same computer. How do I set that up?

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

#### [Go to Main Menu](https://github.com/morrownr/USB-WiFi)

-----

