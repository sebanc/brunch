## Monitor Mode

Purpose: Provide information and tools for testing and using monitor mode.

Monitor mode, or RFMON (Radio Frequency MONitor) mode, allows a computer with a
wireless network interface controller (WNIC) to monitor all traffic received on
a wireless channel. Monitor mode allows packets to be captured without having to
associate with an access point or ad hoc network first. Monitor mode only
applies to wireless networks, while promiscuous mode can be used on both wired
and wireless networks. Monitor mode is one of the eight modes that 802.11
wireless cards and adapters can operate in: Master (acting as an access point),
Managed (client, also known as station), Ad hoc, Repeater, Mesh, Wi-Fi Direct,
TDLS and Monitor mode.

Note: This document and the start-mon.sh script have been tested on the following:
```
Kali Linux
Raspberry Pi OS
Linux Mint
Ubuntu
```
-----

## Steps to test monitor mode


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
sudo apt install aircrack-ng wireshark
```

-----
#### Check wifi interface information
```
iw dev
```

-----

#### Information

The wifi interface name ```wlan0``` is used in this document but you will need
to substitute the name of your wifi interface while using this document.

-----

#### Disable interfering processes

Option 1
```
sudo airmon-ng check kill
```

Option 2, another way that works for me on Linux Mint:

Note: I use multiple wifi adapters in my systems and I need to stay connected
to the internet while testing. This option works well for me and allows
me to stay connected by allowing Network Manager to continue managing interfaces
that are not marked as unmanaged.

Ensure Network Manager doesn't cause problems
```
sudo nano /etc/NetworkManager/NetworkManager.conf
```
add
```
[keyfile]
unmanaged-devices=interface-name:<wlan0>;interface-name:wlan0mon
```

Note: The above tells Network Manager to leave the specified interfaces alone.
Remember to replace ```<wlan0>``` with the name of the wifi interface that you
intend to use in monitor mode.

```
sudo reboot
```

-----

#### Change to monitor mode

Option 1

Note: This option may not work with some driver/adapter combinations. If not,
press on with ```start-mon.sh``` or option 2.
```
sudo airmon-ng start <wlan0>
```

Note: I have provided a script called ```start-mon.sh``` to automate most
of the following option. Please give it a try and make suggestions to improve it.
```
Usage: $ sudo ./start-mon.sh <wlan0>
```

Option 2

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

### airodump-ng can receive and interpret key strokes while running.
```

The following list describes the currently assigned keys and supported actions:


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

