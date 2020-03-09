# Brunch principle

The Brunch framework purpose is to create a generic x86_64 ChromeOS image from an official recovery image. To do so, it uses a 1GB ROOTC partition (containing a custom kernel, an initramfs, the swtpm binaries, userspace patches and config files) and a specific EFI partition to boot from it.

The source directory is composed of:
- the build.sh script used to build the framework,
- the Brunch Linux kernel based on ChromiumOS release including specific ChromeOS and surface devices patches (in the kernel directory),
- An "efi-mods" folder containing shim and GRUB (both issued from Fedora) and the specific GRUB config,
- A "firmware-mods" directory to include firmware files which are not available in the mainline kernel firmware git (the mainline kernel firmware files are downloaded during the build process),
- A script folder which contains sub-scripts used during the build process and the Brunch initramfs script,
- A patches folder which contains the patches which will be applied by the initramfs to the ChromeOS rootfs,
- An "alsa-ucm-mods" folder which contains common alsa ucm files for better sound support.

The build script will copy the rootfs from a ChromeOS recovery image, chroot into it, and install the Chromebrew framework in order to:
- download from git and build the swtpm and its dependencies. (stored in ROOTC image),
- download and install the standard mainline kernel firmware files. (stored in ROOTC image),
- build busybox from its git and create the initramfs. (stored in ROOTC image),
- copy all added alsa ucm files, firmware files and patches. (stored in ROOTC image),
- copy the install script which will be used to create the ChromeOS image (chromeos-install.sh),
- create a ChromeOS partition table with an added 1GB ROOTC partition (maingpt.img file),
- create the ROOTC partition image,
- create 2 different efi partitions ("efi_secure.img" with secure boot support and "efi_legacy.img" for older devices).

From there, to create the ChromeOS image, the install script will only have to:
- copy the primary GPT partition to a device, adjust the disk/partitions size and create the secondary GPT,
- copy the ChromeOS recovery image partitions to this device,
- copy the ROOTC partition which contains the framework,
- replace the EFI partition.

At boot, GRUB will load the kernel present on ROOTC partition and launch the initramfs which is responsible for adding all the userspace patches to the standard ChromeOS rootfs before booting it, this process takes place:
- on the first boot,
- when the ROOTC partition is modified,
- when an update has been applied.

Thanks goes to project Croissant, the swtpm maintainer and the Chromebrew framework !

# Build instructions

Building the framework is currently only possible under Linux (should work with any distro).

The build process consist of 4 successive steps:
- getting the source,
- building the Kernel,
- signing the kernel and GRUB for Secure Boot,
- building the install package.

Warning: The build scripts currently lacks many build process checks. I will try to work on that as soon as I have the time.

## Getting the source

Clone the branch you want to use (usually the latest) and enter the source directory:

```
git clone https://github.com/sebanc/brunch.git -b < ChromeOS version e.g. r79 >
cd brunch
```

## Building the kernel

1. Install all packages required to build the Linux kernel (refer to your distro's online resources).
2. From the path where you extracted the source files, launch the kernel build (in a subdirectory named out).
```
cd kernel
make -j$(nproc) O=out chromeos_defconfig
make -j$(nproc) O=out
```
3. Make sure the kernel build process finished without errors.
4. Go back to the main directory where you extracted the source.

## Signing the kernel and GRUB for Secure Boot

If you want to use Secure Boot, you will have to create your own certificate and sign both the kernel and the grub efi binary. (refer to online resources)
If you do not need Secure Boot, you can skip this step.

## Building the release package.

To build the release package, you need to have:
- root access,
- the `pv` and `cgpt` packages/binaries installed,
- 16 GB free disk space available,
- an internet connection.

1. Download the "nautilus" ChromeOS recovery image from here (https://cros-updates-serving.appspot.com/).

Note: This should work with any ChromeOS image compatible with your CPU. Nevertheless, as far as I know, the nautilus recovery image has the largest kernel/rootfs partitions and therefore has the most compatible GPT partitions scheme regardless of the image you will use at install time.

2. Make sure you have 16GB of free space available for the build.

3. Launch the build as root:
```
sudo bash build.sh < path to the ChromeOS recovery image >
```
4. Verify that everything worked well. You should have an "out" directory containing 2 tar.gz files:
- brunch_< version >.tar.gz which contains the brunch package.
- sources_< version >.tar.gz which contains the sources that were used for building binaries inside the chroot (only for backup/history purpose).

