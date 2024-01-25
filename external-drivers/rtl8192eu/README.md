## rtl8192eu-linux
Realtek rtl8192eu official Linux driver v5.11.2.1

This driver is based on the (latest) official and manufacturer supported Realtek v5.11.2.1 driver with fixes and improvements to support the latest kernels (up to 6.7).

### Before installing

If you're using a different architecture than x86, please set (MakeFile) ```CONFIG_PLATFORM_I386_PC = n``` and also set your architecture (for example ```CONFIG_PLATFORM_ARM_RPI = y``` for 32-bit ARM, or ```CONFIG_PLATFORM_ARM_AARCH64 = y``` for 64-bit ARM).

Monitor mode can be enabled by setting ```CONFIG_WIFI_MONITOR = y```.

Also, make sure you have headers, build, dkms and git packages installed. 

##### Debian, Ubuntu, Mint:

```sudo apt install linux-headers-generic build-essential dkms git```

##### Fedora:

```sudo dnf groupinstall "C Development Tools and Libraries"  & sudo dnf install dkms git```

##### RHEL, CentOS:

```sudo yum groupinstall "Development Tools" && sudo yum install dkms git```

##### Arch Linux, Manjaro:

```sudo pacman -S base-devel dkms git```

##### openSUSE:

```sudo zypper install -t pattern devel_C_C++ && sudo zypper install dkms git```

### Automated (re)install 

Run from driver directory:
```
./install_wifi.sh
```

### Automated uninstall/reset

Run from driver directory:
```
./uninstall_wifi.sh
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

### After installing (secure boot)

If secure boot (deployed mode) is enabled, you must enroll the public key mentioned during the install process once. This may require installing additional packages like mokutil keyutils and openssl. You can temporarily switch to audit mode (bios) and use the driver normally.

To enroll use:
```
sudo mokutil --import [public_key]
```
After reboot, you can enroll the key.
