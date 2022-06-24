2022-02-14

How to Upgrade wpa_supplicant

Tested on: Linux Mint 20.2 - kernel 5.13

Info: Some distros may require additional updates beyond wpa_supplicant
to support WPA3 in managed (client) mode. Hopefully, as time passes,
making modifications will no longer be necessary for the Realtek
drivers to fully support WPA3. No modifications, such as this, are
necessary at this time for AP mode with the Raspberry Pi OS.

Note: This document applies only to the following drivers:

```
https://github.com/morrownr/8821au-20210708
https://github.com/morrownr/8812au-20210629
https://github.com/morrownr/88x2bu-20210702
https://github.com/morrownr/8821cu-20210118
```

Purpose: Provide instructions for upgrading wpa_supplicant. As of the
date of this document, the specified Realtek USB WiFi adapter drivers
cannot support WPA3 without a very new version of wpa_supplicant
installed on most Linux distros. The version of wpa_supplicant required
currently is not even released yet. It is the git main.

This guide and its effectiveness has had limited testing but has been
requested by users. It is now up to you, should you decide this is what
want to do, to report success or failure so that we can improve this
guide.

Disclaimer: This guide attempts to help you install software that has
not been released. This may break your system. If it does break your
system, you own it.


Step 1) Install dependencies required by wpa_supplicant.

```
$ sudo apt install -y libssl-dev build-essential pkg-config libnl-3-dev

$ sudo apt install -y libdbus-1-dev libdbus-glib-1-2 libdbus-glib-1-dev

$ sudo apt install -y libreadline-dev libncurses5-dev libnl-genl-3-dev

$ sudo apt install -y dbus libnl-route-3-dev 
```

Step 2) Go to the `src` directory.

```
$ cd ~/src
```

The `~/src` directory has already been created if you followed the
instructions for installing any of the wifi drivers listed above.


3) Download the wpa_supplicant source package.

```
$ git clone git://w1.fi/hostap.git
```

4) Go in to wpa_supplicant folder.

```
$ cd ~/src/hostap/wpa_supplicant
```

If an error is returned, check whether the directory name is correct.


5) Create a build configuration file.

```
$ sudo nano .config
```

Add the following contents into .config and save ( Ctrl + X, Y, Enter ) 

```
# 2021-11.29
# .config for wpa_supplicant
# Required for WPA3
CONFIG_TLS=openssl
CONFIG_IEEE80211W=y
CONFIG_SAE=y
CONFIG_LIBNL20=y
CONFIG_LIBNL32=y
#
# Defaults per defconfig
CONFIG_DRIVER_WEXT=y
CONFIG_DRIVER_NL80211=y
CONFIG_DRIVER_WIRED=y
CONFIG_DRIVER_MACSEC_LINUX=y
CONFIG_IEEE8021X_EAPOL=y
CONFIG_EAP_MD5=y
CONFIG_EAP_MSCHAPV2=y
CONFIG_EAP_TLS=y
CONFIG_EAP_PEAP=y
CONFIG_EAP_TTLS=y
CONFIG_EAP_FAST=y
CONFIG_EAP_GTC=y
CONFIG_EAP_OTP=y
CONFIG_EAP_PWD=y
CONFIG_EAP_PAX=y
CONFIG_EAP_LEAP=y
CONFIG_EAP_SAKE=y
CONFIG_EAP_GPSK=y
CONFIG_EAP_GPSK_SHA256=y
CONFIG_EAP_TNC=y
CONFIG_WPS=y
CONFIG_EAP_IKEV2=y
CONFIG_MACSEC=y
CONFIG_PKCS12=y
CONFIG_SMARTCARD=y
CONFIG_BACKEND=file
CONFIG_CTRL_IFACE=y
CONFIG_CTRL_IFACE_DBUS_NEW=y
CONFIG_CTRL_IFACE_DBUS_INTRO=y
CONFIG_IEEE80211R=y
CONFIG_DEBUG_FILE=y
CONFIG_DEBUG_SYSLOG=y
CONFIG_IEEE80211AC=y
CONFIG_INTERWORKING=y
CONFIG_HS20=y
CONFIG_AP=y
CONFIG_P2P=y
CONFIG_TDLS=y
CONFIG_WIFI_DISPLAY=y
CONFIG_DPP=y
#
# Not defaults per defconfig
CONFIG_READLINE=y
CONFIG_DEBUG_SYSLOG_FACILITY=LOG_DAEMON
#
# Extra
CFLAGS += -I/usr/include/libnl3
#
# End of .config
```

6) Compile

```
$ make
```

7) Install

```
$ sudo make install
```

8) Reboot

```
$ sudo reboot 
```

9) Check version

```
$ wpa_supplicant -v
```

-----
