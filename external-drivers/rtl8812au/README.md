-----

#### [Click for USB WiFi Adapter Information for Linux](https://github.com/morrownr/USB-WiFi)

-----

#### A FAQ is available at the end of this document.

-----

#### Problem reports go in `Issues`. Include the information obtained with:

```
sudo uname -a; mokutil --sb-state; lsusb; rfkill list all; dkms status; iw dev
```

-----

## 8812au ( 8812au.ko ) :rocket:

## Linux Driver for USB WiFi Adapters that are based on the RTL8812AU Chipset

- v5.13.6 (Realtek) (20210629) plus updates from the Linux community
- 3,122 clones over the 2 weeks ended on 20211218

### Features

- IEEE 802.11 b/g/n/ac WiFi compliant
- 802.1x, WEP, WPA TKIP and WPA2 AES/Mixed mode for PSK and TLS (Radius)
- WPA3 (see pinned issue with title `How to Enable WPA3 support`)
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
  * IBSS
  * Managed
  * Monitor (see `Monitor_Mode.md` in the `docs` folder.)
  * AP (see `Bridged_Wireless_Access_Point.md` in the `docs` folder.)
  * P2P-client
  * P2P-GO
  * Concurrent (see `Concurrent_Mode.md` in the `docs` folder.)
- Log level control
- LED control
- Power saving control
- VHT control (allows 80 MHz channel width in AP mode)
- SU Beamformee and MU Beamformee control
- SU Beamformer control
- AP mode DFS channel control
- USB mode control

### Compatible CPUs

- x86, amd64
- ARM, ARM64
- MIPS

### Compatible Kernels

- Kernels: 4.4  - 5.11 (Realtek)
- Kernels: 5.12 - 5.16 (community support)

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

- Fedora (kernel 5.11)

- Debian 11 (kernels 5.10 and 5.15)

- Kali Linux (kernel 5.10)

- Linux Mint 20.2 (Linux Mint based on Ubuntu) (kernels 5.4 and 5.13)

- LMDE 4 (Linux Mint based on Debian) (kernel 4.19)

- Manjaro 20.1 (kernel 5.9) and 21.1 (kernel 5.13)

- openSUSE Tumbleweed (rolling) (kernel 5.15)

- Raspberry Pi OS (2021-10-30) (ARM 32 bit) (kernel 5.10)

- Raspberry Pi Desktop (x86 32 bit) (kernel 4.19)

- Ubuntu 20.xx (kernels 5.4 and 5.8) and 21.xx (kernels 5.11 and 5.13)

### Download Locations for Tested Linux Distributions

