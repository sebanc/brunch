2021-11-09

## Bridged Wireless Access Point

A bridged wireless access point (aka Dumb AP) works within an existing
ethernet network to add WiFi capability where it does not exist or to
extend the network to WiFi capable computers and devices in areas where
the WiFi signal is weak or otherwise does not meet expectations.

#### Single Band or Dual Band - Your Choice

This document outlines single band and dual band WiFi setups using a Raspberry
Pi 3B, 3B+ or 4B with an AC600 USB2 or AC1200 USB3 WiFi adapter for 5 GHz
band and either an additional external WiFi adapter or internal WiFi for 2.4 GHz
band. There is a lot of flexibility and capability available with this type of
setup.

#### Information

This setup supports WPA3-SAE. It is disabled by default.

WPA3-SAE will not work with some Realtek 88xx drivers. Let's just say that this
issue is in progress.

WPA3-SAE works with Mediatek 761x chipset based USB WiFI adapters and, as far as
I can tell, with all usb wifi adapters that use Linux in-kernel drivers and I
have tested many.

Note: This guide uses systemd-networkd for network management. If your Linux
distro uses Network Manager or Netplan, they must be disabled. Sections that
explain how to do this are located near the end of this document. Please go
to and follow the appropriate section now, if required, before continuing with
this setup guide. If you are using the Raspberry Pi OS, you may continue with
this setup guide now as the Raspberry Pi OS does not use Network Manager or
Netplan.

-----

#### Tested Setup

