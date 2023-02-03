# rtl8192eu linux drivers

**NOTE:** This branch is based on Realtek's driver versioned 4.4.1. `master` is based on 4.3.1.1 originally.

The official drivers for D-Link DWA-131 Rev E, with patches to keep it working on newer kernels.
Also works on Rosewill RNX-N180UBE v2 N300 Wireless Adapter and TP-Link TL-WN821N V6.

**NOTE:** This is just a "mirror". I have no knowledge about this code or how it works. Expect no support from me or any contributors here. I just think GitHub is a nicer way of keeping track of this than random forum posts and precompiled binaries being sent by email. I don't want someone else to have to spend 5 days of googling and compiling with random patches until it works.

## Source for the official drivers

Official drivers were downloaded from D-Link Australia. D-Link USA and the european countries I checked only lists revision A and B. Australia lists all three.

* [Download page for DWA-131][driver-downloads]
* [Direct download link for Linux drivers][direct-download]
  * GitHub will not link to the `ftp://` schema. Raw link contents:

      `ftp://files.dlink.com.au/products/DWA-131/REV_E/Drivers/DWA-131_Linux_driver_v4.3.1.1.zip`

In addition, you can find the contents of this version in the initial commit of this repo: [1387cf623d54bc2caec533e72ee18ef3b6a1db29][initial-commit]

## Patches

You can see the applied patches, their sources and/or motivation by looking at the commits. The `master` branch will mostly be kept clean with a single commit per patch, except for Pull Requests. You can review commit by commit and then record the SHA in order to get a safe reference to use. As long as the SHA stays the same you know that what you get has been reviewed by you.

Note that updates to this README will show up as separate commits. I will not mix changes to this file with changes to the code in case you want to mirror this without the README.

## Building and installing using DKMS

This tree supports Dynamic Kernel Module Support (DKMS), a system for
generating kernel modules from out-of-tree kernel sources. It can be used to
install/uninstall kernel modules, and the module will be automatically rebuilt
from source when the kernel is upgraded (for example using your package manager).

1. Install DKMS and other required tools

    * for normal Linux systems

    ```shell
    sudo apt-get install git linux-headers-generic build-essential dkms
    ```

    * for Raspberry Pi

    ```shell
    sudo apt-get install git raspberrypi-kernel-headers build-essential dkms
    ```

2. Clone this repository and change your directory to cloned path.

    ```shell
    git clone https://github.com/Mange/rtl8192eu-linux-driver
    ```
    ```shell
    cd rtl8192eu-linux-driver
    ```

3. The Makefile is preconfigured to handle most x86/PC versions. However, if you are compiling for something other than an intel x86 architecture, you need to first select the platform.

    * for the Raspberry Pi, you need to set the I386 to n and the ARM_RPI to y:

    ```sh
    ...
    CONFIG_PLATFORM_I386_PC = n
    ...
    CONFIG_PLATFORM_ARM_RPI = y
    ```

    * for arm64 devices (e.g. Orange Pi PC 2):

    ```sh
    ...
    CONFIG_PLATFORM_I386_PC = n
    ...
    CONFIG_PLATFORM_ARM_AARCH64 = y
    ```

4. Add the driver to DKMS. This will copy the source to a system directory so
that it can used to rebuild the module on kernel upgrades.

    ```shell
    sudo dkms add .
    ```

5. Build and install the driver.

    ```shell
    sudo dkms install rtl8192eu/1.0
    ```

6. Distributions based on Debian & Ubuntu have RTL8XXXU driver present & running in kernelspace. To use our RTL8192EU driver, we need to blacklist RTL8XXXU.

    ```shell
    echo "blacklist rtl8xxxu" | sudo tee /etc/modprobe.d/rtl8xxxu.conf
    ```

7. Force RTL8192EU Driver to be active from boot.
    ```shell
    echo -e "8192eu\n\nloop" | sudo tee /etc/modules
    ```

