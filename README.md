<div id="top"></div>

<!-- Shields/Logos -->
[![License][license-shield]][license-url]
[![Issues][issues-shield]][issues-url]
[![Discord][discord-shield]][discord-url]
[![Reddit][reddit-shield]][reddit-url]
[![Telegram][telegram-shield]][telegram-url]

<!-- Project Logo -->
<p align="center">
  <a href="https://github.com/sebanc/brunch" title="Brunch">
   <img src="./images/decon_icon-512.png" width="128px" alt="Logo"/>
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
It is therefore highly recommended to only use this framework on a device which does not contain any sensitive data and to keep non-sensitive data synced with a cloud service.**

<!-- Supported Hardware -->
## Supported Hardware

Hardware support is highly dependent on the general Linux kernel hardware compatibility. As such only Linux supported hardware will work and the same specific kernel command line options recommended for your device should be passed through the Grub bootloader (see "Modify the Grub bootloader" section). Some features such as camera, microphone and touchpad may not work or may require troubleshooting to get working.


✔ Base Requirements:
- x86_64 based computer with UEFI boot support.
  - MBR/Legacy devices may be supported with the MBR patch
- Administrative privileges on the device.
- An entry level understanding of the linux terminal.
  - This guide aims to make this process as easy as possible, but knowing the basics is expected.


✔ CPU Compatibility:
- [Intel CPUs][intel-cpus] from starting with [Sandy Bridge *or later*][intel-list]
  - [1st generation][intel-list] Intel Core i series were last supported on ChromeOS 81
  - [Atom, Celeron and Pentium][atom-cpus] processors are supported as of [Baytrail *or later*][atom-list]
- [AMD Stoney Ridge][amd-sr-list] or [AMD Ryzen][amd-ry-list] (AMD support is limited)


❌ Unsupported Hardware:
- dGPUs are not supported
- Virtual Machines are not supported
- ARM CPUs are not supported
- Intel Core 2 Duo *and older* are not supported


## Install Instructions
This guide has been split into seperate sections, please follow one of the links below for a guide suitable to your current operating system.

### [![Install with Linux][linux-img]][linux-guide]  [Install with Linux][linux-guide]
### [![Install with Windows][windows-img]][windows-guide]  [Install with Windows][windows-guide]

## Troubleshooting and Support

In case you run into issues while installing or using Brunch, below are the main places where you can find support:

[![Discord][discord-shield]][discord-url]
[![Reddit][reddit-shield]][reddit-url]
[![Telegram][telegram-shield]][telegram-url]

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
[telegram-shield]: https://img.shields.io/badge/Telegram-Join-0088cc?style=flat-square&logo=telegram&logoColor=%23FFFFFF
[telegram-url]: https://t.me/chromeosforpc
[reddit-shield]: https://img.shields.io/badge/Reddit-Join-FF5700?style=flat-square&logo=reddit&logoColor=%23FFFFFF
[reddit-url]: https://www.reddit.com/r/Brunchbook

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
[recovery-rammus]: https://cros.tech/device/rammus
[recovery-volteer]: https://cros.tech/device/volteer
[recovery-grunt]: https://cros.tech/device/grunt
[recovery-zork]: https://cros.tech/device/zork
[cros-tech]: https://cros.tech/
[cros-official]: https://cros-updates-serving.appspot.com/
[vboot-utils]: https://aur.archlinux.org/packages/vboot-utils
[auto-brightness]: https://chromium.googlesource.com/chromiumos/platform2/+/master/power_manager/docs/screen_brightness.md
[brunch-toolkit]: https://github.com/WesBosch/brunch-toolkit
[bite-dasher]: https://github.com/BiteDasher/brcr-update

<!-- Images -->
[decon-icon-24]: ./images/decon_icon-24.png
[decon-icon-512]: ./images/decon_icon-512.png
[terminal-icon-24]: ./images/terminal_icon-24.png
[terminal-icon-512]: ./images/terminal_icon-512.png
[settings-icon-512]: ./images/settings_icon-512.png
[windows-img]: https://img.icons8.com/color/24/000000/windows-10.png
[linux-img]: https://img.icons8.com/color/24/000000/linux--v1.png

<!-- Internal Links -->
[cpu-wiki]: https://github.com/sebanc/brunch/wiki/CPUs-&-Recoveries
[windows-guide]: ./install-with-windows.md
[linux-guide]: ./install-with-linux.md
[troubleshooting-and-faqs]: ./troubleshooting-and-faqs.md
[compatibility]: ./README.md#supported-hardware
[changing-kernels]: ./troubleshooting-and-faqs.md#kernels
[framework-options]: ./troubleshooting-and-faqs.md#framework-options
[releases-tab]: https://github.com/sebanc/brunch/releases
[latest-release]: https://github.com/sebanc/brunch/releases/latest
[mbr-patch]: https://github.com/sebanc/brunch/raw/master/mbr_support.tar.gz
[brunch-der]: https://github.com/sebanc/brunch/raw/master/brunch.der
[secure-boot]: ./install-with-linux.md#secure-boot
[brunch-pwa-info]: https://github.com/sebanc/brunch/wiki/Brunch-PWA-Guide

