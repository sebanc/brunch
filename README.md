# Brunch framework

## Overview

First of all, thanks goes to project Croissant, the swtpm maintainer and the Chromebrew framework for their work which was actively used when creating this project.

The Brunch framework purpose is to create a generic x86_64 ChromeOS image from an official recovery image. To do so, it uses a 1GB ROOTC partition (containing a custom kernel, an initramfs, the swtpm binaries, userspace patches and config files) and a specific EFI partition to boot from it.

**Warning: with this setup, ChromeOS is not running in a virtual machine and has therefore direct access to all your devices. As such, many bad things can happen with your device and mostly your data. Make sure you only use this framework on a device which does not contain any sensitive data and keep non-sensitive data synced with a cloud service. I cannot be held responsible for anything bad that would happen to your device, including data loss.**

## Chromeos recovery image

2 types of ChromeOS recovery images exist and use different device configuration mechanisms:
- non-unibuild images: configured for single device configurations like eve (Google Pixelbook) and nocturne (Google Pixel Slate) for example.
- unibuild images: intended to manage multiple devices through the use of the CrosConfig tool.

Contrarily to the Croissant framework which mostly supports non-unibuilds images (configuration and access to android apps), Brunch should work with both but will provide better hardware support for unibuild images.
It is actually tested using the "octopus" ChromeOS recovery image (unibuild) which is therefore recommended.

Note: I don't see any reason why you would have better results using another recovery image as they are all very similar.

ChromeOS recovery images can be downloaded from here: https://cros-updates-serving.appspot.com/

## Hardware support

Hardware support is highly dependent on the general Linux kernel hardware compatibility. As such only Linux supported hardware will work and the same specific kernel command line options recommended for your device should be passed through the GRUB bootloader (see "Modify the GRUB bootloader" section).

Base hardware compatibility:
- x86_64 computers with Intel hardware (cpu and gpu),
- AMD support is untested and probably not working (to be confirmed),
- Nvidia graphic cards are also not supported.

Specific hardware support:
- sensors: HID based accelerometer and light sensors are currently supported through a custom kernel module,
- cameras: an experimental patch aims to allow uvcvideo compatible cameras support (testers needed),
- Microsoft Surface devices: dedicated kernel patches are included.

# Install instructions

You can install ChromeOS on a USB flash drive / SD card (16GB minimum) or as an image on your hard disk for dual booting (14GB of free space needed).

## Install ChromeOS from Linux (the easiest way)

### Requirements

- root access.
- `tar` and `cgpt` packages/binaries.

### Install ChromeOS on a USB flash drive / SD card

