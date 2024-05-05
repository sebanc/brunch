<div id="top"></div>

<!-- Shields/Logos -->
[![License][license-shield]][license-url]
[![Issues][issues-shield]][issues-url]
[![Discord][discord-shield]][discord-url]

<!-- Project Logo -->
<p align="center">
  <a href="https://github.com/sebanc/brunch" title="Brunch">
   <img src="../Images/settings_icon-512.png" width="128px" alt="Logo"/>
  </a>
</p>
<h1 align="center">Troubleshooting and Support</h1>
  
# Common Issues
  
  ***
<!-- This *** line creates a divider so that the dropdown looks nice. 
Empty lines between everything in <angle breackets> is intentional due to markdown issues -->
  
  ### The instructions are too difficult to follow!
* ChromeOS is based on linux, most troubleshooting will require some familiarity with basic linux commands. It is strongly suggested that users are comfortable with linux before attempting an installation. If you are strugging with a specific step or would like to suggest changes to the guide, please reach out to our community for assistance.

[![Discord][discord-shield]][discord-url]
  
  ### I followed a video tutorial, now I'm having issues.
  * Video guides are very frequently out of date or use potentially dangerous scripts. For the most up to date information and guides, be sure to read over this github page thouroughly *before* asking for help. 
  
  ### My computer will not boot a Brunch USB, and I've followed all of the instructions correctly!
  * Some devices (notably Surface Go) will not boot a valid USB flash drive / SD card with secure boot on even if the shim binary is signed. For those devices, you will need to disable secure boot in your bios settings and use the legacy EFI bootloader by adding the `-l` parameter when running the chromeos-install.sh script.
  
  ###  The first boot and the ones after a framework change or an update are incredibly long! 
  * Unfortunately, the Brunch framework has to rebuild itself by copying the original rootfs, modules and firmware files after each significant change. The time this process takes depends mostly on your USB flash drive / SD card write speed. You may try with one that has better write speed or use the dual boot method to install it on your HDD.

  ### ChromeOS reboots randomly!
  * This can in theory be due to a lot of things. However, the most likely reason is that your USB flash drive / SD card is too slow. You may try with one that has better write speed or use the dual boot method to install it on your HDD.

  ### Some apps, like Netflix, do not appear on the playstore or don't run properly!
  * In order to have access to the ChromeOS shell, ChromeOS is started in developer mode by default. If you have a stable enough system, you can manually remove `cros_debug` from grub.cfg on the 12th partiton and then do a Powerwash (ChromeOS mechanism which will wipe all your data partition) to disable developer mode. This will remove access to the Crosh shell and certain other features though.
  
  ### Some android apps on the Playstore show as incompatible with my device!
  * Some Playstore apps are not compatible with genuine Chromebooks so this is probably normal. ChromeOS is not Android, so some apps and games are not optimized or avaliable.
  
  ### A new update is avaliable, is it safe to update?
  * ChromeOS updates can be unpredictable, especially on Brunch devices. Even if it's declared safe by other users, you should *always* have backups ready in case there is an issue while updating or if the update has serious bugs on your hardware. 
  
  ### My Touchpad, Touchscreen, Wifi or other hardware is not working properly!
  * ChromeOS is not optimized for every device. Brunch has several avaliable framework options and multiple customized kernels avaliable further down on this page to help with these issues. If you're still having issues, you can reach out to other users on one of our communities for help.

  ### Grub doens't appears on the boot options
  * Some older devices (usually Acer, Asus and Samsung) can't detect Brunch's Grub. To solve this, you can manually assign grub on the BIOS but if your bios doesn't have this option you can use `efibootmgr` to create a boot entry for grub.

[![Discord][discord-shield]][discord-url]

***

# Brunch Configuration Menu

***
  
The Brunch Configuration menu is a new feature avaliable in Brunch 93 and higher, this menu will allow users to set and controll options easily without needing to manually edit files themselves. 

## How to Use

<details>
<summary> Click for Screenshots and Instructions </summary>

***

The Brunch Configuration Menu can be accessed directly from Grub using the "ChromeOS (settings)" boot option or while logged into a TTY using the `sudo edit-brunch-config` command.
  * To access TTY2, press **Ctrl + Alt + F2** and login as `chronos`.
![Crosh][bcm-crosh]
  
 Once you've entered the Brunch Configuration Menu you will be greeted by several pages of options.
  * Use the arrow keys to move the cursor up or down.
  * Use the spacebar to select an option.
    * An empty set [ ] means the option is *not* selected.
    * A filled set [x] means the option *has* been selected.
  * When you're ready to continue, press the Enter key.
    * You won't be able to go back, but this menu can be opened again later.
    * You *do not* need to select something from every page.
</details>
  
## Framework Options
  
