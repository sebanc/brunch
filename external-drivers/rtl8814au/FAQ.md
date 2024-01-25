### FAQ

Secure Boot Information

Question: The driver installation script completed successfully and the
driver is installed but does not seem to be working. What is wrong?

Answer: This question often comes up after installing the driver to a
system that has Secure Boot on. To test if there is a Secure Boot related
problem, turn secure boot off in the system BIOS and reboot.  If the driver
works as expected after reboot, then the problem is likely related to
Secure Boot.

What will increase my chances of having a sucessessful installation on a
system that has Secure Boot on?

First and foremost, make sure Secure Boot is on when you initially install
your Linux distro. If your Linux distro was installed with Secure Boot off,
the easiest solution is likely to do a clean reinstallation with Secure Boot
on.

Ubuntu is used as the example but other distros should be similar to one
degree or another. During the installation there may be a box on one of
installation pages that will appear if the installation program detects
that Secure Boot is on. You will need to check the appropriate box and
supply a password. You can use the same password that you use for the system
if you wish. After the installation and reboot completes, the first screen
you should see is the mokutil screen. Mokutil will guide you through the
process of setting up your system to support Secure Boot. If you are unsure
what to do, I recommend you seek guidance from your distro documentation or
user forums. Having Secure Boot properly set up in your installation is very
important.

The `install-driver.sh` script currently supports Secure Boot if `dkms`
is installed. Here is a link to the `dkms` website. There is information
regarding Secure Boot in two sections in the `README`.

https://github.com/dell/dkms

Here is a link regarding Debian and Secure Boot:

https://wiki.debian.org/SecureBoot

There is work underway to add Secure Boot support for systems that do not
have `dkms` available or if a manual installation is desired.

If you are using a basic command line (non-dkms) installation, see the
following section in the Installation Steps part of the README:

If you use the `install-driver.sh` script and see the following message

`SecureBoot enabled - read FAQ about SecureBoot`

You need to read the following:

The MOK managerment screen will appear during boot:

`Shim UEFI Key Management"

`Press any key...`

Select "Enroll key"

Select "Continue"

Select "Yes"

When promted, enter the password you entered earlier.

If you enter the wrong password, your computer will not be bootable. In
this case, use the BOOT menu from your BIOS to boot then as follows:

```
sudo mokutil --reset
```

Restart your computer and use the BOOT menu from BIOS to boot. In the MOK
managerment screen, select `reset MOK list`. Then Reboot and retry the
driver installation.

Manual Installation Instructions

It provides secure boot instructions.

-----

Question: Is WPA3 supported?

Answer: No

-----

Question: I bought two usb wifi adapters based on this chipset and am
planning to use both in the same computer. How do I set that up?

Answer: Realtek drivers do not support more than one adapter with the
same chipset in the same computer. You can have multiple Realtek based
adapters in the same computer as long as the adapters are based on
different chipsets.

Recommendation: If this is an important capability for you, I have tested
Mediatek adapters for this capability and it does work with adapters that
use the following chipsets: mt7921au, mt7612u and mt7610u.

-----

Question: Why do you recommend Mediatek based adapters when you maintain
this repo for a Realtek driver?

Answer: Many new and existing Linux users already have adapters based on
Realtek chipsets. This repo is for Linux users to support their existing
adapters but my STRONG recommendation is for Linux users to seek out USB
WiFi solutions based on Mediatek chipsets. Mediatek is making and
supporting their drivers per Linux Wireless Standards guidance per the
Linux Foundation. This results in far fewer compatibility and support
problems. More information and recommended adapters shown at the
following site:

https://github.com/morrownr/USB-WiFi

-----

Question: Will you put volunteers to work?

Answer: Yes. Post a message in `Issues` or `Discussions` if interested.

-----

Question: I am having problems with my adapter and I use Virtualbox?

Answer: This [article](https://null-byte.wonderhowto.com/forum/wifi-hacking-attach-usb-wireless-adapter-with-virtual-box-0324433/) may help.

-----

Question: Can you provide additional information about monitor mode?

Answer: I have a repo that is setup to help with monitor mode:

https://github.com/morrownr/Monitor_Mode

Work to improve monitor mode is ongoing with this driver. Your reports of
success or failure are needed. If you have yet to buy an adapter to use with
monitor mode, there are adapters available that are known to work very well
with monitor mode. My recommendation for those looking to buy an adapter for
monitor mode is to buy adapters based on the following chipsets: mt7921au,
mt7612u, mt7610u, rtl8821cu, and rtl8812bu. My specific recommendations for
adapters in order of preference currently are:

ALFA AWUS036ACHM - long range - in-kernel driver

ALFA AWUS036ACM - in-kernel driver

ALFA AWUS036ACU - in-kernel driver (as of kernel 6.2) and [out-of-kernel driver](https://github.com/morrownr/8821cu)

To ask questions, go to [USB-WiFi](https://github.com/morrownr/USB-WiFi)
and post in `Discussions` or `Issues`.

-----

Question: How do I forget a saved WiFi network on a Raspberry Pi?

Note: This answer is for the Raspberry Pi OS without Network Manager active.

Step 1: Edit `wpa_supplicant.conf`

```
sudo ${EDITOR} /etc/wpa_supplicant/wpa_supplicant.conf
```

Note: Replace ${EDITOR} with the name of the text editor you wish to use.

#### Step 2: Delete the relevant WiFi network block (including the '`network=`' and opening/closing braces).

#### Step 3: Save the file.

#### Step 4: Reboot

-----

Question: How do I disable the onboard WiFi in a Raspberry Pi?

Note: This answer is for the Raspberry Pi OS.

Answer:

Add the following line to `/boot/config.txt`

```
dtoverlay=disable-wifi
```

-----
