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
<h1 align="center">Install Brunch with Windows</h1>

<!-- Installation Guides -->
# USB installations
This guide is for installing Brunch to a USB (or other disk) using Windows. This guide is also required when singlebooting. To begin, boot into Windows and click the dropdown below to continue.

<details>
  <summary>Click to open Brunch USB guide</summary>

### Requirements
- Administrator access.
- Target Disk/USB must be 16 GB minimum.
  - You will also need about 16 GB of free space in your Windows installation.
- A linux installation via WSL2
- `pv`, `tar`, `unzip` and `cgpt` packages.
- A [compatible PC][compatibility] to boot Brunch on.
- An entry level understanding of the linux terminal.
  - This guide aims to make this process as easy as possible, but knowing the basics is expected.

### Recoveries
1. Download a recovery suitable for your CPU. The list below can help you select one. You do *not* need to select a recovery that matches the latest Brunch release number, the most recent avaliable is typically fine.
  
#### Intel
* 8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.
* 10th gen: "[jinlon][recovery-jinlon]".
* 11th gen & above: "[voxel][recovery-voxel]".
#### AMD
* Ryzen: "[gumboz][recovery-gumboz]".

Recoveries can be found by clicking the above links. They can also be found by going to [cros.tech][cros-tech] and searching for the recovery you want.

After selecting the recovery you want, you can select a specific release. Posted releases may be behind the current release, this is normal and you can update into the current release later. It is usually suggested to use the latest release avaliable.

### Gathering Files
2. Download the Brunch files from this GitHub repository. Do not use files found on other sites or linked in videos online. The [releases tab][releases-tab] can be found at the bottom of the right-hand column on the main GitHub page, but it is generally suggested to use the [latest release][latest-release].

When downloading a release, select the brunch...tar.gz file from the assets at the bottom of the release post. You do not need the source code files, do not download them.

Before continuing, you will need a linux distro installed from the Microsoft Store using WSL2, and the distro must be set up and ready to use. Please refer to online resources for this as the setup can be complicated for some systems.

### Prepare the Terminal
3. Once both files have been downloaded, the Brunch release and your chosen ChromeOS recovery, Launch WSL2.
4. Make sure that pv, cgpt, tar and unzip are installed.

```sudo apt update && sudo apt -y install pv cgpt tar unzip```
  * My example uses `apt`, a package manager for Debian and Ubuntu based distros. If you use Arch, you will need [vboot-utils][vboot-utils] for access to cgpt and a different package manager may be needed to install the rest.

4b. Some Linux releases may require the `universe` repo to install some of the above dependencies. If you get any errors about a dependency being unavaliable, add the `universe` repo with this command, and then try the previous step again afterwards.

```sudo add-apt-repository universe```
  
5. After all dependencies have been installed, `cd` into the directory where your files were downloaded.
  * Replace `username` with your *Windows* username.
  * The linux terminal is Case Sensitive, be mindful of capital letters.

```cd /mnt/c/Users/username/Downloads```
  
6. Extract the Brunch archive using `tar`
  * Replace `brunch_filename.tar.gz` with the file's actual filename.

```tar zxvf brunch_filename.tar.gz```
  
7. Extract the ChromeOS recovery using `unzip`
  * Replace `chromeos_filename.bin.zip` with the file's actual filename.

```unzip chromeos_filename.bin.zip```

Once completed, you will have 4 new files from the brunch archive, and a recovery bin that we will use in the next step.

### Install Brunch

8. Once you've got your files ready, you're ready to install Brunch.
  * As before, replace `chromeos_filename.bin` with the bin file's actual filename.

```sudo bash chromeos-install.sh -src chromeos_filename.bin -dst chromeos.img```

The script will ask for confirmation. If you're ready to install, type `yes` into the prompt.

The installation may take some time depending on the speed of your disk, please be patient. There may be a couple of GPT Header errors, which can be safely ignored. 

The installation will report that ChromeOS was installed when it is finished. Before closing the terminal, make sure that there are no additional errors in the terminal. If there are no errors, then you are good to go!

### Making the USB

9. Since WSL2 does not have direct disk access, we make an img with WSL2 and then use another program such as [Rufus][rufus-link] or [Etcher][etcher-link] to write the disk to a USB. Open the program of your choice, select the chromeos.img in your Downloads folder and write it to your USB.

### Next Steps
  
If you installed to a USB or a second internal disk, then you should be ready to boot into Brunch. If you've installed to a USB, keep it plugged in and reboot. It is normal for the first boot to take a very long time, please be patient.

* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.
* At this point, your device may incorrectly state that your installation is only 14 GB, regardless of it's actual size. This can be fixed by opening a developer shell on the startup screen with **Ctrl + Alt + F2**.
  * Log in as `root` there should be no password.
  * Enter `resize-data` then reboot the PC when it's finished. Your reported size should now be accurate.