<details>
<summary> Click to learn about framework options </summary>
  
*** 
  
The first two pages of the Brunch Configuration Menu are for selecting Framework Options. These options act as patches and can be used to add more features or support to your installation, you can select multiple with the Spacebar, or use the Enter key to continue. Continue scrolling for details about what each option does.
![Framework Options 1][bcm-fo1]
![Framework Options 2][bcm-fo2]

Some device specific options can be enabled through brunch configuration menu:
- "enable_updates": allow native ChromeOS updates (use at your own risk: ChromeOS will be updated but not the Brunch framework/kernel which might render your ChromeOS install unstable or even unbootable),
- "pwa": use this option to enable the brunch PWA:
  - You can install the original one from https://sebanc.github.io/brunch-pwa/ or the ITESaurabh version available at: https://itesaurabh.github.io/brunch-pwa,
- "android_init_fix": alternative init to support devices on which the android container fails to start with the standard init,
- "mount_internal_drives": allows automatic mounting of HDD partitions in ChromeOS:
  - Android media server will scan those drives which will cause high CPU usage until it has finished, it might take hours depending on your data,
  - Partition label will be used if it exists,
- "chromebook_audio": enable audio on EOL chromebook devices using the brunch recommended recovery image,
- "native_chromebook_image": enable it to use brunch on a non-EOL chromebook using its official recovery image, make sure to also select the chromebook kernel version used by your device in ChromeOS,
- "broadcom_wl": enable this option if you need the broadcom_wl module,
- "iwlwifi_backport": enable this option if your intel wireless card is not supported natively in the kernel,
- "rtl8188eu": enable this option if you have a rtl8188eu wireless card/adapter,
- "rtl8188fu": enable this option if you have a rtl8188fu wireless card/adapter,
- "rtl8192eu": enable this option if you have a rtl8192eu wireless card/adapter,
- "rtl8723bu": enable this option if you have a rtl8723bu wireless card/adapter,
- "rtl8723de": enable this option if you have a rtl8723de wireless card/adapter,
- "rtl8723du": enable this option if you have a rtl8723du wireless card/adapter,
- "rtl8812au": enable this option if you have a rtl8812au wireless card/adapter,
- "rtl8814au": enable this option if you have a rtl8814au wireless card/adapter,
- "rtl8821ce": enable this option if you have a rtl8821ce wireless card/adapter,
- "rtl8821cu": enable this option if you have a rtl8821cu wireless card/adapter,
- "rtl88x2bu": enable this option if you have a rtl88x2bu wireless card/adapter,
- "rtbth": enable this option if you have a RT3290/RT3298LE bluetooth device,
- "ipts_touchscreen": enable support for ipts touchscreen (SP4/5/6/7,SB1/2),
- "ithc_touchscreen": enable support for ithc touchscreen (SP7+,SB3 and above),
- "goodix": improve goodix touchscreens support,
- "invert_camera_order": use this option if your camera order is inverted,
- "no_camera_config": if your camera does not work you can try this option which disables the camera config,
- "oled_display": enable this option if you have an oled display (use with kernel 5.10),
- "acpi_power_button": try this option if long pressing the power button does not display the power menu,
- "alt_touchpad_config": try this option if you have touchpad issues,
- "alt_touchpad_config2": another option to try if you have touchpad issues,
- "internal_mic_fix": fix for internal mic on some devices,
- "internal_mic_fix2": alternative fix for internal mic on some devices,
- "sysfs_tablet_mode": allow to control tablet mode from sysfs:
  - `echo 1 | sudo tee /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode` to activate it or use 0 to disable it,
- "force_tablet_mode": same as above except tablet mode is enabled by default on boot,
- "suspend_s3": disable suspend to idle (S0ix) and use S3 suspend instead,
- "advanced_als": [default ChromeOS auto-brightness][auto-brightness] is very basic:
  - This option activates more auto-brightness levels (based on the Pixel Slate implementation).

 </details>

## Kernels
  
<details>
<summary> Click to learn about kernels </summary>
  
*** 

Brunch has several precompiled kernels avaliable for users, you can select one from the Brunch Configuration Menu by highlighting the kernel you want, then pressing enter. If you are unsure which to choose, 5.4 is the default kernel for Brunch. Continue scrolling for more information about each avaliable option.

WARNING: Changing kernel can prevent you from logging into your ChromeOS account, in which case a powerwash is the only solution (**Ctrl + Alt + Shift + R** at the login screen). Therefore, before switching to a different kernel, make sure you have a backup of all your data. 
![Kernels][bcm-kernel]
  
