## rtl8192eu-linux
Realtek rtl8192eu official Linux driver v5.2.19.1

This driver is based on the (latest) official Realtek v5.2.19.1 driver with fixes and improvements to support the latest kernels (up to 5.13).

### Before installing

Make sure you have headers, build, dkms and git packages installed.

Check:

```
sudo apt list linux-headers-generic build-essential dkms git
```
Install if necessary:
```
sudo apt -y install linux-headers-generic build-essential dkms git
```
### Automated install 

Run from driver directory:
```
./install_wifi.sh
```

### Manual install

Remove available drivers with (skip if `sudo lshw -C network` and `dkms status` do not show any wifi drivers):

```
sudo rmmod 8192eu
sudo rmmod rtl8xxxu
sudo dkms remove -m rtl8192eu -v 1.0
```

Blacklist default driver (rtl8xxxu on Ubuntu):

```
echo "blacklist rtl8xxxu" >> ./blacklist-rtl8xxxu.conf
sudo mv ./blacklist-rtl8xxxu.conf /etc/modprobe.d/
```

Run add and install commands from driver directory:

```
sudo cp -ar . /usr/src/rtl8192eu-1.0
sudo dkms add -m rtl8192eu -v 1.0
sudo dkms install -m rtl8192eu -v 1.0
```

Load driver (or reboot):
```
sudo modprobe 8192eu
```