- [Arch Linux](https://www.archlinux.org)
- [Debian](https://www.debian.org/)
- [Fedora](https://getfedora.org)
- [Kali Linux](https://www.kali.org/)
- [Linux Mint](https://www.linuxmint.com)
- [Manjaro](https://manjaro.org)
- [openSUSE](https://www.opensuse.org/)
- [Raspberry Pi OS](https://www.raspberrypi.org)
- [Ubuntu](https://www.ubuntu.com)

### Tested Hardware

- [ALFA - AWUS036ACH](https://store.rokland.com/collections/wi-fi-usb-adapters/products/alfa-awus036ach-802-11ac-high-power-ac1200-dual-band-wifi-usb-adapter)

### Compatible Devices

* [ALFA AWUS036AC](https://store.rokland.com/collections/wi-fi-usb-adapters/products/alfa-awus036ac-802-11ac-long-range-dual-band-wifi-usb-adapter)
* [ALFA AWUS036ACH](https://store.rokland.com/collections/wi-fi-usb-adapters/products/alfa-awus036ach-802-11ac-high-power-ac1200-dual-band-wifi-usb-adapter)
* [ALFA AWUS036EAC](https://store.rokland.com/collections/wi-fi-usb-adapters/products/alfa-awus036eac-802-11ac-ac1200-dual-band-wifi-usb-adapter-dongle)
* ASUS USB-AC56 Dual-Band AC1200 Adapter (H/W ver. A1)
* Belkin F9L1109
* Buffalo - WI-U3-866D
* [Edimax EW-7822UAC](https://www.amazon.com/Edimax-EW-7822UAC-Dual-Band-Adjustable-Performance/dp/B00BXAXO7C) - Edimax made the source for this driver available.
* Linksys WUSB6300 V1
* Rosewill RNX-AC1200UBE
* TRENDnet TEW-805UB
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
dkms status
```

The installation instructions are for the novice user. Experienced users are welcome to alter the installation to meet their needs.

Temporary internet access is required for installation. There are numerous ways to enable temporary internet access depending on your hardware and situation. [One method is to use tethering from a phone.](https://www.makeuseof.com/tag/how-to-tether-your-smartphone-in-linux) Another method is to keep a [WiFi adapter that uses an in-kernel driver](https://github.com/morrownr/USB-WiFi) in your toolkit.

You will need to use the terminal interface. The quick way to open a terminal: Ctrl+Alt+T (hold down on the Ctrl and Alt keys then press the T key).

An alternative terminal is to use SSH (Secure Shell) from the same or from another computer, in which case you will be in a suitable terminal after logging in, but this step requires that an SSH daemon/server has already been configured. (There are lots of SSH guides available, e.g., for the [Raspberry Pi](https://www.raspberrypi.com/documentation/computers/remote-access.html#setting-up-an-ssh-server) and for [Ubuntu](https://linuxconfig.org/ubuntu-20-04-ssh-server). Do not forget [to secure the SSH server](https://www.howtogeek.com/443156/the-best-ways-to-secure-your-ssh-server/).)

You will need to have sufficient access rights to use `sudo` so that commands can be executed as the `root` user. (If the command `sudo echo Yes` returns "Yes", with or without having to enter your password, you do have sufficient access rights.)

DKMS is used for the installation. DKMS is a system utility which will automatically recompile and reinstall this driver when a new kernel is installed. DKMS is provided by and maintained by Dell.

It is recommended that you do not delete the driver directory after installation as the directory contains information and scripts that you may need in the future.

There is no need to disable Secure Mode to install this driver. If Secure Mode is properly setup on your system, this installation will support it.

### Installation Steps

#### Step 1: Open a terminal (e.g. Ctrl+Alt+T)

#### Step 2: Update the system package information (select the option for the OS you are using)

- Option for Debian based distributions such as Ubuntu, Linux Mint, Kali and Raspberry Pi OS

```
sudo apt update
```

- Option for Arch based distributions such as Manjaro

```
sudo pacman -Syu
```

- Option for Fedora based distributions

```
sudo dnf -y update
```

- Option for openSUSE based distributions

```
sudo zypper update
```

Note: If you do not regularly maintain your system by installing updated
packages, please do so now and then reboot. The rest of the installation
will appreciate having a fully up to date system to work with. The
installation can then be continued with Step 3.

#### Step 3: Install the required packages (select the option for the OS you are using)

- Option for Raspberry Pi OS

```
sudo apt install -y raspberrypi-kernel-headers bc build-essential dkms git
```

- Option for Debian, Kali and Linux Mint Debian Edition (LMDE)

```
sudo apt install -y linux-headers-$(uname -r) build-essential dkms git libelf-dev
```

- Option for Ubuntu (all flavors) and Linux Mint

```
sudo apt install -y dkms git build-essential
```

- Option for Fedora

```
sudo dnf -y install git dkms kernel-devel kernel-debug-devel
```

- Option for openSUSE

```
sudo zypper install -t pattern devel_kernel dkms
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
git clone https://github.com/morrownr/8812au-20210629.git
```

#### Step 7: Move to the newly created driver directory

```
cd ~/src/8812au-20210629
```

#### Step 8: (optional) Enable Concurrent Mode ( cmode-on.sh )

Note: see `Concurrent_Mode.md` in the `docs` folder to help determine
whether you want to enable Concurrent Mode.

```
./cmode-on.sh
```

#### Step 9: Run a script to reconfigure for Raspberry Pi hardware

Warning: This step only applies if you are installing to Raspberry Pi
*hardware*.

Warning: You should skip this step if installing to x86 or amd64 based
systems.

- Option for the 32 bit Raspberry Pi OS to be installed to Raspberry Pi hardware

```
./raspiOS-32.sh
```

- Option for the 64 bit Raspberry Pi OS to be installed to Raspberry Pi hardware
       
```
./raspiOS-64.sh
```

Note: The best option for other 64 bit operating systems to be
installed to Raspberry Pi hardware is to use the 64 bit option. An
example is Ubuntu for Raspberry Pi.

Note: Other ARM or ARM64 based systems will likely require modifications
similar to those provided in the above scripts for Raspberry Pi hardware
but the number and variety of different ARM and ARM64 based systems
makes supporting each system unpractical so you will need to research
the needs of your system and make the appropriate modifications. If you
discover the settings and make a new script that works with your ARM or
ARM64 based system, you are welcome to submit the script and information
to be included here.

#### Step 10: Run the installation script ( install-driver.sh )

Note: For automated builds, use _NoPrompt_ as an option.

```
sudo ./install-driver.sh
```

Note: If you elect to skip the reboot at the end of the installation
script, the driver may not load immediately and the driver options will
not be applied. Rebooting is strongly recommended.

### Driver Options ( edit-options.sh )

A file called `8812au.conf` will be installed in `/etc/modprobe.d` by
default.

Note: The installation script will prompt you to edit the options.

Location: `/etc/modprobe.d/8812au.conf`

This file will be read and applied to the driver on each system boot.

To edit the driver options file, run the `edit-options.sh` script

```
sudo ./edit-options.sh
```

Note: Documentation for Driver Options is included in the file `8812au.conf`.

### Removal of the Driver ( remove-driver.sh )

Note: This script should be used in the following situations:

- if driver installation fails
- if the driver is no longer needed
- if a new or updated version of the driver needs to be installed
- if a distro version upgrade is going to be installed

Note: This script removes everything that has been installed, with the
exception of the packages installed in Step 3 and the driver directory.
The driver directory can and probably should be deleted in most cases
after running this script.

#### Step 1: Open a terminal (e.g. Ctrl+Alt+T)

#### Step 2: Move to the driver directory

```
cd ~/src/8812au-20210629
```

#### Step 3: Run the removal script

```
sudo ./remove-driver.sh
```

### Recommended WiFi Router/ Access Point Settings

Note: These are general recommendations, some of which may not apply to your specific situation.

- Security: Set WPA2-AES. Do not set WPA2 mixed mode or WPA or TKIP.

- Channel width for 2.4 GHz: Set 20 MHz fixed width. Do not use 40 MHz or 20/40 automatic.

- Channels for 2.4 GHz: Set channel 1 or 6 or 11 depending on the congestion at your location. Do not set automatic channel selection. As time passes, if you notice poor performance, recheck congestion and set channel appropriately. The environment around you can and does change over time.

- Mode for 2.4 GHz: For best performance, set "N only" if you no longer use B or G capable devices.

- Network names: Do not set the 2.4 GHz Network and the 5 GHz Network to the same name. Note: Unfortunately many routers come with both networks set to the same name. You need to be able to control which network that is in use so changing the name of one of the networks is recommended. Since many IoT devices use the 2.4 GHz network, it may be better to change the name of the 5 GHz network.

- Channels for 5 GHz: Not all devices are capable of using DFS channels (I'm looking at you Roku.) It may be necessary to set a fixed channel in the range of 36 to 48 or 149 to 165 in order for all of your devices to work on 5 GHz. (For US, other countries may vary.)

- Best location for the WiFi router/access point: Near center of apartment or house, at least a couple of feet away from walls, in an elevated location. You may have to test to see what the best location is in your environment.

- Check congestion: There are apps available for smart phones that allow you to check the congestion levels on WiFi channels. The apps generally go by the name of ```WiFi Analyzer``` or something similar.

After making and saving changes, reboot the router.


### Check and set regulatory domain

Check the current setting

```
sudo iw reg get
```

If you get 00, that is the default and may not provide optimal performance.

Find the correct setting here: http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2

Set it temporarily

```
sudo iw reg set US
```

Note: Substitute your country code if you are not in the United States.

Set it permanently

```
sudo nano /etc/default/crda
```

Change the last line to read:

```
REGDOMAIN=US
```

### Recommendations regarding USB

- Moving your USB WiFi adapter to a different USB port has been known to fix a variety of problems.

- If connecting your USB WiFi adapter to a desktop computer, use the USB ports on the rear of the computer. Why? The ports on the rear are directly connected to the motherboard which will reduce problems with interference and disconnection.

- If your USB WiFi adapter is USB 3 capable and you want it to operate in USB3 mode, plug it into a USB 3 port.

- Avoid USB 3.1 Gen 2 ports if possible as almost all currently available adapters have been tested with USB 3.1 Gen 1 (aka USB 3) and not with USB 3.1 Gen 2.

- If you use an extension cable and your adapter is USB 3 capable, the cable needs to be USB 3 capable (if not, you will at best be limited to USB 2 speeds).

- Extention cables can be problematic. A way to check if the extension cable is the problem is to plug the adapter temporarily into a USB port on the computer.

- Some USB WiFi adapters require considerable electrical current and push the capabilities of the power available via USB port. One example is adapters that use the Realtek 8814au chipset. Using a powered multiport USB extension can be a good idea in cases like this.


### How to disable onboard WiFi on Raspberry Pi 3B, 3B+, 3A+, 4B and Zero W

Add the following line to /boot/config.txt

```
dtoverlay=disable-wifi
```

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

Answer: WPA3-SAE support is in this driver according to Realtek,
however, for it to work with some current Linux distros, you will need
to download, compile and install the current development version of
wpa_supplicant at the following site:

https://w1.fi/cgit/

See issue titled `How to Enable WPA3 support` for more information.


Question: I bought two rtl8812au based adapters and am planning to run one of them as an AP and another as a WiFi client. How do I set that up?

Answer: You can't without considerable technical skills.  Realtek drivers do not support more than one adapter with the same chipset in the same computer. You have multiple Realtek based adapters in the same computer as long as the adapters are based on different chipsets. Testing has shown that the Mediatek drivers do support more than one adapter with the same chipset in various configurations.


Question: Why do you recommend Mediatek based adapters when you maintain this repo for a Realtek driver?

Answer: Many new and existing Linux users already have adapters based on Realtek chipsets. This repo is for Linux users to support their existing adapters but my STRONG recommendation is for Linux users to seek out WiFi solutions based on Mediatek, Intel or Atheros chipsets and in-kernel drivers. If users are looking at a USB solution, Mediatek and Atheros based adapters are the best solution. Realtek based USB adapters are not a good solution because Realtek does not follow Linux Wireless standards (mac80211) for USB WiFi adapters and the drivers are not maintained in the Linux kernel. These issues make Realtek drivers problematic in many ways. You have been WARNED. For more information about USB WiFi adapters:

https://github.com/morrownr/USB-WiFi


Question: Will you put volunteers to work?

Answer: Yes. Post a message in `Issues` or `Discussions` if interested.


Question: I am having problems with my adapter and I use Virtualbox?

Answer: This [article](https://null-byte.wonderhowto.com/forum/wifi-hacking-attach-usb-wireless-adapter-with-virtual-box-0324433/) may help.

-----