Several kernels can be enabled throught the configuration menu:
- LTS kernels (6.1, 5.15, 5.10, 5.4, 4.19): use those kernels for non-chromebook devices, 6.1 is the default.
- Chromebook kernels (6.1, 5.15, 5.10, 5.4, 4.19): For Chromebook devices, select the same kernel version used by your device in ChromeOS.

 </details>
 

## Kernel command line parameters
  
<details>
<summary> Click to learn about kernel line parameters </summary>
  
*** 

These are optional parameters that are not needed by every user. Some commonly used options are selectable, and more can be input manually. If you do not need any command line parameters, you can just press Enter to skip these sections.
![Command Line Paramters][bcm-cmd1]
![Custom Parameters][bcm-cmd2]
  
The most common kernel command line parameters are listed below:
- "enforce_hyperthreading=1": improve performance by disabling a ChromeOS security feature and forcing hyperthreading everywhere (even in crositini).
- "i915.enable_fbc=0 i915.enable_psr=0": if you want to use crouton (needed with kernel 5.4).
- "psmouse.elantech_smbus=1": fix needed for some elantech touchpads.
- "psmouse.synaptics_intertouch=1": enables gestures with more than 2 fingers on some touchpad models.

Additional kernel parameters can also be added manually from the configuration menu.
  
</details>

## Verbose Mode

<details>
  <summary> Click to learn about Verbose Mode </summary>
  
***
  
Brunch has a Verbose Mode, formerly called Debug Mode. Enabling this boot option will disable any selected Brunch Bootsplash and display a log while booting. This is particularly useful for debugging and solving issues that may prevent your system from booting. To enable Verbose Mode you must enter `yes` at the prompt, then press enter.
![Verbose Mode][bcm-debug]

</details>

## Brunch Bootsplashes
  
<details>
<summary> Click to learn about Brunch Bootsplashes </summary>
  
  ***
  
Brunch Bootsplashes can be selected using the Brunch Configuration Menu, these determine the logo visible while Brunch is booting. (before the ChromeOS logo appears) These bootsplashes *will not appear* if you have enabled Verbose Mode above. You can select any of these options with the Enter key.
![Brunch Bootsplash][bcm-splash]  

<details>
<summary> Click for previews of each avaliable bootsplash option </summary>
    
  ***
  
Currently avaliable bootsplashes are below, the "light" options will show as a framed window while booting.
  
<details>
<summary>   Default </summary>
      
![bs-default1]
![bs-default2]
</details>
    
<details>
<summary>   Blank </summary>
      
![bs-blank]

(What did you expect?)
</details>
    
<details>
<summary>   Brunchbook </summary>
      
![bs-bb1]
![bs-bb2]
</details>
    
<details>
<summary>   Colorful </summary>
      
![bs-color1]
![bs-color2]
</details>
    
<details>
<summary>   Croissant </summary>
      
![bs-croi1]
![bs-croi2]
</details>
    
<details>
<summary>   Neon Blue </summary>
      
![bs-nb1]
![bs-nb2]
</details>
    
<details>
<summary>   Neon Green </summary>
      
![bs-ng1]
![bs-ng2]
</details>
    
<details>
<summary>   Neon Pink </summary>
      
![bs-np1]
![bs-np2]
</details>
    
<details>
<summary>   Neon Red </summary>
      
![bs-nr1]
![bs-nr2]
</details>
    
</details>

</details>

</details>

## Saving Changes

<details>
  <summary> Click for more info </summary>
  
***
  
The last page of the Brunch Configuration Menu will allow you to confirm your changes. Upon hitting enter, your PC will reboot. The next time you boot into Brunch, it *will* be a long boot. (Just like when you first installed) This is normal. If the options you selected were not correct, or you want to change things back, you can reopen the Brunch Configuration Menu and start over.
![Summary][bcm-summary]
  
</details>

  ***

# Updates
  
  ***
  
It is currently recommended to only update ChromeOS when the matching version of the Brunch framework has been released, however it's not a strict requirement that Brunch and ChromeOS be the same version. 

 
 ## How to update Brunch and ChromeOS together
  
<details>
<summary> Click to learn how to update Brunch and ChromeOS together</summary>
  
  ***
 
 The easiest way to update Brunch and ChromeOS is to use the [Brunch PWA][brunch-pwa-info], although it's also possible to update manually.
 
 To manually update Brunch and ChromeOS together: 
 * Download the [latest Brunch release][latest-release]
 * Download the [latest recovery][cros-tech] matching your install and extract the bin.
 * Open a TTY with **Crtl + Alt + F2** and login as `chronos`.
 * Run the built in command to update Brunch.
   * Replace `brunch_archive.tar.gz` with the file's actual filename.
   * Replace `recovery.bin` with the file's actual filename.

