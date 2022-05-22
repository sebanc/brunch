2021-12-03

What is Concurrent Mode?

Concurrent Mode creates 2 wireless network interfaces (wlan0, wlan1) and those
two interfaces share the same WiFi adapter.

This feature allows performing 2 separate wireless tasks at the same time with a
single WiFi adapter.

For example:

Use station mode (called managed or client mode also) to connect with an
AP to access the internet at the same time as it also performs as an AP
to allow other devices to connect to the second interface.

Note: Only supports 3 combinations

1. Station mode + Station mode
2. Station mode + AP mode
3. Station mode + P2P mode

-----

How do I Enable Concurrent Mode?

Run the following as instructed during the installation process:

```
./cmode-on.sh
```

Once the driver is fully installed and you have rebooted the system, you
can verify that this works by typing the “iw dev” command, You should
see two wireless interfaces, and the MAC address of secondary interface
is nearly the same as the first except for one digit.

-----

FAQ:

Q: Which wireless interface can run in station mode? Which
wireless interface can run in AP mode?

A: It is recommended to run station mode in wlan0 and AP or P2P mode in
wlan1.

Q: How is the throughput with 2 wireless interfaces in concurrent mode?

A: Because there is only one physical hardware device, the two wireless
interfaces (wlan0, wlan1) will share the transmit bandwidth.

For example:

Assume the throughput limitation of current environment is 85Mb/s,
then the throughput of wlan0 + the throughput of wlan1 is basically
equal or smaller than 85Mb/s.

Q: Everything is fine when I only start hostapd, but when I start running
station mode in the other interface at the same time, hostapd will disconnect
for a moment then will reconnect again, however ,the channel is differ from
before. Is something wrong?

A: Don’t worry, it is fine! As mentioned before, those two wireless interfaces
share the same physical hardware device. That means those 2 wireless interfaces
must use the same channel. The AP/P2P interface should use the same channel as
the station interface. If both interfaces are running in station mode, the
connected APs MUST be on same channel.
