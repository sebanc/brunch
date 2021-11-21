# This patch generates a script to install ChromeOS from a running ChromeOS environment

ret=0
cat >/roota/usr/sbin/chromeos-install <<'INSTALL'
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ -z $(which cgpt) ]; then echo "cgpt needs to be installed first"; exit 1; fi
if [ -z $(which pv) ]; then echo "pv needs to be installed first"; exit 1; fi
if [ $(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi

usage()
{
	echo ""
	echo "Brunch installer: install ChromeOS on device or create disk image from the brunch framework."
	echo "Usage: chromeos_install.sh [-s X] [-l] -src chromeos_recovery_image_path -dst destination"
	echo "-src (source), --source (source)			ChromeOS recovery image"
	echo "-dst (destination), --destination (destination)	Device (e.g. /dev/sda) or Disk image file (e.g. chromeos.img)"
	echo "-s (disk image size), --size (disk image size)	Disk image output only: final image size in GB (default=14)"
	echo "-l, --legacy_boot				Use legacy boot efi partition (no secure boot support but allows booting from MBR)"
	echo "-h, --help					Display this menu"
}

blocksize() {
  local path="$1"
  if [ -b "${path}" ]; then
    local dev="${path##*/}"
    local sys="/sys/block/${dev}/queue/logical_block_size"
    if [ -e "${sys}" ]; then
      cat "${sys}"
    else
      local part="${path##*/}"
      local block
      block="$(get_block_dev_from_partition_dev "${path}")"
      block="${block##*/}"
      cat "/sys/block/${block}/${part}/queue/logical_block_size"
    fi
  else
    echo 512
  fi
}

numsectors() {
  local block_size
  local sectors
  local path="$1"

  if [ -b "${path}" ]; then
    local dev="${path##*/}"
    block_size="$(blocksize "${path}")"

    if [ -e "/sys/block/${dev}/size" ]; then
      sectors="$(cat "/sys/block/${dev}/size")"
    else
      part="${path##*/}"
      block="$(get_block_dev_from_partition_dev "${path}")"
      block="${block##*/}"
      sectors="$(cat "/sys/block/${block}/${part}/size")"
    fi
  else
    local bytes
    bytes="$(stat -c%s "${path}")"
    local rem=$(( bytes % 512 ))
    block_size=512
    sectors=$(( bytes / 512 ))
    if [ "${rem}" -ne 0 ]; then
      sectors=$(( sectors + 1 ))
    fi
  fi

  echo $(( sectors * 512 / block_size ))
}

write_base_table() {
  local target="$1"
  local blocks
  block_size=$(blocksize "${target}")
  numsecs=$(numsectors "${target}")
  local curr=32768
  if [ $(( 0 & (block_size - 1) )) -gt 0 ]; then
    echo "Primary Entry Array padding is not block aligned." >&2
    exit 1
  fi
  cgpt create -p $(( 0 / block_size )) "${target}"
  blocks=$(( 8388608 / block_size ))
  if [ $(( 8388608 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 11 -b $(( curr / block_size )) -s ${blocks} -t firmware     -l "RWFW" "${target}"
  : $(( curr += blocks * block_size ))
  blocks=$(( 1 / block_size ))
  if [ $(( 1 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 6 -b $(( curr / block_size )) -s ${blocks} -t kernel     -l "KERN-C" "${target}"
  : $(( curr += blocks * block_size ))
  if [ $(( curr % 4096 )) -gt 0 ]; then
    : $(( curr += 4096 - curr % 4096 ))
  fi
  blocks=$(( 1073741824 / block_size ))
  if [ $(( 1073741824 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 7 -b $(( curr / block_size )) -s ${blocks} -t rootfs     -l "ROOT-C" "${target}"
  : $(( curr += blocks * block_size ))
  blocks=$(( 1 / block_size ))
  if [ $(( 1 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 9 -b $(( curr / block_size )) -s ${blocks} -t reserved     -l "reserved" "${target}"
  : $(( curr += blocks * block_size ))
  blocks=$(( 1 / block_size ))
  if [ $(( 1 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 10 -b $(( curr / block_size )) -s ${blocks} -t reserved     -l "reserved" "${target}"
  : $(( curr += blocks * block_size ))
  blocks=$(( 2062336 / block_size ))
  if [ $(( 2062336 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  : $(( curr += blocks * block_size ))
  blocks=$(( 33554432 / block_size ))
  if [ $(( 33554432 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 2 -b $(( curr / block_size )) -s ${blocks} -t kernel     -l "KERN-A" "${target}"
  : $(( curr += blocks * block_size ))
  blocks=$(( 33554432 / block_size ))
  if [ $(( 33554432 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 4 -b $(( curr / block_size )) -s ${blocks} -t kernel     -l "KERN-B" "${target}"
  : $(( curr += blocks * block_size ))
  if [ $(( curr % 4096 )) -gt 0 ]; then
    : $(( curr += 4096 - curr % 4096 ))
  fi
  blocks=$(( 16777216 / block_size ))
  if [ $(( 16777216 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 8 -b $(( curr / block_size )) -s ${blocks} -t data     -l "OEM" "${target}"
  : $(( curr += blocks * block_size ))
  blocks=$(( 67108864 / block_size ))
  if [ $(( 67108864 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  : $(( curr += blocks * block_size ))
  blocks=$(( 33554432 / block_size ))
  if [ $(( 33554432 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 12 -b $(( curr / block_size )) -s ${blocks} -t efi     -l "EFI-SYSTEM" "${target}"
  : $(( curr += blocks * block_size ))
  if [ $(( curr % 4096 )) -gt 0 ]; then
    : $(( curr += 4096 - curr % 4096 ))
  fi
  blocks=$(( 4294967296 / block_size ))
  if [ $(( 4294967296 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 5 -b $(( curr / block_size )) -s ${blocks} -t rootfs     -l "ROOT-B" "${target}"
  : $(( curr += blocks * block_size ))
  if [ $(( curr % 4096 )) -gt 0 ]; then
    : $(( curr += 4096 - curr % 4096 ))
  fi
  blocks=$(( 4294967296 / block_size ))
  if [ $(( 4294967296 % block_size )) -gt 0 ]; then
     : $(( blocks += 1 ))
  fi
  cgpt add -i 3 -b $(( curr / block_size )) -s ${blocks} -t rootfs     -l "ROOT-A" "${target}"
  : $(( curr += blocks * block_size ))
  if [ $(( curr % 4096 )) -gt 0 ]; then
    : $(( curr += 4096 - curr % 4096 ))
  fi
  blocks=$(( numsecs - (curr + 24576) / block_size ))
  cgpt add -i 1 -b $(( curr / block_size )) -s ${blocks} -t data     -l "STATE" "${target}"
  cgpt add -i 2 -S 0 -T 15 -P 15 "${target}"
  cgpt add -i 4 -S 0 -T 15 -P 0 "${target}"
  cgpt add -i 6 -S 0 -T 15 -P 0 "${target}"
  cgpt boot -p -i 12 "${target}"
  cgpt add -i 12 -B 0 "${target}"
  cgpt show "${target}"
}

image_size=14
while [ $# -gt 0 ]; do
	case "$1" in
		-src | --source)
		shift
		if [ ! -f "$1" ]; then echo "ChromeOS recovery image $1 not found"; exit 1; fi
		if [ ! $(dd if="$1" bs=1 count=4 status=none | od -A n -t x1 | sed 's/ //g') == '33c0fa8e' ] || [ $(cgpt show -i 12 -b "$1") -eq 0 ] || [ $(cgpt show -i 13 -b "$1") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$1") == 'ROOT-A' ]; then
			echo "$1 is not a valid ChromeOS recovey image (have you unzipped it ?)"
			exit 1
		fi
		source="$1"
		;;
		-dst | --destination)
		shift
		if [ -z "${1##/dev/*}" ]; then device=1; fi
		destination="$1"
		;;
		-s | --size)
		shift
		if [ ! -z "${1##*[!0-9]*}" ] ; then
			if [ $1 -lt 14 ] ; then
				echo "Disk image size cannot be lower than 14 GB"
				exit 1
			fi
		else
			echo "Provided disk image size is not numeric: $1"
			exit 1
		fi
		image_size=$1
		;;
		-l | --legacy_boot)
		legacy_boot=1
		;;
		-h | --help)
		usage
		exit 0
		;;
		*)
		echo "$1 argument is not valid"
		usage
		exit 1
	esac
	shift
done

if [ -z "$destination" ]; then
	echo "At least the destination parameter should be provided."
	usage
	exit 1
elif [ -z "$source" ]; then
	source=$(rootdev -d)
fi

error()
{
	cat <<ERROR
Could not write partition $1, most likely you are trying to install ChromeOS on a disk with a running operating system.
ERROR
	exit 1
}

copy_partition()
{
	local image="$1"
	case $3 in
		2)
			source_start=$(cgpt show -i 4 -b "$1")
			size=$(cgpt show -i 4 -s "$1")
		;;
		5)
			source_start=$(cgpt show -i 3 -b "$1")
			size=$(cgpt show -i 3 -s "$1")
		;;
		7)
			source_start=0
			image="$(dirname $0)/rootc.img"
			size=$(ls -lp --block-size=512 $image | cut -d" " -f5)
		;;
		12)
			source_start=0
			if [ -z legacy_boot ]; then image="$(dirname $0)/efi_secure.img"; else image="$(dirname $0)/efi_legacy.img"; fi
			size=$(ls -lp --block-size=512 $image | cut -d" " -f5)
		;;
		*)
			source_start=$(cgpt show -i $3 -b "$1")
			size=$(cgpt show -i $3 -s "$1")
		;;
	esac
	destination_start=$(cgpt show -i $3 -b "$2")
	dd if="$image" ibs=512 count="$size" skip="$source_start" status=none | pv | dd of="$2" obs=512 seek="$destination_start" conv=notrunc status=none || error $dest_part
}

if [ -z "$device" ]; then
	rm -rf "$destination"
	if [[ ! $destination == *"/"* ]]; then path="."; else path="$(realpath "$destination")"; path="${path%/*}"; fi
	if grep -qi 'Microsoft' /proc/version && [ ! -z "${path##/mnt/*}" ]; then echo "The ChromeOS disk image has to be installed outside of the linux VM, please specify a path such as /mnt/c/..."; exit 1; fi
	if [ $(( ($(df -k --output=avail "$path" | sed 1d) / 1024 / 1024) - $image_size )) -lt 0 ]; then echo "Not enough space to create image file, available space is $(( ($(df -k --output=avail $path | sed 1d) / 1024 / 1024) )) GB. If you think that this is incorrect, verify that you have correctly mounted the destination partition or if the partition is in ext4 format that there is no reserved space (cf. https://odzangba.wordpress.com/2010/02/20/how-to-free-reserved-space-on-ext4-partitions)"; exit 1; fi
	echo "Creating image file"
	dd if=/dev/zero of="$destination" bs=1G seek=$image_size count=0
else
	if [ ! -b "$destination" ] || [ ! -d /sys/block/"${destination#/dev/}" ]; then echo "$destination is not a valid disk name"; exit 1; fi
	if [ $(blockdev --getsz "$destination") -lt 29360128 ]; then echo "Not enough space on device $destination"; exit 1; fi
	read -rp "All data on device $destination will be lost, are you sure ? (type yes to continue) " confirm
	if [ -z $confirm ] || [ ! $confirm == "yes" ]; then
		echo "Invalid answer $confirm, exiting"
		exit 0
	fi
	umount "$destination"*
fi

write_base_table "$destination"
for (( i=1; i<=12; i++ )); do
	echo "Writing partition $i"
	copy_partition "$source" "$destination" "$i"
done

if [ -z "$device" ]; then
	echo -e "\nChromeOS disk image created.\n"
if grep -qi 'Microsoft' /proc/version; then
	read -rp "If you want to dual boot this disk image with Grub2Win type \"dualboot\" and press ENTER. Otherwise if you want to burn this disk image on a usb flashdrive / sdcard just press ENTER. " dualboot
	if [ -z $dualboot ] || [ ! $dualboot == "dualboot" ]; then exit 0; fi
	read -rp "Open a Windows command prompt and run \"mountvol $(df "$destination" --output=source | sed 1d) /L\" to get the uuid from the partition where brunch is installed, paste it here (only the figures and dashes like: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX) and press ENTER to continue. " uuid
	echo -e "\n"
	cat <<GRUB | tee "$destination".grub.txt 1> /dev/null
This file contains the custom code (lines between stars) needed to boot ChromeOS from Grub2Win.
********************************************************************************
img_uuid=$uuid
img_path=$(if [ $(findmnt -n -o TARGET -T "$destination") == "/" ]; then echo $(realpath "$destination"); else echo $(realpath "$destination") | sed "s#$(findmnt -n -o TARGET -T "$destination")##g"; fi)

menuentry "ChromeOS" --class "brunch" {
	search --no-floppy --set=root --file \$img_path
	loopback loop \$img_path
	source (loop,12)/efi/boot/settings.cfg
	if [ -z \$verbose ] -o [ \$verbose -eq 0 ]; then
		linux (loop,7)\$kernel boot=local noresume noswap loglevel=7 options=\$options chromeos_bootsplash=\$chromeos_bootsplash \$cmdline_params \\
			cros_secure cros_debug loop.max_part=16 img_uuid=\$img_uuid img_path=\$img_path \\
			console= vt.global_cursor_default=0 brunch_bootsplash=\$brunch_bootsplash quiet
	else
		linux (loop,7)\$kernel boot=local noresume noswap loglevel=7 options=\$options chromeos_bootsplash=\$chromeos_bootsplash \$cmdline_params \\
			cros_secure cros_debug loop.max_part=16 img_uuid=\$img_uuid img_path=\$img_path
	fi
	initrd (loop,7)/lib/firmware/amd-ucode.img (loop,7)/lib/firmware/intel-ucode.img (loop,7)/initramfs.img
}

menuentry "ChromeOS (settings)" --class "brunch-settings" {
	search --no-floppy --set=root --file \$img_path
	loopback loop \$img_path
	source (loop,12)/efi/boot/settings.cfg
	linux (loop,7)/kernel boot=local noresume noswap loglevel=7 options= chromeos_bootsplash= edit_brunch_config=1 \\
		cros_secure cros_debug loop.max_part=16 img_uuid=\$img_uuid img_path=\$img_path
	initrd (loop,7)/lib/firmware/amd-ucode.img (loop,7)/lib/firmware/intel-ucode.img (loop,7)/initramfs.img
}
********************************************************************************
GRUB
	cat <<INSTRUCTIONS
The ChromeOS dual boot disk image has been created but there are still important steps to be able to boot from it:
- Ensure that bitlocker is disabled on the drive which contains the ChromeOS image or disable it (refer to online resources)
- Disable fast startup. (refer to online resources)
- Disable hibernation. (refer to online resources)
- Install Grub2Win and launch it, click on "Manage Boot Menu" -> "Add a new entry" -> set "Type" as "Custom code" and "Name" as "ChromeOS" then click on "Edit custom code". Open a new explorer window, navigate to the folder where you installed the disk image and open the text file <disk_image_name>.grub.txt. Copy / Paste the text in the Grub2Win window, click "Apply" and "OK". You can now reboot your computer and start using ChromeOS.
INSTRUCTIONS
else
	cat <<GRUB | tee "$destination".grub.txt
To boot directly from this image file, add the lines between stars to either:
- A brunch usb flashdrive grub config file (then boot from usb and choose boot from disk image in the menu),
- Or your hard disk grub install if you have one (refer to you distro's online resources).
********************************************************************************
img_uuid=$(blkid -s PARTUUID -o value "$(df "$destination" --output=source | sed 1d)")
img_path=$(if [ $(findmnt -n -o TARGET -T "$destination") == "/" ]; then echo $(realpath "$destination"); else echo $(realpath "$destination") | sed "s#$(findmnt -n -o TARGET -T "$destination")##g"; fi)

menuentry "ChromeOS" --class "brunch" {
	rmmod tpm
	search --no-floppy --set=root --file \$img_path
	loopback loop \$img_path
	source (loop,12)/efi/boot/settings.cfg
	if [ -z \$verbose ] -o [ \$verbose -eq 0 ]; then
		linux (loop,7)\$kernel boot=local noresume noswap loglevel=7 options=\$options chromeos_bootsplash=\$chromeos_bootsplash \$cmdline_params \\
			cros_secure cros_debug loop.max_part=16 img_uuid=\$img_uuid img_path=\$img_path \\
			console= vt.global_cursor_default=0 brunch_bootsplash=\$brunch_bootsplash quiet
	else
		linux (loop,7)\$kernel boot=local noresume noswap loglevel=7 options=\$options chromeos_bootsplash=\$chromeos_bootsplash \$cmdline_params \\
			cros_secure cros_debug loop.max_part=16 img_uuid=\$img_uuid img_path=\$img_path
	fi
	initrd (loop,7)/lib/firmware/amd-ucode.img (loop,7)/lib/firmware/intel-ucode.img (loop,7)/initramfs.img
}

menuentry "ChromeOS (settings)" --class "brunch-settings" {
	rmmod tpm
	search --no-floppy --set=root --file \$img_path
	loopback loop \$img_path
	source (loop,12)/efi/boot/settings.cfg
	linux (loop,7)/kernel boot=local noresume noswap loglevel=7 options= chromeos_bootsplash= edit_brunch_config=1 \\
		cros_secure cros_debug loop.max_part=16 img_uuid=\$img_uuid img_path=\$img_path
	initrd (loop,7)/lib/firmware/amd-ucode.img (loop,7)/lib/firmware/intel-ucode.img (loop,7)/initramfs.img
}
********************************************************************************
GRUB
fi
	echo -e "\n"
	if [ -f /etc/lsb-release ]; then
		grep -qi 'Microsoft' /proc/version || ! (grep -qi 'Ubuntu' /etc/lsb-release || grep -qi 'LinuxMint' /etc/lsb-release) || cat <<AUTOGRUB
If you have Ubuntu or Linux Mint installed, you can create the grub config automatically by running the below command:
********************************************************************************
echo -e '#!/bin/sh\nexec tail -n +3 \$0\n\n\n\n'"\$(sed '1,4d;\$d' \$(realpath "$destination").grub.txt)" | sudo tee /etc/grub.d/99_brunch && sudo chmod 0755 /etc/grub.d/99_brunch && sudo update-grub
********************************************************************************
AUTOGRUB
	fi
else
	echo -e "\nChromeOS installed.\n"
fi
INSTALL
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /roota/usr/sbin/chromeos-install
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
