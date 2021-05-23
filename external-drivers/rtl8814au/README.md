# rtl8814au
Drivers for the rtl8814au chipset wireless adapters


# build & install
```
$ git clone https://github.com/aircrack-ng/rtl8814au.git
$ cd rtl8814au
$ make
$ make install
```

# TODO
```
* Fix injection capabilities again
* Fix USBModeSwitch function (and switch between USB2 and USB3/SuperSpeed)
```

# DKMS installation (normal)
```
$ make dkms_install
and to remove the dkms driver, type..
$ make dkms_remove
```

# ubuntu dkms package (require dpkg-dev, dkms)
```
$ apt install debhelper dpkg-dev dkms libelf-dev bc 
$ dpkg-buildpackage -b --no-sign
$ cd ..
$ dpkg -i rtl8814au-dkms_5.8.5.1-24835.20190115_all.deb
```


## UEFI Secure Boot - (boot the kernel with signed)
 if insmod the module it shows error of "Required key not available", you are using a kernel which is signed
 Only signed module can be use in this condition.

 ![sign needed error](pics/need-sign.png)

1. Create signing keys

```
    openssl req -new -x509 -newkey rsa:2048 -keyout MOK.priv -outform DER -out MOK.der -nodes -days 36500 -subj "/CN=Descriptive name/"
```
2. Sign the module

```
    sudo /usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 ./MOK.priv ./MOK.der $(modinfo -n rtl8812au)
```
3. Register the keys to Secure Boot

```
    sudo mokutil --import MOK.der
```
		Supply a password for later use after reboot

4. Reboot and follow instructions to Enroll MOK (Machine Owner Key).
   Here's a sample with pictures. The system will reboot one more time.
5. Confirm the key is enrolled

```
mokutil --test-key MOK.der
```



# USB2.0/3.0 mode switch

<pre>
 initial it will use USB2.0 mode which will limite 5G 11ac throughput (USB2.0 bandwidth only 480Mbps => throughput around 240Mbps)
when modprobe add following options will let it switch to USB3.0 mode at initial driver
options 8814au rtw_switch_usb_mode=1
</<pre>


## TODO: run time change usb2.0/3.0 mode
### usb2.0 => usb3.0
```
sudo sh -c "echo '1' > /sys/module/8814au/parameters/rtw_switch_usb_mode"
```
### usb3.0 => usb2.0
```
sudo sh -c "echo '2' > /sys/module/8814au/parameters/rtw_switch_usb_mode"
```



