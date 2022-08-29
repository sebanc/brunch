# Brunch principle

The Brunch framework purpose is to create a generic x86_64 ChromeOS image from an official recovery image. To do so, it uses a 1GB ROOTC partition (containing custom kernels, an initramfs, the swtpm binaries, userspace patches and config files) and a specific EFI partition to boot from it.

The source directory is composed of:
- the 3 scripts used to build the framework,
- Different Brunch Linux kernels based on ChromiumOS release including specific ChromeOS and surface devices patches (in the kernels directory),
- An "efi-partition" folder containing shim and GRUB and the specific GRUB config,
- An "extra-firmwares" directory to include firmware files which are not available in the mainline kernel firmware git (the mainline kernel firmware files are downloaded during the build process),
- A script folder which contains sub-scripts used during the build process and the Brunch initramfs script,
- A patches folder which contains the patches which will be applied by the initramfs to the ChromeOS rootfs,
- An "alsa-ucm-conf" folder which contains common alsa ucm files for better sound support.
- A few additional binaries in the "packages" folder.

The build script will copy the rootfs from a ChromeOS recovery image, chroot into it, and install the brunch toolchain in order to:
- build a few programs (notably efibootmgr, swtpm, nano)
- copy all added alsa ucm files, firmware files and patches. (stored in ROOTC image),
- copy the install script which will be used to create the ChromeOS image (chromeos-install.sh),
- create the ROOTC partition image,
- create 2 different efi partitions ("efi_secure.img" with secure boot support and "efi_legacy.img" for older devices).

From there, to create the ChromeOS image, the install script will only have to:
- create partitions,
- copy the ChromeOS recovery image partitions to this device,
- copy the ROOTC partition which contains the framework,
- replace the EFI partition.

At boot, GRUB will load the kernel present on ROOTC partition and launch the initramfs which is responsible for adding all the userspace patches to the standard ChromeOS rootfs before booting it, this process takes place:
- on the first boot,
- when the ROOTC partition is modified,
- when an update has been applied.

## Getting the source

Clone the branch you want to use (usually the latest) and enter the source directory:

```
git clone https://github.com/sebanc/brunch.git -b < ChromeOS version e.g. r103 >
cd brunch
```

## Build instructions

Building the framework is currently only possible under Linux (should work with any distro).

To build the release package, you need to have:
- root access,
- the `pv` package installed,
- 16 GB free disk space available,
- an internet connection.

The build process consist of 3 successive steps (only the last step needs to be run as root):
- preparing the kernels source:
`./prepare_kernels.sh`
- building the kernels:
`./build_kernels.sh`
- building the brunch package:
`sudo bash build_brunch.sh`

4. Verify that everything worked well. You should have an "out" directory containing the brunch_< version >_<date>.tar.gz archive.