## Secure Boot
  
10. If secure boot is enabled, a blue screen saying `Verification failed: (15) Access Denied` may appear upon boot. 
  * To enroll the key directly from a USB, select OK -> Enroll key from disk -> EFI-SYSTEM -> brunch.der -> Continue and reboot.

  </details>
  
***
 
# Singleboot installations
This guide is for installing Brunch to a disk using a Brunch USB. This guide requires having a working Brunch USB to initiate the install, you can make one by following the [guide above][brunch-usb-guide-win]. To begin, boot into a working Brunch USB and click the dropdown below to continue.

<details>
  <summary>Click to open singleboot guide</summary>

### Requirements
- Administrator access.
- Target Disk must be 16 GB minimum.
- Working Brunch USB.
- A [compatible PC][compatibility] to boot Brunch on.
- An entry level understanding of the linux terminal.
  - This guide aims to make this process as easy as possible, but knowing the basics is expected.

### Selecting a Target Disk
  
1. Log into ChromeOS, and switch to the TTY2 terminal with **Ctrl + Alt + F2**, then login as `chronos`.
  
2. Before continuing, you will need to know what disk you want to install to. Be absolutely sure **before** you continue, this installation will erase **everything** on that disk, including other partitions. The disk must be at least 16 GB, or the installation will fail. There are several ways to determine which disk is your target, in my example I'll be using `lsblk`.
  
```lsblk -e7```
  
This command will show your disks, and the partitions on them. It will also show their sizes and if they are currently mounted. Use this information to determine which disk is your target.
  
***
  
#### Tips:
  
* Your target will **never** be zram or a loop device.
* Some PCs may require RAID to be disabled before showing your disks correctly.
* For this installation, a USB is treated the same as any HDD or SSD.
* If there is an EFI mountpoint on a disk that disk is your boot disk.
  * You **cannot** install Brunch directly onto the same disk you are currently booting from.
* When doing a singleboot installation, your target will **not** be a partition. This method installs to the *entire* disk.
  
  ***
  
### Install Brunch
  
3. Once you've determined your target disk, you're ready to install Brunch.
  * You will replace `disk` with your target disk. (Such as `sdb`, `mmcblk0` or `nvme0n1` for example)
  
```sudo chromeos-install -dst /dev/disk```
  
The script will ask for confirmation. If you're ready to install, type `yes` into the prompt.
  
The installation may take some time depending on the speed of your target disk, please be patient. There may be a couple of GPT Header errors, which can be safely ignored. 
  
The installation will report that ChromeOS was installed when it is finished. Before closing the terminal, make sure that there are no additional errors in the terminal. If there are no errors, then you are good to go!

### Next Steps
  
It is normal for the first boot to take a very long time, please be patient.

* The first boot is the best time to setup anything important such as [changing kernels][changing-kernels] or [framework options][framework-options] by selecting the "ChromeOS (Settings)" boot option.
* If you have any issues, it is strongly advised to check out the [Brunch Configuration Menu][edit-brunch-config] for possible patches or solutions.
  
</details>  
  
  ***
 
# Dualboot installations
This guide is for installing Brunch to a partition using Windows WSL2.

<details>
  <summary>Click to open dualboot guide</summary>

### Requirements
- Administrator access.
- Target partition must be 16gb minimum, unencrypted (bitlocker disabled), and formatted as NTFS.
- A linux installation vis WSL2
- `pv`, `tar`, `unzip` and `cgpt` packages.
- A [compatible PC][compatibility] to boot Brunch on.
- An entry level understanding of the linux terminal.
  - This guide aims to make this process as easy as possible, but knowing the basics is expected.

### Recoveries
1. Download a recovery suitable for your CPU. The list below can help you select one. You do *not* need to select a recovery that matches the latest Brunch release number, the most recent avaliable is typically fine.
  
#### Intel
* 8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.
* 10th gen: "[jinlon][recovery-jinlon]".
* 11th gen & above: "[voxel][recovery-voxel]".
#### AMD
* Ryzen: "[gumboz][recovery-gumboz]".

Recoveries can be found by clicking the above links. They can also be found by going to [cros.tech][cros-tech] and searching for the recovery you want.

After selecting the recovery you want, you can select a specific release. Posted releases may be behind the current release, this is normal and you can update into the current release later. It is usually suggested to use the latest release avaliable.

### Gathering Files
2. Download the Brunch files from this GitHub repository. Do not use files found on other sites or linked in videos online. The [releases tab][releases-tab] can be found at the bottom of the right-hand column on the main GitHub page, but it is generally suggested to use the [latest release][latest-release].

When downloading a release, select the brunch...tar.gz file from the assets at the bottom of the release post. You do not need the source code files, do not download them.

