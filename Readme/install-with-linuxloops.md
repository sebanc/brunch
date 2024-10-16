<div id="top"></div>

<!-- Shields/Logos -->
[![License][license-shield]][license-url]
[![Issues][issues-shield]][issues-url]
[![Discord][discord-shield]][discord-url]
<!-- Project Logo -->
<p align="center">
  <a href="https://github.com/sebanc/brunch" title="Brunch">
   <img src="../Images/terminal_icon-512.png" width="128px" alt="Logo"/>
  </a>
</p>
<h1 align="center">Install Brunch with Linuxloops</h1>
  
<!-- Installation Guides -->
# Introduction
Linuxloops is a general purpose linux distro installer that also supports brunch and is available at: [https://github.com/sebanc/linuxloops][linuxloops]  
Linuxloops will perform automatically most of the brunch install steps (notably downloading brunch, downloading the ChromeOS recovery image, installing build dependencies and running the install script).  
  
# Install from Linux
  
<details>
  <summary>Install Brunch to a USB drive</summary>
  
### Requirements
- x86_64 based computer with UEFI boot support  
- Root access.  
- 8 GB available in your home directory.  
- Target USB must be 16 GB minimum.  
  
### Install process
1. Identify the recovery suitable for your CPU:  
  
Intel
  8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.  
  10th gen: "[jinlon][recovery-jinlon]".  
  11th gen & above: "[voxel][recovery-voxel]".  
AMD
  Ryzen: "[gumboz][recovery-gumboz]".  
  
2. Install the `PyQtWebEngine` package for your distribution:  
Debian / Ubuntu derivatives:  
  Debian 12 / Ubuntu 24.04 based distributions: `sudo apt install python3-venv python3-pyqt6.qtwebengine`  
  Older Debian / Ubuntu based distributions: `sudo apt install python3-venv python3-pyqt5.qtwebengine`  
Arch-based distributions: `sudo pacman -Syu python-pyqt6-webengine`  
RHEL-based distributions: `sudo dnf install python3-pyqt6-webengine`  
OpenSUSE: `sudo zypper in python3-PyQt6-WebEngine`  
Gentoo: `sudo emerge dev-python/PyQt6-WebEngine`  
Void: `sudo xbps-install python3-pyqt6-webengine python3-pyqt6-gui python3-pyqt6-widgets python3-pyqt6-network python3-pyqt6-webchannel python3-pyqt6-printsupport`  
  
3. Download the linuxloops script.  
  
`curl -L https://raw.githubusercontent.com/sebanc/linuxloops/main/linuxloops -O --create-dirs --output-dir ~/bin`  
  
4. Launch the linuxloops script and follow the GUI installer selecting "Brunch" as the distro and the recovery compatible with your CPU as the environment.  
  
`sudo -E bash ~/bin/linuxloops`  
  
5. Once the install process is finished, reboot your computer and select your USB drive as boot device.  
  
6. (secure boot enabled) A blue screen saying `Verification failed: (15) Access Denied` may appear upon boot.  
  * To enroll the key directly from a USB, select OK -> Enroll key from disk -> EFI-SYSTEM -> brunch.der -> Continue and reboot.  
  
### Next Steps
It is normal for the first boot to take a very long time, please be patient.  
  
* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.  
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.  
  
  
</details>
  
<details>
  <summary>Install Brunch to a HDD/SSD</summary>
  
### Requirements
- x86_64 based computer with UEFI boot support  
- Root access.  
- 8 GB available in your home directory.  
- A USB drive that must be 16 GB minimum.  
- A HDD/SSD drive that must also be 16 GB minimum.  
  
### Install process
1. Make a Brunch USB flashdrive using the above "Install Brunch to a USB drive" guide.  
  
2. Boot the ChromeOS USB drive, and switch to the TTY2 terminal with **Ctrl + Alt + F2**, then login as `chronos`.  
  
3. Before continuing, you will need to know what disk you want to install to. Be absolutely sure **before** you continue, this installation will erase **everything** on that disk, including other partitions. The disk must be at least 16 GB, or the installation will fail. There are several ways to determine which disk is your target, in this example we will use `lsblk`.  
  
`lsblk -e7`  
  
4. Once you've determined your target disk, you're ready to install Brunch.  
  * Replace `disk` in the below command with your target disk. (Such as `sdb`, `mmcblk0` or `nvme0n1` for example)  
  
`sudo chromeos-install -dst /dev/disk`  
  
The script will ask for confirmation. If you're ready to install, type `yes` into the prompt.  
  
5. Reboot your computer and select your HDD/SDD in the boot menu.  
  
### Next Steps
It is normal for the first boot to take a very long time, please be patient.  
  
* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.  
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.  
  
  
</details>
  
<details>
  <summary>Install Brunch as a disk image (dualboot)</summary>
  
### Requirements
- x86_64 based computer with UEFI boot support  
- Root access.  
- 8 GB available in your home directory.  
- An unencrypted partition with 14 GB available (in ext4, btrfs, ntfs or exfat format).  
- GRUB as bootloader.  
  
### Install process
1. Identify the recovery suitable for your CPU:  
  
Intel
  8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.  
  10th gen: "[jinlon][recovery-jinlon]".  
  11th gen & above: "[voxel][recovery-voxel]".  
AMD
  Ryzen: "[gumboz][recovery-gumboz]".  
  
2. Install the `PyQtWebEngine` package for your distribution:  
Debian / Ubuntu derivatives:  
  Debian 12 / Ubuntu 24.04 based distributions: `sudo apt install python3-venv python3-pyqt6.qtwebengine`  
  Older Debian / Ubuntu based distributions: `sudo apt install python3-venv python3-pyqt5.qtwebengine`  
Arch-based distributions: `sudo pacman -Syu python-pyqt6-webengine`  
RHEL-based distributions: `sudo dnf install python3-pyqt6-webengine`  
OpenSUSE: `sudo zypper in python3-PyQt6-WebEngine`  
Gentoo: `sudo emerge dev-python/PyQt6-WebEngine`  
Void: `sudo xbps-install python3-pyqt6-webengine python3-pyqt6-gui python3-pyqt6-widgets python3-pyqt6-network python3-pyqt6-webchannel python3-pyqt6-printsupport`  
  
3. Download the linuxloops script.  
  
`curl -L https://raw.githubusercontent.com/sebanc/linuxloops/main/linuxloops -O --create-dirs --output-dir ~/bin`  
  
4. Launch the linuxloops script and follow the GUI installer selecting "Brunch" as the distro and the recovery compatible with your CPU as the environment.  
  
`sudo -E bash ~/bin/linuxloops`  
  
Choose "image" at the install type prompt, place the image on an unencrypted parition and define the disk image size.  
  
5. At the end of the install process, the GUI installer will provide you with a command that will generate the needed GRUB configuration. Run it in a terminal.  
  
6. (secure boot enabled) Once install is finished, run:  
  
`sudo mokutil --import <image_path>/<image_name>.img.der`  
  
7. Reboot your computer and launch Brunch from GRUB.  
  
### Next Steps
It is normal for the first boot to take a very long time, please be patient.  
  
* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.  
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.  
  
  
</details>
  
  
# Install from Windows
  
<details>
  <summary>Install Brunch to a USB drive</summary>
  
### Requirements
- x86_64 based computer with UEFI boot support  
- Microsoft Windows 10/11 with WSL2  
- 22 GB available (8 GB for the install process, 14 GB for the USB image).  
- Target USB must be 16 GB minimum.  
  
### Install process
1. Identify the recovery suitable for your CPU:  
  
Intel
  8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.  
  10th gen: "[jinlon][recovery-jinlon]".  
  11th gen & above: "[voxel][recovery-voxel]".  
AMD
  Ryzen: "[gumboz][recovery-gumboz]".  
  
2. Launch WSL2 and install the `PyQtWebEngine` package:  
Ubuntu 24.04 and above: `sudo apt install python3-venv python3-pyqt6.qtwebengine`  
Older Ubuntu versions: `sudo apt install python3-venv python3-pyqt5.qtwebengine`  
  
3. Download the linuxloops script.  
  
`curl -L https://raw.githubusercontent.com/sebanc/linuxloops/main/linuxloops -O --create-dirs --output-dir ~/bin`  
  
4. Launch the linuxloops script and follow the GUI installer selecting "Brunch" as the distro and the recovery compatible with your CPU as the environment.  
  
`sudo -E bash ~/bin/linuxloops`  
  
Choose "image" at the install type prompt, place the image outside of WSL2 (e.g. /mnt/c/Users/"username"/Downloads) and define the disk image size as 14GB.  
  
5. Once the install process is finished, use a software like Rufus or Etcher to write the image you have just create to an USB drive.  
  
6. Reboot your computer and select your USB drive as boot device.  
  
7. (secure boot enabled) A blue screen saying `Verification failed: (15) Access Denied` may appear upon boot.  
  * To enroll the key directly from a USB, select OK -> Enroll key from disk -> EFI-SYSTEM -> brunch.der -> Continue and reboot.  
  
### Next Steps
It is normal for the first boot to take a very long time, please be patient.  
  
* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.  
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.  
  
  
</details>
  
<details>
  <summary>Install Brunch to a HDD/SSD</summary>
  
### Requirements
- x86_64 based computer with UEFI boot support  
- Microsoft Windows 10/11 with WSL2  
- 22 GB available (8 GB for the install process, 14 GB for the USB image).  
- A USB drive that must be 16 GB minimum.  
- A HDD/SSD drive that must also be 16 GB minimum.  
  
### Install process
1. Make a Brunch USB flashdrive using the above "Install Brunch to a USB drive" guide.  
  
2. Boot the ChromeOS USB drive, and switch to the TTY2 terminal with **Ctrl + Alt + F2**, then login as `chronos`.  
  
3. Before continuing, you will need to know what disk you want to install to. Be absolutely sure **before** you continue, this installation will erase **everything** on that disk, including other partitions. The disk must be at least 16 GB, or the installation will fail. There are several ways to determine which disk is your target, in this example we will use `lsblk`.  
  
`lsblk -e7`  
  
4. Once you've determined your target disk, you're ready to install Brunch.  
  * Replace `disk` in the below command with your target disk. (Such as `sdb`, `mmcblk0` or `nvme0n1` for example)  
  
`sudo chromeos-install -dst /dev/disk`  
  
The script will ask for confirmation. If you're ready to install, type `yes` into the prompt.  
  
5. Reboot your computer and select your HDD/SDD in the boot menu.  
  
### Next Steps
It is normal for the first boot to take a very long time, please be patient.  
  
* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.  
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.  
  
  
</details>
  
<details>
  <summary>Install Brunch as a disk image (dualboot)</summary>
  
### Requirements
- x86_64 based computer with UEFI boot support  
- Microsoft Windows 10/11 with WSL2  
- 8 GB available.  
- An unencrypted partition with 14 GB available (in ntfs or exfat format).  
- Secure boot disabled.  
  
### Install process
1. Identify the recovery suitable for your CPU:  
  
Intel
  8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.  
  10th gen: "[jinlon][recovery-jinlon]".  
  11th gen & above: "[voxel][recovery-voxel]".  
AMD
  Ryzen: "[gumboz][recovery-gumboz]".  
  
2. Launch WSL2 and install the `PyQtWebEngine` package:  
Ubuntu 24.04 and above: `sudo apt install python3-venv python3-pyqt6.qtwebengine`  
Older Ubuntu versions: `sudo apt install python3-venv python3-pyqt5.qtwebengine`  
  
3. Download the linuxloops script.  
  
`curl -L https://raw.githubusercontent.com/sebanc/linuxloops/main/linuxloops -O --create-dirs --output-dir ~/bin`  
  
4. Launch the linuxloops script and follow the GUI installer selecting "Brunch" as the distro and the recovery compatible with your CPU as the environment.  
  
`sudo -E bash ~/bin/linuxloops`  
  
Choose "image" at the install type prompt, place the image on your unencrypted parition and define the disk image size.  
  
5. Install [Grub2win][grub2win] and launch the program.  
  
6. Click on the `Manage Boot Menu` button, then click `Chrome` under 'Import Configuration File'.  
  
  * Select the .grub.txt file in the brunch image folder.  
  * Click `Import Selected Items`  
    * Your entry will not be saved unless you click `Apply`.  
  
7. Prevent Windows from locking the NTFS partition.  
  
ChromeOS will not be bootable and / or stable if you do not perform the below actions (Refer to Windows online resources if needed):  
  - Ensure that bitlocker is disabled on the drive which contains the ChromeOS image or disable it.  
  - Disable fast startup.  
  - Disable hibernation.  
  
At this point, you are ready to reboot and you'll be greeted by the Grub2win menu instead, select Brunch in the boot menu.  
  
### Next Steps
It is normal for the first boot to take a very long time, please be patient.  
  
* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.   
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.  
  
  
</details>
 ***
  
# [Troubleshooting and Support][troubleshooting-and-faqs]
  
See the full [Troubleshooting and Support][troubleshooting-and-faqs] page if you're having issues.  
  
### Additional Tips
* If you're having trouble booting a Brunch USB, make sure that UEFI is enabled in the BIOS.  
* Some PCs require a key to be held when booting to boot from USB or that USB booting is enabled in the BIOS.  
* The first boot can take up to an hour on some hardware. Brunch does not typically freeze on the Brunch logo. If you are seeing the Brunch logo, the system is _probably_ still booting.  
* If your PC is stuck on the ChromeOS logo (White background), it is likely that you've got an incompatible dedicated GPU.  
* If you get a blue screen saying "Verification failed" you can either disable secure boot in your bios settings, or [enroll the secure boot key][secure-boot].  
  * To enroll the key directly from a USB, select OK -> Enroll key from disk -> EFI-SYSTEM -> brunch.der -> Continue and reboot.  
* If the system reboots _itself_ when booting normally, then Brunch has run into an error and you may need to do some advanced troubleshooting.  
  
In case you run into issues while installing or using Brunch, you can find support on Discord:  
  
[![Discord][discord-shield]][discord-url]
  

<!-- Reference Links -->
<!-- Badges -->
[license-shield]: https://img.shields.io/github/license/sebanc/brunch?label=License&logo=Github&style=flat-square
[license-url]: ../LICENSE
[forks-shield]: https://img.shields.io/github/forks/sebanc/brunch?label=Forks&logo=Github&style=flat-square
[forks-url]: https://github.com/sebanc/brunch/fork
[stars-shield]: https://img.shields.io/github/stars/sebanc/brunch?label=Stars&logo=Github&style=flat-square
[stars-url]: https://github.com/sebanc/brunch/stargazers
[issues-shield]: https://img.shields.io/github/issues/sebanc/brunch?label=Issues&logo=Github&style=flat-square
[issues-url]: https://github.com/sebanc/brunch/issues
[pulls-shield]: https://img.shields.io/github/issues-pr/sebanc/brunch?label=Pull%20Requests&logo=Github&style=flat-square
[pulls-url]: https://github.com/sebanc/brunch/pulls
[discord-shield]: https://img.shields.io/badge/Discord-Join-7289da?style=flat-square&logo=discord&logoColor=%23FFFFFF
[discord-url]: https://discord.gg/x2EgK2M

<!-- Outbound Links -->
[linuxloops]: https://github.com/sebanc/linuxloops
[linuxloops-live]: https://github.com/sebanc/linuxloops/releases
[croissant]: https://github.com/imperador/chromefy
[swtpm]: https://github.com/stefanberger/swtpm
[linux-surface]: https://github.com/linux-surface/linux-surface
[chromebrew]: https://github.com/skycocker/chromebrew
[intel-cpus]: https://en.wikipedia.org/wiki/Intel_Core
[intel-list]: https://en.wikipedia.org/wiki/List_of_Intel_CPU_microarchitectures
[atom-cpus]: https://en.wikipedia.org/wiki/Intel_Atom
[atom-list]: https://en.wikipedia.org/wiki/List_of_Intel_Atom_microprocessors
[amd-sr-list]: https://en.wikipedia.org/wiki/List_of_AMD_accelerated_processing_units#%22Stoney_Ridge%22_(2016)
[amd-ry-list]: https://en.wikipedia.org/wiki/List_of_AMD_Ryzen_processors
[recovery-bobba]: https://cros.tech/device/bobba
[recovery-shyvana]: https://cros.tech/device/shyvana
[recovery-jinlon]: https://cros.tech/device/jinlon
[recovery-voxel]: https://cros.tech/device/voxel
[recovery-gumboz]: https://cros.tech/device/gumboz
[cros-tech]: https://cros.tech/
[cros-official]: https://cros-updates-serving.appspot.com/
[vboot-utils]: https://aur.archlinux.org/packages/vboot-utils
[rufus-link]: https://rufus.ie/
[etcher-link]: https://www.balena.io/etcher/
[grub2win]: https://sourceforge.net/projects/grub2win/

<!-- Images -->
[decon-icon-24]: ../Images/decon_icon-24.png
[decon-icon-512]: ../Images/decon_icon-512.png
[terminal-icon-24]: ../Images/terminal_icon-24.png
[terminal-icon-512]: ../Images/terminal_icon-512.png
[settings-icon-512]: ../Images/settings_icon-512.png
[windows-img]: https://img.icons8.com/color/24/000000/windows-10.png
[linux-img]: https://img.icons8.com/color/24/000000/linux--v1.png

<!-- Internal Links -->
[windows-guide]: ./install-with-windows.md
[linux-guide]: ./install-with-linux.md
[troubleshooting-and-faqs]: ./troubleshooting-and-faqs.md
[compatibility]: ../README.md#supported-hardware
[changing-kernels]: ./troubleshooting-and-faqs.md#kernels
[framework-options]: ./troubleshooting-and-faqs.md#framework-options
[releases-tab]: https://github.com/sebanc/brunch/releases
[latest-release]: https://github.com/sebanc/brunch/releases/latest
[brunch-der]: https://github.com/sebanc/brunch/raw/main/brunch.der
[secure-boot]: ./install-with-linux.md#secure-boot
[brunch-usb-guide-win]:  ./install-with-windows.md#usb-installations
[brunch-usb-guide-lin]:  ./install-with-linux.md#usb-installations
[edit-brunch-config]: ./troubleshooting-and-faqs.md#brunch-configuration-menu
