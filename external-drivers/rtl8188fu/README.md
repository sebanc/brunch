RTL8188FU driver for Linux kernel 4.15.x ~ 5.11.x (Linux Mint, Ubuntu or Debian Derivatives)

------------------

## How to install

`sudo apt-get install build-essential git dkms linux-headers-$(uname -r)`

`git clone https://github.com/kelebek333/rtl8188fu`

`sudo dkms add ./rtl8188fu`

`sudo dkms build rtl8188fu/1.0`

`sudo dkms install rtl8188fu/1.0`

`sudo cp ./rtl8188fu/firmware/rtl8188fufw.bin /lib/firmware/rtlwifi/`

------------------

Run following commands for disable power management and plugging/replugging issues.

`sudo mkdir -p /etc/modprobe.d/`

`sudo touch /etc/modprobe.d/rtl8188fu.conf`

`echo "options rtl8188fu rtw_power_mgnt=0 rtw_enusbss=0" | sudo tee /etc/modprobe.d/rtl8188fu.conf`


Run following commands for disabling MAC Address Spoofing (Note: No need this on Ubuntu based distributions. MAC Address Spoofing is already disable on Ubuntu base).

`sudo mkdir -p /etc/NetworkManager/conf.d/`

`sudo touch /etc/NetworkManager/conf.d/disable-random-mac.conf`

`echo -e "[device]\nwifi.scan-rand-mac-address=no" | sudo tee /etc/NetworkManager/conf.d/disable-random-mac.conf`


------------------

## How to uninstall

`sudo dkms remove rtl8188fu/1.0 --all`

`sudo rm -f /lib/firmware/rtlwifi/rtl8188fufw.bin`

`sudo rm -f /etc/modprobe.d/rtl8188fu.conf`


------------------

## How to install from PPA repository

You can install rtl8188fu driver with following commands from PPA.

for xUbuntu 16.04-18.04-20.04-20.10 / Linux Mint 18.x-19.x-20.x

`sudo add-apt-repository ppa:kelebek333/kablosuz`

`sudo apt-get update`

`sudo apt install rtl8188fu-dkms`


You can purge packages with following commands

`sudo apt purge rtl8188fu-dkms`

------------------

## How to install (for arm devices)

https://github.com/kelebek333/rtl8188fu/tree/arm#how-to-install-for-arm-devices
