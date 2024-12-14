<div id="top"></div>

<!-- Shields/Logos -->
[![License][license-shield]][license-url]
[![Issues][issues-shield]][issues-url]
[![Discord][discord-shield]][discord-url]

<!-- Project Logo -->
<p align="center">
  <a href="https://github.com/sebanc/brunch" title="Brunch">
   <img src="./Images/decon_icon-512.png" width="128px" alt="Logo"/>
  </a>
</p>
<h1 align="center">Brunch Framework</h1>

<!-- Special Thanks -->

## Special Thanks

First of all, thanks goes to [Project Croissant][croissant], the [swtpm][swtpm] maintainer, the [Linux-Surface crew][linux-surface] and the [Chromebrew framework][chromebrew] for their work which was actively used when creating this project.

<!-- About This Project -->
## About This Project

The purpose of the Brunch framework is to create a generic x86_64 ChromeOS image from an official recovery image. To do so, it uses a 1GB ROOTC partition (containing a custom kernel, an initramfs, the swtpm binaries, userspace patches and config files) and a specific EFI partition to boot from it.

**Warning: Brunch is not the intended way for ChromeOS to work, at some point ChromeOS could potentially become incompatible with Brunch and delete data unexpectedly (even on non-ChromeOS partitions). By installing Brunch you agree to take those risks and I cannot be held responsible for anything bad that would happen to your device including data loss.
It is therefore highly recommended to only use this framework on a device which does not contain any sensitive data and to keep data synced with a cloud service.**

<!-- Supported Hardware -->
## Supported Hardware

Hardware support is highly dependent on the general Linux kernel hardware compatibility. As such only Linux supported hardware will work and the same specific kernel command line options recommended for your device should be passed through the Grub bootloader (see "Modify the Grub bootloader" section). Some features such as camera, microphone and touchpad may not work or may require troubleshooting to get working.


✔ Base Requirements:
- x86_64 based computer with UEFI boot support.
- Administrative privileges on the device.
- An entry level understanding of the linux terminal.
  - This guide aims to make this process as easy as possible, but knowing the basics is expected.


✔ CPU Compatibility:
- [Intel CPUs][intel-cpus] from 8th Gen / [Celeron CPUs][celeron-cpus] from Goldmont
- [AMD Ryzen][amd-ry-list]


❌ Unsupported Hardware:
- Older Intel/AMD CPUs are not supported.
- dGPUs are not supported.
- Virtual Machines are not supported.
- ARM CPUs are not supported.


## Install Instructions
This guide has been split into seperate sections, please follow one of the links below for a guide suitable to your current operating system.

### New: Simplified install with [linuxloops][linuxloops]

Linuxloops is a tool that allows the installation of Brunch with a GUI.

First, identify the recovery image suitable for your CPU:  
#### Intel
* 8th gen & 9th gen: "[shyvana][recovery-shyvana]" for Intel / "[bobba][recovery-bobba]" for Celeron.
* 10th gen: "[jinlon][recovery-jinlon]".
* 11th gen & above: "[voxel][recovery-voxel]".
#### AMD
* Ryzen: "[gumboz][recovery-gumboz]".
  
Once you have identified the recovery image suitable for your CPU, follow the instructions in the [linuxloops][linuxloops] repository Readme.

### Manual install instructions

### [![Install with Linux][linux-img]][linux-guide]  [Install with Linux][linux-guide]
### [![Install with Windows][windows-img]][windows-guide]  [Install with Windows][windows-guide]

## Troubleshooting and Support

In case you run into issues while installing or using Brunch, you can find support on Discord:

[![Discord][discord-shield]][discord-url]

Additional troubleshooting and support tips can be found at the following page:

### [![Troubleshooting][decon-icon-24]][troubleshooting-and-faqs]  [Troubleshooting and Support][troubleshooting-and-faqs]



<!-- Reference Links -->
<!-- Badges -->
[license-shield]: https://img.shields.io/github/license/sebanc/brunch?label=License&logo=Github&style=flat-square
[license-url]: ./LICENSE
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
[croissant]: https://github.com/imperador/chromefy
[swtpm]: https://github.com/stefanberger/swtpm
[linux-surface]: https://github.com/linux-surface/linux-surface
[chromebrew]: https://github.com/skycocker/chromebrew
[celeron-cpus]: https://en.wikipedia.org/wiki/List_of_Intel_Celeron_processors
[intel-cpus]: https://en.wikipedia.org/wiki/Intel_Core
[intel-list]: https://en.wikipedia.org/wiki/List_of_Intel_CPU_microarchitectures
[atom-cpus]: https://en.wikipedia.org/wiki/Intel_Atom
[atom-list]: https://en.wikipedia.org/wiki/List_of_Intel_Atom_microprocessors
[amd-sr-list]: https://en.wikipedia.org/wiki/List_of_AMD_accelerated_processing_units#%22Stoney_Ridge%22_(2016)
[amd-ry-list]: https://en.wikipedia.org/wiki/List_of_AMD_Ryzen_processors
[recovery-shyvana]: https://cros.tech/device/shyvana
[recovery-jinlon]: https://cros.tech/device/jinlon
[recovery-voxel]: https://cros.tech/device/voxel
[recovery-gumboz]: https://cros.tech/device/gumboz
[cros-tech]: https://cros.tech/
[cros-official]: https://cros-updates-serving.appspot.com/
[vboot-utils]: https://aur.archlinux.org/packages/vboot-utils
[auto-brightness]: https://chromium.googlesource.com/chromiumos/platform2/+/master/power_manager/docs/screen_brightness.md
[brunch-toolkit]: https://github.com/WesBosch/brunch-toolkit
[bite-dasher]: https://github.com/BiteDasher/brcr-update

<!-- Images -->
[decon-icon-24]: ./Images/decon_icon-24.png
[decon-icon-512]: ./Images/decon_icon-512.png
[terminal-icon-24]: ./Images/terminal_icon-24.png
[terminal-icon-512]: ./Images/terminal_icon-512.png
[settings-icon-512]: ./Images/settings_icon-512.png
[windows-img]: https://img.icons8.com/color/24/000000/windows-10.png
[linux-img]: https://img.icons8.com/color/24/000000/linux--v1.png

<!-- Internal Links -->
[linux-guide]: ./Readme/install-with-linux.md
[windows-guide]: ./Readme/install-with-windows.md
[troubleshooting-and-faqs]: ./Readme/troubleshooting-and-faqs.md
[compatibility]: ./README.md#supported-hardware
[changing-kernels]: ./Readme/troubleshooting-and-faqs.md#kernels
[framework-options]: ./Readme/troubleshooting-and-faqs.md#framework-options
[releases-tab]: https://github.com/sebanc/brunch/releases
[latest-release]: https://github.com/sebanc/brunch/releases/latest
[brunch-der]: https://github.com/sebanc/brunch/raw/master/brunch.der
[secure-boot]: ./Readme/install-with-linux.md#secure-boot

