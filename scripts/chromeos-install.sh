#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ -z $(which cgpt) ]; then echo "cgpt needs to be installed first"; exit 1; fi

usage()
{
	echo ""
	echo "Brunch installer: install chromeos on device or create disk image from the brunch framework."
	echo "Usage: chromeos_install.sh [-s X] [-l] -src chromeos_recovery_image -dst destination"
	echo "-src (source), --source (source)			Chromeos recovery image"
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
		if [ ! -f "$1" ]; then echo "Chromeos recovery image $1 not found"; exit 1; fi
		if [ $(cgpt show -i 12 -b "$1") -eq 0 ] || [ $(cgpt show -i 13 -b "$1") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$1") == 'ROOT-A' ]; then
			echo "$1 is not a valid Chromeos recovey image"
			exit 1
		fi
		source="$1"
		;;
		-dst | --destination)
		shift
		if [ -z "${1##/dev/*}" ]; then
			if [ $(whoami) != "root" ]; then echo "Please run with this script with sudo to write directly to a device"; exit 1; fi
			device=1
		fi
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
		 ;;
		*)
		echo "$1 argument is not valid"
		usage
		exit 1
	esac
	shift
done
if [ -z "$source" ]  || [ -z "$destination" ]; then
	echo "At least the input and output parameters should be provided."
	usage
	exit 1
fi

if [[ $device = 1 ]]; then
	if [ ! -b $destination ]; then echo "Device $destination does not exist"; exit 1; fi
	if [ $(blockdev --getsz "$destination") -lt 29360128 ]; then echo "Not enought space on device $destination"; exit 1; fi
	read -rp "All data on device $destination will be lost, are you sure ? (type yes to continue) " confirm; if [ -z $confirm ] || [ ! $confirm == "yes" ]; then exit 0; fi
	umount "$destination"*
	write_base_table "$destination"
	sync
	partx -u "$destination"
	if (expr match "$destination" ".*[0-9]$" >/dev/null); then
		partition="$destination"p
	else
		partition="$destination"
	fi
	loopdevice=$(losetup --show -fP "$source")
	sleep 5
	partx -u "$loopdevice"
	sleep 5
	for (( i=1; i<=12; i++ )); do
		echo "Writing partition $i"
		case $i in
			2)
			pv "$loopdevice"p4 > "$partition"2
			;;
			5)
			pv "$loopdevice"p3 > "$partition"5
			;;
			7)
			pv rootc.img > "$partition"7
			;;
			12)
			if [[ $legacy_boot = 1 ]]; then
				pv efi_legacy.img > "$partition"12
			else
				pv efi_secure.img > "$partition"12
			fi
			;;
			*)
			pv "$loopdevice"p"$i" > "$partition""$i"
			;;
		esac
	done
	losetup -d "$loopdevice"
	echo "ChromeOS installed."
else
	copy_partition()
	{
		source_start=$(cgpt show -i $3 -b "$1")
		size=$(cgpt show -i $3 -s "$1")
		destination_start=$(cgpt show -i $4 -b "$2")
		dd if="$1" ibs=512 count="$size" skip="$source_start" status=none | pv | dd of="$2" obs=512 seek="$destination_start" conv=notrunc status=none
	}
	if [ -f "$destination" ]; then rm "$destination"; fi
	if [[ ! $destination == *"/"* ]]; then path="."; else path="${destination%/*}"; fi
	if [ $(( ($(df -k --output=avail "$path" | sed 1d) - ( $image_size * 1024 * 1024)) )) -lt 0 ]; then echo "Not enought space to create image file"; exit 1; fi
	echo "Creating image file"
	dd if=/dev/zero of="$destination" bs=1G seek=$image_size count=0
	if [ ! "$?" -eq 0 ]; then echo "Could not write image here, try with sudo ?"; rm "$destination"; exit 1; fi
	write_base_table "$destination"
	sync
	for (( i=1; i<=12; i++ )); do
		echo "Writing partition $i"
		case $i in
			2)
			copy_partition "$source" "$destination" 4 $i
			;;
			5)
			copy_partition "$source" "$destination" 3 $i
			;;
			7)
			destination_start=$(cgpt show -i $i -b "$destination")
			dd if="rootc.img" status=none | pv | dd of="$destination" obs=512 seek="$destination_start" conv=notrunc status=none
			;;
			12)
			destination_start=$(cgpt show -i $i -b "$destination")
			if [[ $legacy_boot = 1 ]]; then
				dd if="efi_legacy.img" status=none | pv | dd of="$destination" obs=512 seek="$destination_start" conv=notrunc status=none
			else
				dd if="efi_secure.img" status=none | pv | dd of="$destination" obs=512 seek="$destination_start" conv=notrunc status=none
			fi
			;;
			*)
			copy_partition "$source" "$destination" $i $i
			;;
		esac
	done
	echo "ChromeOS installed."
	grep -q 'Microsoft' /proc/version || cat <<GRUB
To boot directly from this image file, add the lines between stars to either:
- A brunch usb flashdrive grub config file (then boot from usb and choose boot from disk image in the menu),
- Or your hard disk grub install if you have one (refer to you distro's online resources).
********************************************************************************
menuentry "ChromeOS (boot from disk image)" {
	img_part=$(df "$destination" --output=source | sed 1d)
	img_path=$(if [ $(findmnt -n -o TARGET -T "$destination") == "/" ]; then echo $(realpath "$destination"); else echo $(realpath "$destination") | sed "s#$(findmnt -n -o TARGET -T "$destination")##g"; fi)
	search --no-floppy --set=root --file \$img_path
	loopback loop \$img_path
	linux (loop,gpt7)/kernel boot=local noresume noswap loglevel=7 disablevmx=off \\
		cros_secure cros_debug loop.max_part=16 img_part=\$img_part img_path=\$img_path
	initrd (loop,gpt7)/initramfs.img
}
********************************************************************************
GRUB
fi
