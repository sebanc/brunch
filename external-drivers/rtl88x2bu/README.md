# Administrative Note

As of upstream version 5.6.1, I'm moving away from individual repositories for each upstream version in favor of a single repository with version-based branches.  Hopefully, this will help with clutter and URL consistency moving forward.  The archived repositories are available here:
* [rtl88x2BU_WiFi_linux_v5.6.1.6_35492.20191025_COEX20180928-6a6a](https://github.com/cilynx/rtl88x2bu/tree/5.6.1.6_35492.20191025_COEX20180928-6a6a)
* [rtl88x2BU_WiFi_linux_v5.6.1_30362.20181109_COEX20180928-6a6a](https://github.com/cilynx/rtl88x2bu/tree/5.6.1_30362.20181109_COEX20180928-6a6a)
* [rtl88x2BU_WiFi_linux_v5.3.1_27678.20180430_COEX20180427-5959](https://github.com/cilynx/rtl88x2BU_WiFi_linux_v5.3.1_27678.20180430_COEX20180427-5959)
* [rtl88x2BU_WiFi_linux_v5.2.4.4_26334.20180126_COEX20171012-5044](https://github.com/cilynx/rtl88x2BU_WiFi_linux_v5.2.4.4_26334.20180126_COEX20171012-5044)
* [rtl88x2BU_WiFi_linux_v5.2.4.4_25643.20171212_COEX20171012-5044](https://github.com/cilynx/rtl88x2BU_WiFi_linux_v5.2.4.4_25643.20171212_COEX20171012-5044)
* [rtl88x2BU_WiFi_linux_v5.2.4.1_22719_COEX20170518-4444.20170613](https://github.com/cilynx/rtl88x2BU_WiFi_linux_v5.2.4.1_22719_COEX20170518-4444.20170613)

# Driver for rtl88x2bu wifi adaptors

Updated driver for rtl88x2bu wifi adaptors based on Realtek's source distributed with myriad adapters.

Realtek's 5.6.1.6 source was found bundled with the [Cudy WU1200 AC1200 High Gain USB Wi-Fi Adapter](https://amzn.to/351ADVq) and can be downloaded from [Cudy's website](http://www.cudytech.com/wu1200_software_download).

Build confirmed on:

```
Linux version 5.4.0-4-amd64 (debian-kernel@lists.debian.org) (gcc version 9.2.1 20200203 (Debian 9.2.1-28)) #1 SMP Debian 5.4.19-1 (2020-02-13)
```

## Using and Installing the Driver

### Simple Usage

In order to make direct use of the driver it should suffice to build the driver
with `make` and to load it with `insmod 88x2bu.ko`. This will allow you
to use the driver directly without changing your system persistently.

It might happen that your system freezes instantaneously. Ensure to not loose
important work by saving and such beforehand.

### DKMS installation

If you want to have the driver available at startup, it will be convenient to
register it in DKMS. An executable explanation of how to do so can be found in
the script `deploy.sh`. Since registering a kernel module in DKMS is a major
intervention, only execute it if you understand what the script does.

### Unknown Symbol Errors

Some users reported problems due to `Unknown symbol in module`. This can be
caused by old deployments of the driver still being present in the systems
directories. One solution reported was to forcefully remove all old driver
modules:

    sudo dkms remove rtl88x2bu/5.8.7.4 --all
    find /lib/modules -name cfg80211.ko -ls
    sudo rm -f /lib/modules/*/updates/net/wireless/cfg80211.ko


This can also be caused by cfg80211 module not being present in the kernel.
You can remedy this by running:

    sudo modprobe cfg80211

### Linux 5.18+ and RTW88 Driver

Starting from Linux 5.18, some distributions have added experimental RTW88 USB
support (include RTW88x2BU support). It is not yet stable but if it works well
on your system, then you no longer need this driver. But if it doesn't work or
is unstable, you need to manually blacklist it because it has a higher loading
priority than this external drivers.

Check the currently loaded module using `lsmod`. If you see `rtw88_core`,
`rtw88_usb`, or any name beginning with `rtw88_` then you are using the RTW88
driver. If you see `88x2bu` then you are using this RTW88x2BU driver.

To blacklist RTW88 8822bu USB driver, run the following command:

```
echo "blacklist rtw88_8822bu" > /etc/modprobe.d/rtw8822bu.conf
```

And reboot your system.


### Secure Boot

Secure Boot will prevent the module from loading as it isn't signed. In order
to check whether you have secure boot enabled, you couly run  `mokutil
--sb-state`. If you see something like `SecureBoot disabled`, you do not take
to setup module signing.

If Secure Boot is enabled on your machine, you either could disable it in BIOS
or UEFI or you could set up signing the module. How to do so is described
[here](https://github.com/cilynx/rtl88x2bu/issues/210#issuecomment-1166402943).


## Raspberry Pi Access Point

```bash
# Update all packages per normal
sudo apt update
sudo apt upgrade

# Install prereqs
sudo apt install git dnsmasq hostapd bc build-essential dkms raspberrypi-kernel-headers

# Reboot just in case there were any kernel updates
sudo reboot

# Pull down the driver source
git clone https://github.com/cilynx/rtl88x2bu
cd rtl88x2bu/

# Configure for RasPi
sed -i 's/I386_PC = y/I386_PC = n/' Makefile
sed -i 's/ARM_RPI = n/ARM_RPI = y/' Makefile

# DKMS as above
VER=$(sed -n 's/\PACKAGE_VERSION="\(.*\)"/\1/p' dkms.conf)
sudo rsync -rvhP ./ /usr/src/rtl88x2bu-${VER}
sudo dkms add -m rtl88x2bu -v ${VER}
sudo dkms build -m rtl88x2bu -v ${VER} # Takes ~3-minutes on a 3B+
sudo dkms install -m rtl88x2bu -v ${VER}

# Plug in your adapter then confirm your new interface name
ip addr

# Set a static IP for the new interface (adjust if you have a different interface name or preferred IP)
sudo tee -a /etc/dhcpcd.conf <<EOF
interface wlan1
    static ip_address=192.168.4.1/24
    nohook wpa_supplicant
EOF

# Clobber the default dnsmasq config
sudo tee /etc/dnsmasq.conf <<EOF
interface=wlan1
  dhcp-range=192.168.4.100,192.168.4.199,255.255.255.0,24h
EOF

# Configure hostapd
sudo tee /etc/hostapd/hostapd.conf <<EOF
interface=wlan1
driver=nl80211
ssid=pinet
hw_mode=g
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=CorrectHorseBatteryStaple
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

sudo sed -i 's|#DAEMON_CONF=""|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd

# Enable hostapd
sudo systemctl unmask hostapd
sudo systemctl enable hostapd

# Reboot to pick up the config changes
sudo reboot
```

If you want 802.11an speeds 144Mbps you could use this config below:
```
# Configure hostapd
sudo tee /etc/hostapd/hostapd.conf <<EOF
interface=wlx74ee2ae24062
driver=nl80211
ssid=borg

macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=toe54321
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP

hw_mode=a
channel=36
wmm_enabled=1

country_code=US

require_ht=1
ieee80211ac=1
require_vht=1

#This below is supposed to get us 867Mbps and works on rtl8814au doesn't work on this driver yet
#vht_oper_chwidth=1
#vht_oper_centr_freq_seg0_idx=157

ieee80211n=1
ieee80211ac=1
EOF

$ iwconfig
wlx74ee2ae24062  IEEE 802.11an  ESSID:"borg"  Nickname:"<WIFI@REALTEK>"
          Mode:Master  Frequency:5.18 GHz  Access Point: 74:EE:2A:E2:40:62
          Bit Rate:144.4 Mb/s   Sensitivity:0/0
          Retry:off   RTS thr:off   Fragment thr:off
          Power Management:off
          Link Quality=0/100  Signal level=-100 dBm  Noise level=0 dBm
          Rx invalid nwid:0  Rx invalid crypt:0  Rx invalid frag:0
          Tx excessive retries:0  Invalid misc:0   Missed beacon:0

```
If you want to setup
[masquerading](https://www.raspberrypi.org/documentation/configuration/wireless/access-point-routed.md)
or
[bridging](https://www.raspberrypi.org/documentation/configuration/wireless/access-point-bridged.md),
check out the official Raspberry Pi docs.