1. Download a ChromeOS recovery image (octopus recommended) and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery image version you have downloaded (from the GitHub release section).
3. Open a terminal, navigate to the directory containing the package.
4. Extract it: 
```
tar zxvf brunch_< version >.tar.gz
```
5. Identify your USB flash drive / SD card device name e.g. /dev/sdX (Be careful here as the installer will erase all data on the target drive)
6. Install ChromeOS on the USB flash drive / SD card:
```
sudo bash chromeos-install.sh -src < path to the ChromeOS recovery image > -dst < your USB flash drive / SD card device. e.g. /dev/sdX >
```
7. Reboot your computer and boot from the USB flash drive / SD card (refer to your computer manufacturer's online resources).
8. (Secure Boot only) A blue screen saying "Verfification failed: (15) Access Denied" will appear upon boot and you will have to enroll the secure boot key by selecting "OK->Enroll key from disk->EFI-SYSTEM->brunch.der->Continue". Reboot your computer and boot again from the USB flash drive / SD card.

The GRUB menu should appear, select ChromeOS and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen. You can now start using ChromeOS.

### Dual Boot ChromeOS from your HDD

ChromeOS partition scheme is very specific which makes it difficult to dual boot. One solution to circumvent that is to keep ChromeOS in a disk image on the hard drive and run it from there.

Make sure you have an ext4 or NTFS partition with at least 14gb of free space available and no encryption or create one (refer to online resources).

1. Perform the steps 1 to 4 as described in the previous section (Install ChromeOS on a USB flash drive / SD card).
2. Create the ChromeOS image on your unencrypted ext4 or NTFS partition.
```
sudo bash chromeos-install.sh -src < path to the ChromeOS recovery image > -dst < path to the ChromeOS image on the chosen partition > -s < ChromeOS image size in GB >
```
3. Copy the GRUB configuration which appears in the terminal at the end of the process (between lines with stars) to either:
- your hard disk GRUB install if you have one (refer to you distro's online resources).
- the USB flash drive / SD card GRUB config file (then boot from USB flash drive / SD card and choose "boot from disk image" in the GRUB menu),
4. (secure boot only) Download the secure boot key "brunch.der" in this branch (master) of the repository and enroll it by running the command:
```
sudo mokutil --import brunch.der
```
5. Reboot your computer and boot to the bootloader with the modified GRUB config.

The GRUB menu should appear, select "ChromeOS (boot from disk image)" and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen. You can now start using ChromeOS from your HDD.

## Install ChromeOS from Windows

### Install ChromeOS on a USB flash drive / SD card

1. Download a recovery image (octopus recommended) and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery version you have downloaded (from the GitHub release section).
3. Install the Ubuntu WSL from the Microsoft store (refer to online resources).
4. Launch Ubuntu WSL and install tar and cgpt packages:
```
sudo apt update && sudo apt install tar cgpt
```
5. Browse to your Downloads folder using `cd`:
```
cd /mnt/c/Users/< username >/Downloads/
```
6. Extract the package:
```
sudo tar zxvf brunch_< version >.tar.gz
```
7. Make sure you have at least 14gb of free space available
8. Create a ChromeOS image:
```
sudo bash chromeos-install.sh -src < path to the ChromeOS recovery image > -dst chromeos.img
```
9. Use "Rufus" (https://rufus.ie/) to write the chromeos.img to the USB flash drive / SD card.
10. Reboot your computer and boot from the USB flash drive / SD card (refer to your computer manufacturer's online resources).
11. (Secure Boot only) A blue screen saying "Verfification failed: (15) Access Denied" will appear upon boot and you will have to enroll the secure boot key by selecting "OK->Enroll key from disk->EFI-SYSTEM->brunch.der->Continue". Reboot your computer and boot again from the USB flash drive / SD card.
12. The GRUB menu should appear, select ChromeOS and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen.
At this stage, your USB flash drive / SD card is incorrectly recognized as 14GB regardless of its actual capacity. To fix this:
13. At the ChromeOS startup screen, press CTRL+ALT+F2 to go into a shell session.
14. Login as `root`
15. Execute the below command:
```
sudo bash resize-data
```
16. Reboot your computer when requested and boot again from USB flash drive / SD card. You can now start using ChromeOS.

### Dual Boot ChromeOS from your HDD

1. Make sure you have a NTFS partition with at least 14gb of free space available and no BitLocker encryption or create one (refer to online resources).
2. Create a ChromeOS USB flash drive / SD card using the above method (Install ChromeOS on a USB flash drive / SD card) and boot it.
3. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
4. Identify the destination NTFS partition within /media/removable/XXXXXXX folders.
5. Create the disk image:
```
sudo bash chromeos-install -dst < path of the ChromeOS image on the NTFS partition > -s < ChromeOS image size in GB >
```
6. Use vi to copy the GRUB configuration which is displayed in the terminal at the end of the process (between lines with stars) to the USB flash drive / SD card GRUB config file.
7. Reboot your computer and boot from USB flash drive / SD card.

The GRUB menu should appear, select "ChromeOS (boot from disk image)" and you should be greeted by ChromeOS startup screen. You can now start using ChromeOS from your HDD.

# Optional steps

## Framework options

Some options can be passed through the kernel command lines to activate specific features which might be dangerous or not work from everyone:
- advanced_als: default ChromeOS auto-brightness is very basic (https://chromium.googlesource.com/chromiumos/platform2/+/master/power_manager/docs/screen_brightness.md). This option activates more auto-brightness levels (based on the Google Pixel Slate implementation),
- disable_intel_hda: some Chromebooks need to blacklist the snd_hda_intel module, use this option to reproduce it.
- disable_touchpad_fix: disable the patch which aims at improving touchpad sensitivity and fixing tap to click (if you encounter touchpad issues).
- enable_updates: allow native ChromeOS updates (use at your own risk: ChromeOS will be updated but not the Brunch framework/kernel which might render your ChromeOS install unstable or even unbootable),

Add "options=option1,option2,..." (without spaces) to the kernel command line to activate them.

For example: booting with "options=enable_updates,advanced_als" will activate both options.

## Update both ChromeOS and the Brunch framework

It is currently recommended to only update ChromeOS when the matching version of the Brunch framework has been released.

1. Download the new ChromeOS recovery image version and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery version (from the GitHub release section).
3. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
4. Update the framework:
```
sudo bash chromeos-update -r < path to the ChromeOS recovery image > -f < path to the Brunch release archive >
```
5. Restart ChromeOS

## Update only the Brunch framework (if you have enabled native ChromeOS updates)

If you chose to use the "enable_updates" option and have updated to a new ChromeOS release, you might want to update the brunch framework to match your current ChromeOS version.

1. Download the Brunch release corresponding to your ChromeOS version (from the GitHub release section).
2. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
3. Update the framework:
```
sudo bash chromeos-update -f < path to the Brunch release archive >
```
4. Restart ChromeOS

## Modify the GRUB bootloader

### From Windows

1. Install notepad++ (https://notepad-plus-plus.org/)
2. Look for the EFI partition in the Explorer and browse to the efi/boot folder.
3. Edit the grub.cfg file with notepad++ (warning: editing this file with standard Notepad or Wordpad will render the file unusable and prevent GRUB from booting due to formatting issues)
4. Add your specific kernel parameters at the end of the Linux line arguments.

### From Linux

1. Create a directory to mount the EFI partition:
```
mkdir /tmp/efi_part
```
2. Mount the partition 12 of your device to your EFI partition:
```
sudo mount /dev/< partition 12 of ChromeOS device > /tmp/efi_part
```
3. Edit the file /tmp/efi_part/efi/boot/grub.cfg with your favorite editor (launched as root).
4. Unmount the partition:
```
sudo umount /tmp/efi_part
```

### From ChromeOS

Same instructions that for Linux, except that on step 3 there is no available text editor with root support other than `vi`. You will probably need to have a look at the `vi` online resources.

# FAQ

1) The instructions are difficult to follow as I am not familiar with Linux commands.

I cannot not go much deeper into details here for now but I will try to clarify the install process once I see the main pain points. Nevertheless, ChromeOS is based on Linux and it would probably be interesting for you to read online resources on Linux basics before attempting this.

2) My computer will not boot the created USB flash drive / SD card whereas it normally can (and I have correctly followed the instructions).

Some devices (notably Surface Go) will not boot a valid USB flash drive / SD card with secure boot on even if the shim binary is signed. For those devices, you will need to disable secure boot in your bios settings and use the legacy EFI bootloader by adding the "-l" parameter when running the chromeos-install.sh script.

3) The first boot and the ones after a framework change or an update are incredibly long.

Unfortunately, the Brunch framework has to rebuild itself by copying the original rootfs, modules and firmware files after each significant change. The time this process takes depends mostly on your USB flash drive / SD card write speed. You may try with one that has better write speed or use the dual boot method to install it on your HDD.

4) ChromeOS reboots randomly.

This can in theory be due to a lot of things. However, the most likely reason is that your USB flash drive / SD card is too slow. You may try with one that has better write speed or use the dual boot method to install it on your HDD.

5) Some apps do not appear on the playstore (Netflix...)

In order to have access to the ChromeOS shell, ChromeOS is started in developer mode by default. If you have a stable enough system, you can remove "cros_debug" from the GRUB kernel command line (see "Modify the GRUB bootloader" section) and then do a Powerwash (ChromeOS mechanism which will wipe all your data partition) to disable developer mode.

6) Some apps on the Playstore show as incompatible with my device.

Some Playstore apps are not compatible with genuine Chromebooks so it is probably normal.

