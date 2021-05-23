# Realtek RTL8811CU/RTL8821CU USB wifi adapter driver version 5.4.1 for Linux 4.4.x up to 5.x

Before build this driver make sure `make`, `gcc`, `linux-header`/`kernel-devel`, `bc` and `git` have been installed.

## First, clone this repository
```
mkdir -p ~/build
cd ~/build
git clone https://github.com/brektrou/rtl8821CU.git
```
## Check the name of the interface

Check the interface name of your wifi adapter using `ifconfig`. Usually, it will be wlan0 by default, but it may vary depends on the kernel and your device. On Ubuntu, for example, it may be named as wlx + MAC address. (https://www.freedesktop.org/wiki/Software/systemd/PredictableNetworkInterfaceNames/) 

If this is the case, you can either disable the feature following the link above, or replace the name used in the driver by

```
grep -lr . | xargs sed -i '' -e '/ifcfg-wlan0/!s/wlan0/<name of the device>/g'
```

## Build and install with DKMS

DKMS is a system which will automatically recompile and install a kernel module when a new kernel gets installed or updated. To make use of DKMS, install the dkms package.

### Debian/Ubuntu:
```
sudo apt-get install dkms
```
### Arch Linux/Manjaro:
```
sudo pacman -S dkms
```
To make use of the **DKMS** feature with this project, just run:
```
./dkms-install.sh
```
If you later on want to remove it, run:
```
./dkms-remove.sh
```

### Plug your USB-wifi-adapter into your PC
If wifi can be detected, congratulations.
If not, maybe you need to switch your device usb mode by the following steps in terminal:
1. find your usb-wifi-adapter device ID, like "0bda:1a2b", by type:
```
lsusb
```
2. switch the mode by type: (the device ID must be yours.)

Need install `usb_modeswitch` (Archlinux: `sudo pacman -S usb_modeswitch`)
```
sudo usb_modeswitch -KW -v 0bda -p 1a2b
systemctl start bluetooth.service - starting Bluetooth service if it's in inactive state
```

It should work.

### Make it permanent

If steps above worked fine and in order to avoid periodically having to make `usb_modeswitch` you can make it permanent (Working in **Ubuntu 18.04 LTS**):

1. Edit `usb_modeswitch` rules:

   ```bash
   sudo nano /lib/udev/rules.d/40-usb_modeswitch.rules
   ```

2. Append before the end line `LABEL="modeswitch_rules_end"` the following:

   ```
   # Realtek 8211CU Wifi AC USB
   ATTR{idVendor}=="0bda", ATTR{idProduct}=="1a2b", RUN+="/usr/sbin/usb_modeswitch -K -v 0bda -p 1a2b"
   ```   

## Build and install without DKMS
Use following commands:
```
cd ~/build/rtl8821CU
make
sudo make install
```
If you later on want to remove it, do the following:
```
cd ~/build/rtl8821CU
sudo make uninstall
```
## Checking installed driver
If you successfully install the driver, the driver is installed on `/lib/modules/<linux version>/kernel/drivers/net/wireless/realtek/rtl8821cu`. Check the driver with the `ls` command:
```
ls /lib/modules/$(uname -r)/kernel/drivers/net/wireless/realtek/rtl8821cu
```
Make sure `8821cu.ko` file present on that directory

### Check with **DKMS** (if installing via **DKMS**):

``
sudo dkms status
``
### ARM architecture tweak for this driver (this solves compilation problem of this driver):
```
# For AArch32
sudo cp /lib/modules/$(uname -r)/build/arch/arm/Makefile /lib/modules/$(uname -r)/build/arch/arm/Makefile.$(date +%Y%m%d%H%M)
sudo sed -i 's/-msoft-float//' /lib/modules/$(uname -r)/build/arch/arm/Makefile
sudo ln -s /lib/modules/$(uname -r)/build/arch/arm /lib/modules/$(uname -r)/build/arch/armv7l

# For AArch64
sudo cp /lib/modules/$(uname -r)/build/arch/arm64/Makefile /lib/modules/$(uname -r)/build/arch/arm64/Makefile.$(date +%Y%m%d%H%M)
sudo sed -i 's/-mgeneral-regs-only//' /lib/modules/$(uname -r)/build/arch/arm64/Makefile

```
### Monitor mode
Use the tool 'iw', please don't use other tools like 'airmon-ng'
```
iw dev wlan0 set monitor none
```