```sudo chromeos-update -r ~/Downloads/recovery.bin -f ~/Downloads/brunch_archive.tar.gz```
 * Restart ChromeOS after the update finishes.
  
  Brunch and ChromeOS can also be updated with [BiteDasher's Script][bite-dasher]
  
  </details>
  
  
 ## How to update ChromeOS
  
<details>
<summary> Click to learn how to update ChromeOS </summary>
  
  ***
 
 The easiest way to update ChromeOS is to enable the `enable_updates` framework option, then update directly from the Settings page.
 
 To enable the framework option: 
 * Open a TTY with **Crtl + Alt + F2** and login as `chronos`.
 * Enter `sudo edit-brunch-config` to open the Brunch Configuration Menu described above.
 * Select `enable_updates` with the spacebar, then continue through the menu with the enter key.
 * When finished, the Brunch Configuration Menu will prompt to restart ChromeOS.
 * Update from the standard ChromeOS settings page after rebooting.
  
  </details>
  
  
  
 ## How to update Brunch
  
<details>
<summary> Click to learn how to update Brunch </summary>
  
  ***
 
 The easiest way to update Brunch is to use the [Brunch PWA][brunch-pwa-info], although it's also possible to update manually.
 
 To manually update Brunch: 
 * Download the [latest Brunch release][latest-release]
 * Open a TTY with **Crtl + Alt + F2** and login as `chronos`.
 * Run the built in command to update Brunch.
   * Replace `brunch_archive.tar.gz` with the file's actual filename.

```sudo chromeos-update -f ~/Downloads/brunch_archive.tar.gz```
 * Restart ChromeOS after the update finishes.
  
  </details>

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
[auto-brightness]: https://chromium.googlesource.com/chromiumos/platform2/+/master/power_manager/docs/screen_brightness.md
[brunch-toolkit]: https://github.com/WesBosch/brunch-toolkit
[bite-dasher]: https://github.com/BiteDasher/brcr-update

<!-- Images -->
[decon-icon-24]: ../Images/decon_icon-24.png
[decon-icon-512]: ../Images/decon_icon-512.png
[terminal-icon-24]: ../Images/terminal_icon-24.png
[terminal-icon-512]: ../Images/terminal_icon-512.png
[settings-icon-512]: ../Images/settings_icon-512.png
[windows-img]: https://img.icons8.com/color/24/000000/windows-10.png
[linux-img]: https://img.icons8.com/color/24/000000/linux--v1.png
  
 <!-- Brunch Configuration Menu Examples -->
[bcm-crosh]: ../Images/brunch-config-menu/edit-brunch-config.png
[bcm-fo1]: ../Images/brunch-config-menu/framework-options-1.png
[bcm-fo2]: ../Images/brunch-config-menu/framework-options-2.png
[bcm-kernel]: ../Images/brunch-config-menu/select-kernel.png
[bcm-cmd1]: ../Images/brunch-config-menu/cmd-line-params.png
[bcm-cmd2]: ../Images/brunch-config-menu/custom-params.png
[bcm-debug]: ../Images/brunch-config-menu/verbose-mode.png
[bcm-splash]: ../Images/brunch-config-menu/select-bootsplash.png
[bcm-summary]: ../Images/brunch-config-menu/summary.png

<!-- Brunch Bootsplash Examples -->
[bs-default1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/default_dark/main.png
[bs-default2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/default_light/main.png
[bs-blank]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/blank/main.png
[bs-bb1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/brunchbook_dark/main.png
[bs-bb2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/brunchbook_light/main.png
[bs-color1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/colorful_dark/main.png
[bs-color2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/colorful_light/main.png
[bs-croi1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/croissant_dark/main.png
[bs-croi2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/croissant_light/main.png
[bs-nb1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_blue_dark/main.png
[bs-nb2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_blue_light/main.png
[bs-ng1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_green_dark/main.png
[bs-ng2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_green_light/main.png
[bs-np1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_pink_dark/main.png
[bs-np2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_pink_light/main.png
[bs-nr1]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_red_dark/main.png
[bs-nr2]: https://github.com/sebanc/brunch/blob/r97/bootsplashes/neon_red_light/main.png

<!-- Internal Links -->
[windows-guide]: ./install-with-windows.md
[linux-guide]: ./install-with-linux.md
[troubleshooting-and-faqs]: ./troubleshooting-and-faqs.md
[compatibility]: ../README.md#supported-hardware
[changing-kernels]: ./troubleshooting-and-faqs.md#kernels
[framework-options]: ./troubleshooting-and-faqs.md#framework-options
[releases-tab]: https://github.com/sebanc/brunch/releases
[latest-release]: https://github.com/sebanc/brunch/releases/latest
[brunch-der]: https://github.com/sebanc/brunch/raw/master/brunch.der
[secure-boot]: ./install-with-linux.md#secure-boot
