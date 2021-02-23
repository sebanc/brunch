# Brunch framework

## Overview

First of all, thanks goes to project Croissant, the swtpm maintainer and the Chromebrew framework for their work which was actively used when creating this project.

The purpose of the Brunch framework is to create a generic x86_64 ChromeOS image from an official recovery image. To do so, it uses a 1GB ROOTC partition (containing a custom kernel, an initramfs, the swtpm binaries, userspace patches and config files) and a specific EFI partition to boot from it.

**Warning: As Brunch is not the intended way for ChromeOS to work, at some point a ChromeOS script could potentially behave badly with Brunch and delete data unexpectedly (even on non-ChromeOS partitions). Also, ChromeOS recovery images include device firmware updates that a close enough device might potentially accept and get flashed with the wrong firmware. By installing Brunch you agree to take those risks and I cannot be held responsible for anything bad that would happen to your device including data loss.
It is therefore highly recommended to only use this framework on a device which does not contain any sensitive data and to keep non-sensitive data synced with a cloud service.**

## Hardware support and added features

Hardware support is highly dependent on the general Linux kernel hardware compatibility. As such only Linux supported hardware will work and the same specific kernel command line options recommended for your device should be passed through the GRUB bootloader (see "Modify the GRUB bootloader" section).

Base hardware compatibility:
- x86_64 computers with UEFI boot support (BIOS/MBR devices have limited support through a specific procedure detailled in "Limited support for MBR/BIOS devices" section),
- Intel hardware (CPU and GPU) starting from 1st generation "Nehalem" (refer to https://en.wikipedia.org/wiki/Intel_Core),
- AMD Ryzen 3XXX (CPU and GPU), only with "zork" recovery image (newer Ryzen models are not yet supported),
- AMD Stoney Ridge (refer to https://en.wikipedia.org/wiki/List_of_AMD_accelerated_processing_units), only with "grunt" recovery image (older AMD CPU are not supported),
- Devices having only a Nvidia graphic card are not supported.

Notes: 
- Intel Gen 1 graphics do not work with ChromeOS versions above r81 (it might still change with a future ChromeOS update).
- Some Intel 10th Gen devices do not seem to be compatible with the android container.
- Intel 11th Gen devices are not supported for now.

Specific hardware support:
- sensors: an experimental patch aims to allow intel ISH accelerometer and light sensors through a custom kernel module,
- Microsoft Surface devices: dedicated kernel patches are included.

Additional features:
- nano text editor
- efibootmgr

## ChromeOS recovery images

2 types of ChromeOS recovery images exist and use different device configuration mechanisms:
- non-unibuild images: configured for single device configurations like eve (Google Pixelbook) and nocturne (Google Pixel Slate) for example.
- unibuild images: intended to manage multiple devices through the use of the CrosConfig tool.

Contrarily to the Croissant framework which mostly supports non-unibuilds images (configuration and access to android apps), Brunch should work with both but will provide better hardware support for unibuild images.

Currently:
- "rammus" is the recommended image for devices with 4th generation Intel CPU and newer.
- "samus" is the recommended image for devices with 3rd generation Intel CPU and older.
- "zork" is the image to use for AMD Ryzen 3XXX.
- "grunt" is the image to use for AMD Stoney Ridge.

ChromeOS recovery images can be downloaded from: https://cros-updates-serving.appspot.com/ or https://cros.tech/

# Brunch toolkit

If you are not comfortable with the linux commands in the below instructions, the "brunch-toolkit" provided by WesBosch has been designed to make installing and updating brunch easy:
https://github.com/WesBosch/brunch-toolkit

General features (available within Brunch, Linux & WSL):
- Check user's CPU for Brunch compatibility.
- Suggest usable recoveries based on user's hardware.
- Install Brunch to disk or partition.

Features available when used within a brunch install:
- Update ChromeOS and/or Brunch.
- Modify the ChromeOS start up animation.

# Support

In case you run into issues while installing / using Brunch, below are the main places where you can find support:
- Discord: https://discord.gg/x2EgK2M
- Telegram: https://t.me/chromeosforpc
- Reddit: https://www.reddit.com/r/Brunchbook

# Install instructions

You can install ChromeOS on a USB flash drive / SD card (16GB minimum) or as an image on your hard disk for dual booting (14GB of free space needed).

## Install ChromeOS from Linux (the easiest way)

### Requirements

- root access.
- `pv`, `tar` and `cgpt` packages/binaries.

### Install ChromeOS on a USB flash drive / SD card / HDD (full disk install / single boot)

1. Download the ChromeOS recovery image and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery image version you have downloaded (from the GitHub release section).
3. Open a terminal, navigate to the directory containing the package.
4. Extract it: 
```
tar zxvf brunch_< version >.tar.gz
```
5. Identify your USB flash drive / SD card / HDD device name e.g. /dev/sdX (Be careful here as the installer will erase all data on the target drive)
6. Install ChromeOS on the USB flash drive / SD card / HDD:
```
sudo bash chromeos-install.sh -src < path to the ChromeOS recovery image > -dst < your USB flash drive / SD card device. e.g. /dev/sdX >
```
7. Reboot your computer and boot from the USB flash drive / SD card / HDD (refer to your computer manufacturer's online resources).
8. (Secure Boot only) A blue screen saying "Verification failed: (15) Access Denied" will appear upon boot and you will have to enroll the secure boot key by selecting "OK->Enroll key from disk->EFI-SYSTEM->brunch.der->Continue". Reboot your computer and boot again from the USB flash drive / SD card.

The GRUB menu should appear, select ChromeOS and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen. You can now start using ChromeOS.

### Dual Boot ChromeOS from your HDD

ChromeOS partition scheme is very specific which makes it difficult to dual boot. One solution to circumvent that is to keep ChromeOS in a disk image on the hard drive and run it from there.

Make sure you have an ext4 (recommended) or NTFS partition with at least 14gb of free space available and no encryption or create one (refer to online resources).

1. Download the ChromeOS recovery image and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery image version you have downloaded (from the GitHub release section).
3. Open a terminal, navigate to the directory containing the package.
4. Extract it: 
```
tar zxvf brunch_< version >.tar.gz
```
5. Mount the unencrypted ext4 or NTFS partition on which we will create the disk image to boot from:
```
mkdir -p ~/tmpmount
sudo mount < the destination partition (ext4 or ntfs) which will contain the disk image > ~/tmpmount
```
6. Create the ChromeOS disk image:
```
sudo bash chromeos-install.sh -src < path to the ChromeOS recovery image > -dst ~/tmpmount/chromeos.img -s < size you want to give to your chromeos install in GB (system partitions will take around 10GB, the rest will be for your data) >
```
7. Create a GRUB configuration file for brunch in your linux install:
- Copy the grub config which appeared in the terminal at the end of the process (the text between lines with stars)
- Run `sudo cp /etc/grub.d/40_custom /etc/grub.d/99_brunch`
- Then run `sudo nano /etc/grub.d/99_brunch`, paste the grub config at the end of the file. Save the changes and exit nano (CTRL-X).
- Lastly, run `sudo update-grub`.
8. Unmount the destination partition
```
sudo umount ~/tmpmount
```
9. (secure boot only) Download the secure boot key "brunch.der" in this branch (master) of the repository and enroll it by running the command:
```
sudo mokutil --import brunch.der
```
10. Reboot your computer and boot to the bootloader with the modified GRUB config.

The GRUB menu should appear, select "ChromeOS (boot from disk image)" and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen. You can now start using ChromeOS from your HDD.

## Install ChromeOS from Windows

### Requirements

- Administrator access.

### Install ChromeOS on a USB flash drive / SD card

1. Download the ChromeOS recovery image and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery version you have downloaded (from the GitHub release section).
3. Install the Ubuntu WSL from the Microsoft store (refer to online resources).
4. Launch Ubuntu WSL and install pv, tar and cgpt packages:
```
sudo apt update && sudo apt install pv tar cgpt
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
11. (Secure Boot only) A blue screen saying "Verification failed: (15) Access Denied" will appear upon boot and you will have to enroll the secure boot key by selecting "OK->Enroll key from disk->EFI-SYSTEM->brunch.der->Continue". Reboot your computer and boot again from the USB flash drive / SD card.
12. The GRUB menu should appear, select ChromeOS and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen.
At this stage, your USB flash drive / SD card is incorrectly recognized as 14GB regardless of its actual capacity. To fix this:
13. At the ChromeOS startup screen, press CTRL+ALT+F2 to go into a shell session.
14. Login as `root`
15. Execute the below command:
```
sudo resize-data
```
16. Reboot your computer when requested and boot again from USB flash drive / SD card. You can now start using ChromeOS.

### Dual Boot ChromeOS from your HDD

1. Make sure you have a NTFS partition with at least 14gb of free space available and no BitLocker encryption or create one (refer to online resources).
2. Make a ChromeOS USB flashdrive / SD card (see above) and boot it.
3. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite).
4. Mount the unencrypted ext4 or NTFS partition on which we will create the disk image to boot from:
```
mkdir -p ~/tmpmount
sudo mount < the destination partition (ext4 or ntfs) which will contain the disk image > ~/tmpmount
```
5. Create the ChromeOS disk image:
```
sudo bash chromeos-install -dst ~/tmpmount/chromeos.img -s < size you want to give to your chromeos install in GB (system partitions will take around 10GB, the rest will be for your data) >
```
6. Copy the GRUB configuration which is displayed in the terminal (select it and CTRL+SHIFT+C).
7. Run `sudo nano ~/tmpmount/chromeos.grub.txt` and paste the config there (CTRL°SHIFT+V to paste and then CTRL-X to exit)
8. Unmount the destination partition
```
sudo umount ~/tmpmount
```
9. Reboot to Windows, Install grub 2 win (https://sourceforge.net/projects/grub2win/) and launch the application.
10. Click on `Manage Boot Menu` button, then `Add A New Entry`.
11. Select `submenu` in the 'Type' section and input "Chrome OS" as title.
12. Now, click `Edit Custom Code` this will open a text file. Open the chromeos.grub.txt file we saved in step 7 and copy the grub configuration in grub2win.
#### Then remove the "rmmod tpm" line.
13. Click `Ok` and `apply` (It won't save your entry unless you click `ok` and `apply`)
14. Important: Disable "Fast startup" in Windows (refer to online resources).
15. Reboot.
16. The GRUB-2 win menu should appear, select "ChromeOS". Brunch will be rebuilt on first boot so, be patient. You should be greeted by ChromeOS startup screen once the process completes.
You can now start using ChromeOS from your HDD.

## Install ChromeOS on HDD from ChromeOS

1. Boot your ChromeOS USB flash drive / SD card.
2. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
3. Identify your HDD device name e.g. /dev/sdX (Be careful here as the installer will erase all data on the target drive)
4. Install ChromeOS to HDD:
```
sudo chromeos-install -dst < your HDD device. e.g. /dev/sdX >
```
5. Shutdown your computer and remove your ChromeOS USB flash drive / SD card.

Note: Even if you boot from GRUB on your HDD, if you have a ChromeOS USB flash drive / SD card inserted, the initramfs will boot from it in priority.

The GRUB menu should appear, select ChromeOS and after a few minutes (the Brunch framework is building itself on the first boot), you should be greeted by ChromeOS startup screen. You can now start using ChromeOS.

## Limited support for MBR/BIOS devices

The mbr_support.tar.gz patch present in the this branch (master) allows booting Brunch on BIOS/MBR devices with a few limitations:
- Brunch can only be installed from linux.
- The dual boot method is not supported.

Installation procedure:
1) Verify that your device cpu/gpu is supported according to the "base hardware compatibility" requirements.
2) Verify that `pv`, `tar`,`cgpt` and `sgdisk` packages/binaries are installed.
3) Extract the brunch release archive to your working directory.
4) Extract the mbr_support.tar.gz patch present in the this branch (master) of the Brunch repo in the same directory (overwriting files when requested)
5) Download the ChromeOS recovery image and extract it.
6) Follow the linux "Install ChromeOS on a USB flash drive / SD card / HDD (full disk install / single boot)" instructions from step 5.

# Optional steps

## Framework options

Some options can be passed through the kernel command lines to activate specific features which might be dangerous or not work from everyone:
- "enable_updates": allow native ChromeOS updates (use at your own risk: ChromeOS will be updated but not the Brunch framework/kernel which might render your ChromeOS install unstable or even unbootable),
- "android_init_fix": alternative init to support devices on which the android container fails to start with the standard init,
- "mount_internal_drives": allows automatic mounting of HDD partitions in ChromeOS (android media server will scan those drives which will cause high CPU usage until it has finished, it might take hours depending on your data), partition label will be used if it exists,
- "broadcom_wl": enable this option if you need the broadcom_wl module,
- "iwlwifi_backport": enable this option if your intel wireless card is not supported natively in the kernel,
- "rtl8188eu": enable this option if you have a rtl8188eu wireless card/adapter,
- "rtl8723bu": enable this option if you have a rtl8723bu wireless card/adapter,
- "rtl8723de": enable this option if you have a rtl8723de wireless card/adapter,
- "rtl8812au": enable this option if you have a rtl8812au wireless card/adapter,
- "rtl8814au": enable this option if you have a rtl8814au wireless card/adapter,
- "rtl8821ce": enable this option if you have a rtl8821ce wireless card/adapter,
- "rtl8821cu": enable this option if you have a rtl8821cu wireless card/adapter,
- "rtl88x2bu": enable this option if you have a rtl88x2bu wireless card/adapter,
- "rtbth": enable this option if you have a RT3290/RT3298LE bluetooth device,
- "ipts": enable support for Surface devices touchscreen with kernel 5.4 / 5.10 (thanks go to the linux-surface team, especially StollD),
- "oled_display": enable this option if you have an oled display (use with kernel 5.10),
- "acpi_power_button": try this option if long pressing the power button does not display the power menu,
- "alt_touchpad_config": try this option if you have touchpad issues,
- "alt_touchpad_config2": another option to try if you have touchpad issues,
- "disable_intel_hda": some Chromebooks need to blacklist the snd_hda_intel module, use this option to reproduce it,
- "internal_mic_fix": allows to forcefully enable internal mic on some devices,
- "asus_c302": applies asus c302 specific firmwares and fixes,
- "baytrail_chromebook": applies baytrail chromebooks specific audio fixes,
- "sysfs_tablet_mode": allow to control tablet mode from sysfs (`echo 1 | sudo tee /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode` to activate it or use 0 to disable it),
- "force_tablet_mode": same as above except tablet mode is enabled by default on boot,
- "suspend_s3": disable suspend to idle (S0ix) and use S3 suspend instead,
- "advanced_als": default ChromeOS auto-brightness is very basic (https://chromium.googlesource.com/chromiumos/platform2/+/master/power_manager/docs/screen_brightness.md). This option activates more auto-brightness levels (based on the Google Pixel Slate implementation).

Add "options=option1,option2,..." (without spaces) to the kernel command line to activate them.
you can add it after cros_debug and before loop.max.....

For example: booting with "options=enable_updates,advanced_als" will activate both options.

## Changing kernel version

Currently Brunch uses ChromiumOS kernel 5.4 by default as it is considered to be the most stable, however kernel 4.19 and an experimental kernel 5.10 are also included. If you want to try another kernel, you can replace "/kernel" in the grub configuration by "/kernel-4.19" or "/kernel-5.10".

WARNING: Changing kernel can prevent you from logging into your ChromeOS account, in which case a powerwash is the only solution (CTRL+ALT+SHIFT+R at the login screen). Therefore, before switching to a different kernel, make sure you have a backup of all your data.

## Kernel command line parameters

Those are not options, just add them on the kernel command line after "cros_debug" and before "options=...." if any:
- "enforce_hyperthreading=1": improve performance by disabling a ChromeOS security feature and forcing hyperthreading everywhere (even in crositini).
- "i915.enable_fbc=0 i915.enable_psr=0": if you want to use crouton (needed with kernel 5.4).
- "psmouse.elantech_smbus=1": fix needed for some elantech touchpads.
- "psmouse.synaptics_intertouch=1": enables gestures with more than 2 fingers on some touchpad models.
- "console=": No text will be displayed on boot (it will not make boot faster).

like this: 
![](https://user-images.githubusercontent.com/69226625/97113026-9fec2880-170d-11eb-930f-972f0b38af4f.png)

## Identify the installed Brunch framework version

1. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
2. Display the Brunch version:
```
cat /etc/brunch_version
```

## Update both ChromeOS and the Brunch framework

It is currently recommended to only update ChromeOS when the matching version of the Brunch framework has been released.

1. Download the new ChromeOS recovery image version and extract it.
2. Download the Brunch release corresponding to the ChromeOS recovery version (from the GitHub release section).
3. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
4. Update both ChromeOS and Brunch:
```
sudo chromeos-update -r < path to the ChromeOS recovery image > -f < path to the Brunch release archive >
```
5. Restart ChromeOS

Note: BiteDasher created a script which updates both Brunch and ChromeOS with a single command: https://github.com/BiteDasher/brcr-update

## Update only the Brunch framework

If you chose to use the "enable_updates" option and have updated to a new ChromeOS release, you might want to update the brunch framework to match your current ChromeOS version.

1. Download the Brunch release corresponding to your ChromeOS version (from the GitHub release section).
2. Open the ChromeOS shell (CTRL+ALT+T and enter `shell` at the invite)
3. Update Brunch:
```
sudo chromeos-update -f < path to the Brunch release archive >
```
4. Restart ChromeOS

## Modify the GRUB configuration

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

1. Run `sudo edit-grub-config`.
2. Now you can modify your grub entry.
3. Save by Ctrl+o (Press enter to confirm).
4. Exit by Ctrl+x.

You can visit wiki https://github.com/sebanc/brunch/wiki


# FAQ

1) The instructions are difficult to follow as I am not familiar with Linux commands.

I can not go much deeper into details here for now but I will try to clarify the install process once I see the main pain points. Nevertheless, ChromeOS is based on Linux and it would probably be interesting for you to read online resources on Linux basics before attempting this.

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