Before continuing, you will need a linux distro installed from the Microsoft Store using WSL2, and the distro must be set up and ready to use. Please refer to online resources for this as the setup can be complicated for some systems.

### Prepare the Terminal
3. Once both files have been downloaded, the Brunch release and your chosen ChromeOS recovery, Launch WSL2.
4. Make sure that pv, cgpt, tar and unzip are installed.

```sudo apt update && sudo apt -y install pv cgpt tar unzip```
  * My example uses `apt`, a package manager for Debian and Ubuntu based distros. If you use Arch, you will need [vboot-utils][vboot-utils] for access to cgpt and a different package manager may be needed to install the rest.

4b. Some Linux releases may require the `universe` repo to install some of the above dependencies. If you get any errors about a dependency being unavaliable, add the `universe` repo with this command, and then try the previous step again afterwards.

```sudo add-apt-repository universe```
  
5. After all dependencies have been installed, `cd` into the directory where your files were downloaded.
  * Replace `username` with your *Windows* username.
  * The linux terminal is Case Sensitive, be mindful of capital letters.

```cd /mnt/c/Users/username/Downloads```
  
6. Extract the Brunch archive using `tar`
  * Replace `brunch_filename.tar.gz` with the file's actual filename.

```tar zxvf brunch_filename.tar.gz```
  
7. Extract the ChromeOS recovery using `unzip`
  * Replace `chromeos_filename.bin.zip` with the file's actual filename.

```unzip chromeos_filename.bin.zip```

Once completed, you will have 4 new files from the brunch archive, and a recovery bin that we will use in the next step.

### Install Brunch

8. Once you've got your files ready, you're ready to install Brunch.
  * As before, replace `chromeos_filename.bin` with the bin file's actual filename.
  * You will also replace `size` with a whole number. (Such as `14`, `20`, or `100` for example)
    * The number must be a *minimum* of 14, but *less* than the avaliable space on your partition in GB.

Make a directory to install Brunch, for example:  
  - run `mkdir /mnt/c/Users/username/brunch` if you want to install brunch in your home folder on C: partition.
  - or `mkdir /mnt/d/brunch` if you want to install brunch in the D: partition.

  Then launch the installer providing "-dst" argument with the name of the image file to be created (in your brunch directory):
```sudo bash chromeos-install.sh -src chromeos_filename.bin -dst /mnt/c/Users/username/brunch/chromeos.img -s size```

The installation may take some time depending on the speed of your target disk, please be patient. There may be a couple of GPT Header errors, which can be safely ignored. If you are told that there is not enough space to install, reduce the number at the end of your command until it fits. It is normal that the img cannot take the entire space of the partition, as some of that space is reserved by the system.

When the installer asks you for the type of install, type "dualboot" in the terminal and press "Enter" to continue.

The installation will report that ChromeOS was installed when it is finished. Before continuing, make sure that there are no additional errors in the terminal. If there are no errors, then you are good to continue!
  
### Set up Grub2Win
10. Install [Grub2win][grub2win] if you have not already, then launch the program. (Windows Defender sometimes will flag Grub2Win as a virus and remove it)
  
11. Click on the `Manage Boot Menu` button, then click `Chrome` under 'Import Configuration File'.
  
  * Select your chromeos.img.grub.txt file that we created earlier.
  * Click `Import Selected Items`
    * Your entry will not be saved unless you click `Apply`.

### Prevent Windows from locking the NTFS partition
12. Disable encryption / hibernation

ChromeOS will not be bootable and / or stable if you do not perform the below actions (Refer to Windows online resources if needed):
  - Ensure that bitlocker is disabled on the drive which contains the ChromeOS image or disable it.
  - Disable fast startup.
  - Disable hibernation.
  
At this point, you are ready to reboot and you'll be greeted by the Grub2win menu instead of booting into Windows. 

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
* Some PCs require a key to be held when booting to boot from USB or that USB booting is enabled in the BIOS
* The first boot can take up to an hour on some hardware. Brunch does not typically freeze on the Brunch logo. If you are seeing the Brunch logo, the system is _probably_ still booting.
* If your PC is stuck on the ChromeOS logo (White background), it is likely that you've got an incompatible dedicated GPU.
* If you get a blue screen saying "Verification failed" you can either disable secure boot in your bios settings, or [enroll the secure boot key][secure-boot].
  * To enroll the key directly from a USB, select OK -> Enroll key from disk -> EFI-SYSTEM -> brunch.der -> Continue and reboot.
* If the system reboots _itself_ when booting normally, then Brunch has run into an error and you may need to do some advanced troubleshooting.

In case you run into issues while installing or using Brunch, you can find support on Discord:

[![Discord][discord-shield]][discord-url]

<!-- Alternate Guide -->
## Looking for the Linux guide?
### [![Install with Linux][linux-img]][linux-guide]  [Install with Linux][Linux-guide]

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