[Raspberry Pi 4B (4gb)](https://www.raspberrypi.org/products/raspberry-pi-4-model-b/)

[Raspberry Pi OS (2021-05-07) (32 bit) (kernel 5.10.17-v7l+)](https://www.raspberrypi.org/software/operating-systems/#raspberry-pi-os-32-bit)

Ethernet connection providing internet

[USB WiFi Adapter(s)](https://github.com/morrownr/USB-WiFi)

[Case](https://www.amazon.com/dp/B07X8RL8SL)

[Right Angle USB Extender](https://www.amazon.com/dp/B07S6B5X76)

[Power Supply](https://www.amazon.com/dp/B08C9VYLLK)

[SD Card](https://www.amazon.com/Samsung-Endurance-32GB-Micro-Adapter/dp/B07B98GXQT)

Note: I use the case upside down. There are several little things that
work better with the case upside down and no negatives that I can find.

Note: Very few Powered USB 3 Hubs will work well with Raspberry Pi hardware. The
primary problem has to do with the backfeeding of current into the Raspberry Pi.
One that seems to work well here is:

[Transcend USB 3.0 4-Port Hub TS-HUB3K](https://www.amazon.com/gp/product/B005D69QD8)

Note: The rtl88XXxu chipset based USB3 WiFi adapters require from 504 mA of
power up to well over 800 mA of power depending on the adapter. The Raspberry
Pi 3B, 3B+ and 4B USB subsystems are only able to supply a total of 1200
mA of power total divided between all attached devices.

Note: The Alfa AWUS036ACM adapter, a mt7612u based adapter, requests a maximum
of 400 mA from the USB subsystem during initialization. Testing with a meter
shows actual usage of 360 mA during heavy load and usage of 180 mA during
light loads. This is much lower power usage than most AC1200 class adapters
which makes this adapter a good choice for a Raspberry Pi based access point.

-----

#### Setup Steps

USB WiFi adapter driver installation, if required, should be performed and tested
prior to continuing.

Note: For USB3 adapters based on the Realtek rtl8812au, rtl8812bu and rtl8814au
chipsets, the following module parameters may be needed for best performance
when the adapter is set to support 5 GHz band: (if using a rtl8812bu based
adapter with a Raspberry Pi 4B or 400, you may need to limit USB mode to USB2
due to a bug, probably in the Raspberry Pi 4B, that causes dropped connections--
rtw_switch_usb_mode=2)

```
rtw_vht_enable=2 rtw_switch_usb_mode=1
```

Note: For USB2 adapters based on the Realtek rtl8811au chipset, the following
module parameters may be needed for best performance when the adapter is set to
support 5 GHz band:

```
rtw_vht_enable=2
```

Note: For USB3 adapters based on the Realtek rtl8812au, rtl8812bu and rtl8814au
chipsets, the following module parameters may be needed for best performance
when the adapter is set to support 2.4 GHz band:

```
rtw_vht_enable=1 rtw_switch_usb_mode=2
```

Note: For USB2 adapters based on the Realtek rtl8811au chipset, the following
module parameters may be needed for best performance when the adapter is set to
support 2.4 GHz band:

```
rtw_vht_enable=1
```

Note: For USB3 or USB2 adapters based on Mediatek mt7612u or mt7610u chipsets,
the following module parameter may be needed for best performance:

```
disable_usb_sg=1
```

Note: More information is available at the following site:

https://github.com/morrownr/7612u

Note: For this access point setup to support WPA3-SAE in a dual band setup, two
USB WiFi adapters with Mediatek or Atheros chipsets are required as the Realtek
and internal Raspberry Pi WiFi drivers do not support WPA3-SAE as of the date
of this document.

The follow site provides links to adapters that support WPA3-SAE: [USB-WIFI](https://github.com/morrownr/USB-WiFi)

-----

Update system.
```
sudo apt update
```

-----

Upgrade system.
```
sudo apt full-upgrade
```
Note: Upgrading system is not mandatory for this installation but since some
users forget to upgrade their system on a regular basis, maybe it is a good idea.

-----

Reduce overall power consumption and overclock the CPU a modest amount.

Note: All items in this step are optional and some items are specific to
the Raspberry Pi 4B. If installing to a Raspberry Pi 3b or 3b+ you will
need to use the appropriate settings for that hardward.
```
sudo nano /boot/config.txt
```
Change
```
# turn off onboard audio
dtparam=audio=off

# disable DRM VC4 V3D driver on top of the dispmanx display stack
#dtoverlay=vc4-fkms-v3d
#max_framebuffers=2
```
Add
```
# turn off Mainboard LEDs
dtoverlay=act-led

# disable Activity LED
dtparam=act_led_trigger=none
dtparam=act_led_activelow=off

# disable Power LED
dtparam=pwr_led_trigger=none
dtparam=pwr_led_activelow=off

# turn off Ethernet port LEDs
dtparam=eth_led0=4
dtparam=eth_led1=4

# turn off Bluetooth
dtoverlay=disable-bt

# turn off onboard WiFi
dtoverlay=disable-wifi

# overclock CPU
over_voltage=1
arm_freq=1600
```
-----

Enable predictable network interface names

Note: While this step is optional, problems can arise without it on dual band
setups. Some operating systems have this capability enabled by default but not
the Raspberry Pi OS.
```
sudo raspi-config
```
Select: Advanced options > A4 Network Interface Names > Yes

-----

Reboot system.
```
sudo reboot
```
-----

Determine name and state of the network interfaces.
```
ip a
```
You may need to additionally run the following commands in order to
determine which adapter, in a dual band setup, has which interface name.
```
iw list
```
```
iw dev
```
Note: If the interface names are not `eth0`, `wlan0` and `wlan1`,
then the interface names used in your system will have to replace
`eth0`, `wlan0` and `wlan1` for the remainder of this document.

-----

Install needed package. Website - [hostapd](https://w1.fi/hostapd/)
```
sudo apt install hostapd
```
-----

Enable the wireless access point service and set it to start when your
Raspberry Pi boots.
```
sudo systemctl unmask hostapd
```
```
sudo systemctl enable hostapd
```
-----

Note: The below steps include creating two hostapd configurations files but
only one is needed if using a single band setup.

Create hostapd configuration file for 5 GHz band.
```
sudo nano /etc/hostapd/hostapd-5g.conf
```
File contents
```
# /etc/hostapd/hostapd-5g.conf
# Documentation: https://w1.fi/cgit/hostap/plain/hostapd/hostapd.conf
# 2021-11-09

# SSID
ssid=myPI-5g
# PASSPHRASE
wpa_passphrase=myPW1234
# Band: a = 5g (a/n/ac), g = 2g (b/g/n)
hw_mode=a
# Channel
channel=36
# Channel width
vht_oper_chwidth=1
# VHT center channel (chan + 6)
vht_oper_centr_freq_seg0_idx=42
# Country code
country_code=US
# Bridge interface
bridge=br0
# WiFi interface
interface=wlan0

# nl80211 is used with all Linux mac80211 and modern Realtek drivers
driver=nl80211
#ctrl_interface=/var/run/hostapd
#ctrl_interface_group=0

ieee80211d=1
# Enables support for 5GHz DFS channels
#ieee80211h=1

beacon_int=100
dtim_period=2
max_num_sta=32
macaddr_acl=0
rts_threshold=2347
fragm_threshold=2346
#send_probe_response=1

# security
# auth_algs=3 is required for WPA-3 SAE and WPA-3 SAE Transitional
auth_algs=1
ignore_broadcast_ssid=0
# wpa=2 is required for WPA2 and WPA3 (read the docs)
wpa=2
rsn_pairwise=CCMP
# only one wpa_key_mgmt= line should be active.
# wpa_key_mgmt=WPA-PSK is required for WPA2-AES
wpa_key_mgmt=WPA-PSK
# wpa_key_mgmt=SAE WPA-PSK is required for WPA3-AES Transitional
#wpa_key_mgmt=SAE WPA-PSK
# wpa_key_mgmt=SAE is required for WPA3-SAE
#wpa_key_mgmt=SAE
#wpa_group_rekey=1800
# ieee80211w=1 is required for WPA-3 SAE Transitional
# ieee80211w=2 is required for WPA-3 SAE
#ieee80211w=1
# if parameter is not set, 19 is the default value.
#sae_groups=19 20 21 25 26
# sae_require_mfp=1 is required for WPA-3 SAE Transitional
#sae_require_mfp=1
# if parameter is not 9 set, 5 is the default value.
#sae_anti_clogging_threshold=10

# Note: Capabilities can vary even between adapters with the same chipset.
#
# Note: Only one ht_capab= line and one vht_capab= should be active. The
# content of these lines is determined by the capabilities of your adapter.
#
# IEEE 802.11n
ieee80211n=1
wmm_enabled=1
#
# mt7612u - mt7610u
#ht_capab=[HT40+][HT40-][GF][SHORT-GI-20][SHORT-GI-40]
#
# rtl8812au - rtl8811au - rtl8811cu
#ht_capab=[HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][MAX-AMSDU-7935]
# rtl8812bu
#ht_capab=[LDPC][HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][MAX-AMSDU-7935]
# rtl8814au
ht_capab=[LDPC][HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][MAX-AMSDU-7935][DSSS_CCK-40]
#

# IEEE 802.11ac
ieee80211ac=1
#
# mt7610u
#vht_capab=[SHORT-GI-80][MAX-A-MPDU-LEN-EXP3][RX-ANTENNA-PATTERN][TX-ANTENNA-PATTERN]
# mt7612u
#vht_capab=[RXLDPC][SHORT-GI-80][TX-STBC-2BY1][RX-STBC-1][MAX-A-MPDU-LEN-EXP3][RX-ANTENNA-PATTERN][TX-ANTENNA-PATTERN]
#
# rtl8812au - rtl8812bu
#vht_capab=[MAX-MPDU-11454][SHORT-GI-80][TX-STBC-2BY1][RX-STBC-1][HTC-VHT][MAX-A-MPDU-LEN-EXP7]
# rtl8814au
vht_capab=[MAX-MPDU-11454][RXLDPC][SHORT-GI-80][TX-STBC-2BY1][RX-STBC-1][HTC-VHT][MAX-A-MPDU-LEN-EXP7]
# rtl8811au - rtl8811cu
#vht_capab=[MAX-MPDU-11454][SHORT-GI-80][RX-STBC-1][HTC-VHT][MAX-A-MPDU-LEN-EXP7]
#
# Note: [TX-STBC-2BY1] may cause problems with some Realtek drivers

# Event logger - as desired
#logger_syslog=-1
#logger_syslog_level=2
#logger_stdout=-1
#logger_stdout_level=2

# WMM - as desired
#uapsd_advertisement_enabled=1
#wmm_ac_bk_cwmin=4
#wmm_ac_bk_cwmax=10
#wmm_ac_bk_aifs=7
#wmm_ac_bk_txop_limit=0
#wmm_ac_bk_acm=0
#wmm_ac_be_aifs=3
#wmm_ac_be_cwmin=4
#wmm_ac_be_cwmax=10
#wmm_ac_be_txop_limit=0
#wmm_ac_be_acm=0
#wmm_ac_vi_aifs=2
#wmm_ac_vi_cwmin=3
#wmm_ac_vi_cwmax=4
#wmm_ac_vi_txop_limit=94
#wmm_ac_vi_acm=0
#wmm_ac_vo_aifs=2
#wmm_ac_vo_cwmin=2
#wmm_ac_vo_cwmax=3
#wmm_ac_vo_txop_limit=47
#wmm_ac_vo_acm=0

# TX queue parameters - as desired
#tx_queue_data3_aifs=7
#tx_queue_data3_cwmin=15
#tx_queue_data3_cwmax=1023
#tx_queue_data3_burst=0
#tx_queue_data2_aifs=3
#tx_queue_data2_cwmin=15
#tx_queue_data2_cwmax=63
#tx_queue_data2_burst=0
#tx_queue_data1_aifs=1
#tx_queue_data1_cwmin=7
#tx_queue_data1_cwmax=15
#tx_queue_data1_burst=3.0
#tx_queue_data0_aifs=1
#tx_queue_data0_cwmin=3
#tx_queue_data0_cwmax=7
#tx_queue_data0_burst=1.5

# end of hostapd-5g.conf
```
-----

Create the 2g hostapd configuration file.
```
sudo nano /etc/hostapd/hostapd-2g.conf
```
File contents
```
# /etc/hostapd/hostapd-2g.conf
# Documentation: https://w1.fi/cgit/hostap/plain/hostapd/hostapd.conf
# 2021-11-09

# SSID
ssid=myPI-2g
# PASSPHRASE
wpa_passphrase=myPW1234
# Band: a = 5g (a/n/ac), g = 2g (b/g/n)
hw_mode=g
# Channel
channel=6
# Country
country_code=US
# Bridge interface
bridge=br0
# WiFi interface
interface=wlan1

# nl80211 is used with all Linux mac80211 and modern Realtek drivers
driver=nl80211
#ctrl_interface=/var/run/hostapd
#ctrl_interface_group=0

beacon_int=100
dtim_period=2
max_num_sta=32
rts_threshold=2347
fragm_threshold=2346
#send_probe_response=1

# security
# auth_algs=3 is required for WPA-3 SAE and WPA-3 SAE Transitional
auth_algs=1
macaddr_acl=0
ignore_broadcast_ssid=0
wpa=2
wpa_pairwise=CCMP
# WPA-2 AES
wpa_key_mgmt=WPA-PSK
# WPA-3 SAE
#wpa_key_mgmt=SAE
#wpa_group_rekey=1800
rsn_pairwise=CCMP
# ieee80211w=2 is required for WPA-3 SAE
#ieee80211w=2
# If parameter is not set, 19 is the default value.
#sae_groups=19 20 21 25 26
#sae_require_mfp=1
# If parameter is not 9 set, 5 is the default value.
#sae_anti_clogging_threshold=10

# IEEE 802.11n
ieee80211n=1
wmm_enabled=1
#
# Note: Only one ht_capab= line should be active. The content of these lines is
# determined by the capabilities of your adapter.
#
# RasPi4B internal wifi
ht_capab=[HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][DSSS_CCK-40]
#
# ar9271
#ht_capab=[HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][RX-STBC1][DSSS_CCK-40]
#
# mt7612u - mt7610u
#ht_capab=[HT40+][HT40-][GF][SHORT-GI-20][SHORT-GI-40]
#
# rtl8812au - rtl8811au -  rtl8812bu - rtl8811cu
#ht_capab=[HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][MAX-AMSDU-7935]
# rtl8814au
#ht_capab=[LDPC][HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][MAX-AMSDU-7935][DSSS_CCK-40]

# Event logger - as desired
#logger_syslog=-1
#logger_syslog_level=2
#logger_stdout=-1
#logger_stdout_level=2

# WMM - as desired
#uapsd_advertisement_enabled=1
#wmm_ac_bk_cwmin=4
#wmm_ac_bk_cwmax=10
#wmm_ac_bk_aifs=7
#wmm_ac_bk_txop_limit=0
#wmm_ac_bk_acm=0
#wmm_ac_be_aifs=3
#wmm_ac_be_cwmin=4
#wmm_ac_be_cwmax=10
#wmm_ac_be_txop_limit=0
#wmm_ac_be_acm=0
#wmm_ac_vi_aifs=2
#wmm_ac_vi_cwmin=3
#wmm_ac_vi_cwmax=4
#wmm_ac_vi_txop_limit=94
#wmm_ac_vi_acm=0
#wmm_ac_vo_aifs=2
#wmm_ac_vo_cwmin=2
#wmm_ac_vo_cwmax=3
#wmm_ac_vo_txop_limit=47
#wmm_ac_vo_acm=0

# TX queue parameters - as desired
#tx_queue_data3_aifs=7
#tx_queue_data3_cwmin=15
#tx_queue_data3_cwmax=1023
#tx_queue_data3_burst=0
#tx_queue_data2_aifs=3
#tx_queue_data2_cwmin=15
#tx_queue_data2_cwmax=63
#tx_queue_data2_burst=0
#tx_queue_data1_aifs=1
#tx_queue_data1_cwmin=7
#tx_queue_data1_cwmax=15
#tx_queue_data1_burst=3.0
#tx_queue_data0_aifs=1
#tx_queue_data0_cwmin=3
#tx_queue_data0_cwmax=7
#tx_queue_data0_burst=1.5

# End of hostapd-2g.conf
```
-----

Establish hostapd conf file and log file locations.

Note: Make sure to change <your_home> to your home directory.
```
sudo nano /etc/default/hostapd
```
Select one of the following options

Dual band option: Add to bottom of file
```
DAEMON_CONF="/etc/hostapd/hostapd-5g.conf /etc/hostapd/hostapd-2g.conf"
DAEMON_OPTS="-d -K -f /home/<your_home>/hostapd.log"
```
Single band option for 5g: Add to bottom of file
```
DAEMON_CONF="/etc/hostapd/hostapd-5g.conf"
DAEMON_OPTS="-d -K -f /home/<your_home>/hostapd.log"
```
Single band option for 2g: Add to bottom of file
```
DAEMON_CONF="/etc/hostapd/hostapd-2g.conf"
DAEMON_OPTS="-d -K -f /home/<your_home>/hostapd.log"
```
-----

Modify hostapd.service file.

Code:
```
sudo cp /usr/lib/systemd/system/hostapd.service /etc/systemd/system/hostapd.service
```
```
sudo nano /etc/systemd/system/hostapd.service
```
Select one of the following options

Dual band option: Change the 'Environment=' line and 'ExecStart=' line to the following
```
Environment=DAEMON_CONF="/etc/hostapd/hostapd-5g.conf /etc/hostapd/hostapd-2g.conf"
ExecStart=/usr/sbin/hostapd -B -P /run/hostapd.pid -B $DAEMON_OPTS $DAEMON_CONF
```
Single band option for 5g: Change the 'Environment=' line and 'ExecStart=' line to the following
```
Environment=DAEMON_CONF="/etc/hostapd/hostapd-5g.conf"
ExecStart=/usr/sbin/hostapd -B -P /run/hostapd.pid -B $DAEMON_OPTS $DAEMON_CONF
```
Single band option for 2g: Change the 'Environment=' line and 'ExecStart=' line to the following
```
Environment=DAEMON_CONF="/etc/hostapd/hostapd-2g.conf"
ExecStart=/usr/sbin/hostapd -B -P /run/hostapd.pid -B $DAEMON_OPTS $DAEMON_CONF
```
-----

Block the eth0, wlan0 and wlan1 interfaces from being processed, and let dhcpcd
configure only br0 via DHCP.
```
sudo nano /etc/dhcpcd.conf
```
Add the following line above the first `interface xxx` line, if any, for dual band setup
```
denyinterfaces eth0 <wlan0> <wlan1>
```
Add the following line above the first `interface xxx` line, if any, for single band setup
```
denyinterfaces eth0 <wlan0>
```
Go to the end of the file and add the following line
```
interface br0
```
-----

Enable systemd-networkd service. Website - [systemd-network](https://www.freedesktop.org/software/systemd/man/systemd.network.html)
```
sudo systemctl enable systemd-networkd
```
-----

Create bridge interface br0.
```
sudo nano /etc/systemd/network/10-bridge-br0-create.netdev
```
File contents
```
[NetDev]
Name=br0
Kind=bridge
```
-----

Bind ethernet interface.
```
sudo nano /etc/systemd/network/20-bridge-br0-bind-ethernet.network
```
File contents
```
[Match]
Name=eth0

[Network]
Bridge=br0
```
-----

Configure bridge interface.
```
sudo nano /etc/systemd/network/21-bridge-br0-config.network
```
Note: The contents of the Network block below should reflect the needs of your network.

File contents
```
[Match]
Name=br0

[Network]
Address=192.168.1.100/24
Gateway=192.168.1.1
DNS=8.8.8.8
```
-----

Ensure WiFi radio not blocked.
```
sudo rfkill unblock wlan
```
-----

Reboot system.
```
sudo reboot
```
-----

End of installation.

-----

-----

Notes: The following sections contain good to know information 

-----

Restart systemd-networkd service.
```
sudo systemctl restart systemd-networkd
```
-----

Check status of the services.
```
systemctl status hostapd
```
```
systemctl status systemd-networkd
```
-----

Install and autostart iperf3
```
sudo apt install iperf3
```
```
sudo nano /etc/systemd/system/iperf3.service
```
File contents
```
[Unit]
Description=iPerf3 Service
After=syslog.target network.target auditd.service

[Service]
Type=simple
ExecStart=/usr/bin/iperf3 -s

[Install]
WantedBy=multi-user.target
```
```
sudo systemctl enable iperf3
```
Check iperf3 status
```
sudo systemctl status iperf3
```

-----

Disable NetworkManager

Note: For systems not running the Gnome desktop, purging Network Manager
is the easiest solution.
```
sudo apt purge network-manager
```
Note: For systems running the Gnome desktop, use the following.
```
sudo systemctl stop NetworkManager.service
```
```
sudo systemctl disable NetworkManager.service
```
```
sudo systemctl stop NetworkManager-wait-online.service
```
```
sudo systemctl disable NetworkManager-wait-online.service
```
```
sudo systemctl stop NetworkManager-dispatcher.service
```
```
sudo systemctl disable NetworkManager-dispatcher.service
```
```
sudo systemctl stop network-manager.service
```
```
sudo systemctl disable network-manager.service
```
```
sudo reboot
```

-----

Disable Netplan

Note: Netplan is the default network manager on Ubuntu server.

Disable and mask networkd-dispatcher.

Note: we are activating /etc/network/interfaces
```
sudo apt-get install ifupdown
```
```
sudo systemctl stop networkd-dispatcher
```
```
sudo systemctl disable networkd-dispatcher
```
```
sudo systemctl mask networkd-dispatcher
```
Purge netplan.
```
sudo apt-get purge nplan netplan.io
```
```
sudo reboot
```
-----
