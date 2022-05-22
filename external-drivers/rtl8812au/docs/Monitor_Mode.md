-----

2021-12-18

## Monitor Mode

Purpose: Provide information and tools for testing and using monitor
mode with the following Realtek drivers:

```
https://github.com/morrownr/8812au-20210629
https://github.com/morrownr/8821au-20210708
https://github.com/morrownr/88x2bu-20210702
```

For adapters that use in-kernel drivers, use any of the many guides that
are available as the in-kernel drivers work in the textbook, standards
compliant manner.

Please submit corrections or additions via Issues.

Monitor mode, or RFMON (Radio Frequency MONitor) mode, allows a computer
with a wireless network interface controller (WNIC) to monitor all
traffic received on a wireless channel. Monitor mode allows packets to
be captured without having to associate with an access point or ad hoc
network first. Monitor mode only applies to wireless networks, while
promiscuous mode can be used on both wired and wireless networks.
Monitor mode is one of the eight modes that 802.11 wireless cards and
adapters can operate in: Master (acting as an access point), Managed
(client, also known as station), Ad hoc, Repeater, Mesh, Wi-Fi Direct,
TDLS and Monitor mode.

Note: This document and the `test-mon.sh` script have been tested on the
following:

```
Kali Linux
Raspberry Pi OS
Linux Mint
Ubuntu
```
-----

## Steps to test monitor mode

#### Install USB WiFi adapter and driver per instructions.


#### Update system
```
sudo apt update
```
```
sudo apt full-upgrade
```

-----

#### Ensure WiFi radio is not blocked
```
sudo rfkill unblock wlan
```

-----

#### Install the aircrack-ng and wireshark packages
```
sudo apt install -y aircrack-ng wireshark
```

-----

#### Check wifi interface information
```
iw dev
```

-----

#### Information

The wifi interface name `wlan0` is used in this document but you will
need to substitute the name of your wifi interface while using this
document.

-----

#### Enter and check monitor mode

A script called `test-mon.sh` is available in the driver directory.
It will automate much of the following. It is a work in progress so 
please feel free to make and submit improvements. It is written in Bash.

Usage:

```
sudo ./test-mon.sh [interface:wlan0]
```

Note: If you want to do things manually, continue below.

-----

#### Disable interfering processes

```
sudo airmon-ng check kill
```

#### Change to monitor mode 

Option 1 (the airmon-ng way)

Note: This option may not work with some driver/adapter combinations
(I'm looking at you Realtek). If this option does not work, you can
use Option 2 or the `test-mon.sh` script that was previously mentioned.
```
sudo airmon-ng start <wlan0>
```

Option 2 (the manual way)

Check the wifi interface name and mode
```
iw dev
```

Take the interface down
```
sudo ip link set <wlan0> down
```

Set monitor mode
```
sudo iw <wlan0> set monitor control
```

Bring the interface up
```
sudo ip link set <wlan0> up
```

Verify the mode has changed
```
iw dev
```

-----

### Test injection

Option for 5 GHz and 2.4 GHz
```
sudo airodump-ng <wlan0> --band ag
```
Option for 5 GHz only
```
sudo airodump-ng <wlan0> --band a
```
Option for 2.4 GHz only
```
sudo airodump-ng <wlan0> --band g
```
Set the channel of your choice
```
sudo iw dev <wlan0> set channel <channel> [NOHT|HT20]
```
```
sudo aireplay-ng --test <wlan0>
```

-----

### Test deauth

Option for 5 GHz and 2.4 GHz
```
sudo airodump-ng <wlan0> --band ag
```
Option for 5 GHz only
```
sudo airodump-ng <wlan0> --band a
```
Option for 2.4 GHz only
```
sudo airodump-ng <wlan0> --band g
```
```
sudo airodump-ng <wlan0> --bssid <routerMAC> --channel <channel of router>
```
Option for 5 GHz:
```
sudo aireplay-ng --deauth 0 -c <deviceMAC> -a <routerMAC> <wlan0> -D
```
Option for 2.4 GHz:
```
sudo aireplay-ng --deauth 0 -c <deviceMAC> -a <routerMAC> <wlan0>
```

-----

### Revert to Managed Mode

Check the wifi interface name and mode
```
iw dev
```

Take the wifi interface down
```
sudo ip link set <wlan0> down
```

Set managed mode
```
sudo iw <wlan0> set type managed
```

Bring the wifi interface up
```
sudo ip link set <wlan0> up
```

Verify the wifi interface name and mode has changed
```
iw dev
```

-----

### Change the MAC Address before entering Monitor Mode

Check the wifi interface name, MAC address and mode
```
iw dev
```

Take the wifi interface down
```
sudo ip link set dev <wlan0> down
```

Change the MAC address
```
sudo ip link set dev <wlan0> address <new mac address>
```

Set monitor mode
```
sudo iw <wlan0> set monitor control
```

Bring the wifi interface up
```
sudo ip link set dev <wlan0> up
```

Verify the wifi interface name, MAC address and mode has changed
```
iw dev
```

-----

### Change txpower
```
sudo iw dev <wlan0> set txpower fixed 1600
```

Note:  1600 = 16 dBm

-----

### Information

airodump-ng can receive and interpret key strokes while running.

```

The following list describes the currently assigned keys and supported
actions:


a

Select active areas by cycling through these display options:
 AP+STA; AP+STA+ACK; AP only; STA only


d

Reset sorting to defaults (Power)


i

Invert sorting algorithm


m

Mark the selected AP or cycle through different colors if the selected AP is
already marked


o

Enable colored display of APs and their stations.


p

Disable colored display.


q

Quit program.


r

(De-)Activate realtime sorting -
 applies sorting algorithm every time the display will be redrawn


s

Change column to sort by, which currently includes:

 BSSID;
 PWR level;
 Beacons;
 Data packets;
 Packet rate;
 Channel;
 Max. data rate;
 Encryption;
 Strongest Ciphersuite;
 Strongest Authentication;
 ESSID
```

-----

