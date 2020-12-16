Work in progress driver for the touchbar and ambient-light-sensor on 2019 MacBook Pro's.

Building and Installing:
------------------------
```
git clone --branch mbp15 https://github.com/roadrunner2/macbook12-spi-driver.git
cd macbook12-spi-driver
make
sudo modprobe industrialio_triggered_buffer
sudo insmod apple-ibridge.ko
sudo insmod apple-ib-tb.ko
sudo insmod apple-ib-als.ko
```

Alternatively, use dkms:

DKMS module:
------------
As root, do the following (use `dnf` instead of `apt` if on Fedora or similar):
```
apt install dkms
git clone --branch mbp15 https://github.com/roadrunner2/macbook12-spi-driver.git /usr/src/apple-ibridge-0.1
dkms install -m apple-ibridge -v 0.1
modprobe apple-ib-tb
modprobe apple-ib-als
```

Touchbar/ALS/iBridge:
---------------------
The touchbar and ambient-light-sensor (ALS) are part of the iBridge (T2) chip, and hence there are 3 modules corresponding to these (`apple_ibridge`, `apple_ib_tb`, and `apple_ib_als`). Generally loading any one of these will load the others, unless you are loading them via `insmod`. If loading manually (i.e. via `insmod`), you need to first load the `industrialio_triggered_buffer` and `apple_ibridge` modules.

The touchbar driver provides basic touchbar functionality (enabling the touchbar and switching between modes based on the FN key). The touchbar is automatically dimmed and later switched off if no (internal) keyboard, touchpad, or touchbar input is received for a period of time; any (internal) keyboard, touchpad, or touchbar input switches it back on. The timeouts till the touchbar is dimmed and turned off can be changed via the `idle_timeout` and `dim_timeout` module params or sysfs attributes (`/sys/class/input/input9/device/...`); they default to 5 min and 4.5 min, respectively. See also `modinfo apple_ib_tb`.

The ALS driver exposes the ambient light sensor; if you have the `iio-sensor-proxy` installed then it should be recognized and handled automatically.