8. Newer versions of Ubuntu has weird plugging/replugging issue (Check #94). This includes weird idling issues, To fix this:

    ```shell
    echo "options 8192eu rtw_power_mgnt=0 rtw_enusbss=0" | sudo tee /etc/modprobe.d/8192eu.conf
    ```

9. Update changes to Grub & initramfs

    ```shell
    sudo update-grub; sudo update-initramfs -u
    ```

10. Reboot system to load new changes from newly generated initramfs.

    ```shell
    systemctl reboot -i
    ```

11. Check that your kernel has loaded the right module:
 
    ```shell
    sudo lshw -c network
    ```
   
You should see the line ```driver=8192eu```
    
If you wish to uninstall the driver at a later point, use
_sudo dkms uninstall rtl8192eu/1.0_. To completely remove the driver from DKMS use
_sudo dkms remove rtl8192eu/1.0 --all_.

## Using as AP

Reference: Intelbras IWA 3001 USB WiFi Adapter  
Devices using the 8192eu chip can serve as decent access points, with speeds up to ~50Mbps.  
 
Using hostapd to manage your AP, set the proper ht-capab field for this device, which is:  

`HT_CAPAB=[RX-STBC1][SHORT-GI-40][SHORT-GI-20][DSSS_CCK-40][MAX-AMSDU-7935]`

Optionally enable wideband, if you don't have neighbours:  
Note that while this will result in a increase in network throughput it may cause clients further away to fail connecting.  
It may also make the device work better with repeaters repeating its signal.  

`HT_CAPAB=[HT40+][RX-STBC1][SHORT-GI-40][SHORT-GI-20][DSSS_CCK-40][MAX-AMSDU-7935]` (for channels 1-7), or  
`HT_CAPAB=[HT40-][RX-STBC1][SHORT-GI-40][SHORT-GI-20][DSSS_CCK-40][MAX-AMSDU-7935]` (for channels 5-13)

## Changing transmit power

Currently there is no way to change transmit power in the driver with iw or iwconfig tools, as you would with other wireless devices.  
The values returned by these tools are purely fictional on this driver.
However, you can still manually change the transmit power at compile time
by editing the file `hal/rl8192e/rtl8192e_phycfg.c` and changing the lines below:

```
/* Manual Transmit Power Control 
   The following options take values from 0 to 63, where:
   0 - disable
   1 - lowest transmit power the device can do
   63 - highest transmit power the device can do
   Note that these options may override your country's regulations about transmit power.
   Setting the device to work at higher transmit powers most of the time may cause premature 
   failure or damage by overheating. Make sure the device has enough airflow before you increase this.
   It is currently unknown what these values translate to in dBm.
*/


// Transmit Power Boost
// This value is added to the device's calculation of transmit power index.
// Useful if you want to keep power usage low while still boosting/decreasing transmit power.
// Can take a negative value as well to reduce power.
// Zero disables it. Default: 2, for a tiny boost.
int transmit_power_boost = 2;
// (ADVANCED) To know what transmit powers this device decides to use dynamically, see:
// https://github.com/lwfinger/rtl8192ee/blob/42ad92dcc71cb15a62f8c39e50debe3a28566b5f/hal/phydm/rtl8192e/halhwimg8192e_rf.c#L1310


// Transmit Power Override
// This value completely overrides the driver's calculations and uses only one value for all transmissions.
// Zero disables it. Default: 0
int transmit_power_override = 0;


/* Manual Transmit Power Control */
```

## Submitting patches

1. Fork repo
2. Do your patch in a topic branch
3. Open a pull request on GH, or send it by email to `Magnus Bergmark <magnus.bergmark@gmail.com>`.
4. I'll squash your commits when everything checks out and add it to `master`.

## Copyright and licenses

The original code is copyrighted, but I don't know by whom. The driver download does not contain license information; please open an issue if you are the copyright holder.

Most C files are licensed under GNU General Public License (GPL), version 2.

[driver-downloads]: http://support.dlink.com.au/Download/download.aspx?product=DWA-131
[direct-download]: ftp://files.dlink.com.au/products/DWA-131/REV_E/Drivers/DWA-131_Linux_driver_v4.3.1.1.zip
[initial-commit]: https://github.com/Mange/rtl8192eu-linux-driver/commit/1387cf623d54bc2caec533e72ee18ef3b6a1db29
