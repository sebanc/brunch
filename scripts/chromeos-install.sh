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
	echo "-l, --legacy_boot					Use legacy efi partition (no secure boot support)"
	echo "-h, --help					Display this menu"
}

blocksize()
{
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

numsectors()
{
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
    bytes="$(ls -nl "${path}" | xargs | cut -d' ' -f5)"
    local rem=$(( bytes % 512 ))
    block_size=512
    sectors=$(( bytes / 512 ))
    if [ "${rem}" -ne 0 ]; then
      sectors=$(( sectors + 1 ))
    fi
  fi

  echo $(( sectors * 512 / block_size ))
}

write_base_table()
{
  local target="$1"
  local blocks
  block_size=$(blocksize "${target}")
  numsecs=$(numsectors "${target}")
  local curr=32768
  if [ $(( 0 & (block_size - 1) )) -gt 0 ]; then
    echo "Primary Entry Array padding is not block aligned." >&2
    exit 1
  fi
  cgpt create -p $(( 0 / block_size )) "${target}" 2> /dev/null
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

check_args()
{
if [ -z "$source" ]  || [ -z "$destination" ]; then
	echo "At least the source and destination parameters should be provided."
	usage
	exit 1
fi
}

error()
{
if [ ! -z "$zenity" ]; then
	zenity --height=480 --width=640 --error --text="Could not write partition $1, verify that you are not trying to install ChromeOS on a disk with a running operating system or reboot your computer and try again."
	exit 1
else
	cat <<ERROR
Could not write partition $1, verify that you are not trying to install ChromeOS on a disk with a running operating system or reboot your computer and try again.
ERROR
	exit 1
fi
}

install_singleboot()
{
local image
local source_start
local destination_start
local size
image="$source"
if [ ! -z "$zenity" ]; then
	write_base_table "$destination" > /dev/null >(zenity --height=480 --width=640 --title="Brunch installer" --progress --auto-close --pulsate --text="Creating partition table..." --percentage=100)
else
	echo "Creating partition table..."
	write_base_table "$destination"
fi
for (( i=1; i<=12; i++ )); do
	case $i in
		2)
			source_start=$(cgpt show -i 4 -b "$source")
			size=$(cgpt show -i 4 -s "$source")
		;;
		5)
			source_start=$(cgpt show -i 3 -b "$source")
			size=$(cgpt show -i 3 -s "$source")
		;;
		7)
			source_start=0
			image="$(dirname $0)/rootc.img"
			size=$(du -B 512 $image | sed 's/\t.*//g')
		;;
		9|10|11)
			continue
		;;
		12)
			source_start=0
			if [ -z legacy_boot ]; then image="$(dirname $0)/efi_secure.img"; else image="$(dirname $0)/efi_legacy.img"; fi
			size=$(du -B 512 $image | sed 's/\t.*//g')
		;;
		*)
			source_start=$(cgpt show -i $i -b "$source")
			size=$(cgpt show -i $i -s "$source")
		;;
	esac
	destination_start=$(cgpt show -i $i -b "$destination")
	if [ ! -z "$zenity" ]; then
		dd if="$image" ibs=512 count="$size" skip="$source_start" 2> /dev/null | pv -n -s $(( $size * 512 )) 2> >(zenity --height=480 --width=640 --title="Brunch installer" --progress --auto-close --text="Writing partition $i..." --percentage=0 --no-cancel) | dd of="$destination" obs=512 seek="$destination_start" conv=notrunc 2> /dev/null || error $i
	else
		dd if="$image" ibs=512 count="$size" skip="$source_start" 2> /dev/null | pv -s $(( $size * 512 )) | dd of="$destination" obs=512 seek="$destination_start" conv=notrunc 2> /dev/null || error $i
	fi
done
}

singleboot()
{
if [ ! -z "$zenity" ]; then
	local t
	local test
	local device
	local size
	t=0
	for i in $(lsblk -drnbpf -o NAME,SIZE); do
		if [ $((t % 2)) == 0 ]; then device=$i; fi
		if [ $((t % 2)) == 1 ]; then
			size=$((i / 1024 /1024 / 1024))
			if [ "$device" != "$(lsblk -npo pkname $(df /usr | tail -1 | awk '{print $1;}'))" ] && [ $((size - 14)) -gt 0 ]; then test="$test radio $device $size"; fi
		fi
		t=$((t + 1))
	done
	dev=$(zenity --height=480 --width=640 --title="Brunch installer" --list --radiolist --text "Select the drive that you want to use for installation." --column "Select" --column "Device" --column "Size (in GB)" $test --ok-label="Next")
	if [ -z "$dev" ]; then exit 1; else destination="$dev"; fi
	check_args
	if [ ! -b "$destination" ] || [ ! -d /sys/block/"${destination#/dev/}" ]; then zenity --height=480 --width=640 --title="Brunch installer" --error --text="$destination is not a valid disk name"; exit 1; fi
	if [ $(blockdev --getsz "$destination") -lt 29360128 ]; then zenity --height=480 --width=640 --title="Brunch installer" --error --text="Not enough space on device $destination"; exit 1; fi
	if ! zenity --height=480 --width=640 --title="Brunch installer" --question --text "All data on device $destination will be lost, are you sure that you want to continue?"; then exit 1; fi
else
	check_args
	if [ ! -b "$destination" ] || [ ! -d /sys/block/"${destination#/dev/}" ]; then echo "$destination is not a valid disk name"; exit 1; fi
	if [ $(blockdev --getsz "$destination") -lt 29360128 ]; then echo "Not enough space on device $destination"; exit 1; fi
	read -p "All data on device $destination will be lost, are you sure ? (type yes to continue)"$'\n' confirm
	if [ -z $confirm ] || [ ! $confirm == "yes" ]; then
		echo "Invalid answer $confirm, exiting"
		exit 0
	fi
fi
umount "$destination"* > /dev/null 2>&1
install_singleboot
if [ ! -z "$zenity" ]; then
zenity --height=480 --width=640 --title="Brunch installer" --info --text="Brunch has been successfully installed on the device $destination." --ok-label="Exit"
else
echo -e "Brunch has been successfully installed on the device $destination."
fi
}

install_dualboot()
{
local source_loop
local source_part
local destination_loop
local size
if [ -z "$zenity" ]; then echo "Creating image file"; fi
dd if=/dev/zero of="$fullpath" bs=1073741824 seek=$image_size count=0 2> /dev/null
if [ ! -z "$zenity" ]; then
	write_base_table "$fullpath" > /dev/null >(zenity --height=480 --width=640 --title="Brunch installer" --progress --auto-close --pulsate --text="Creating partition table..." --percentage=100)
else
	echo "Creating partition table..."
	write_base_table "$fullpath"
fi
source_loop=$(losetup --show -fP "$source")
destination_loop=$(losetup --show -fP "$fullpath")
for (( i=1; i<=12; i++ )); do
	case $i in
		2)
			source_part="$source_loop"p4
			size=$(cgpt show -i 4 -s "$source_loop")
		;;
		5)
			source_part="$source_loop"p3
			size=$(cgpt show -i 3 -s "$source_loop")
		;;
		7)
			source_part="$(dirname $0)/rootc.img"
			size=$(ls -lp --block-size=512 "$source_part" | cut -d" " -f5)
		;;
		9|10|11)
			continue
		;;
		12)
			if [ -z legacy_boot ]; then source_part="$(dirname $0)/efi_secure.img"; else source_part="$(dirname $0)/efi_legacy.img"; fi
			size=$(ls -lp --block-size=512 "$source_part" | cut -d" " -f5)
		;;
		*)
			source_part="$source_loop"p"$i"
			size=$(cgpt show -i "$i" -s "$source_loop")
		;;
	esac
	if [ ! -z "$zenity" ]; then
		dd if="$source_part" ibs=1048576 2> /dev/null | pv -n -s $(( $size * 512 )) 2> >(zenity --height=480 --width=640 --title="Brunch installer" --progress --auto-close --text="Writing partition $i..." --percentage=0 --no-cancel) | dd of="$destination_loop"p"$i" obs=1048576 2> /dev/null || error $i
	else
		dd if="$source_part" ibs=1048576 2> /dev/null | pv -s $(( $size * 512 )) | dd of="$destination_loop"p"$i" obs=1048576 2> /dev/null || error $i
	fi
done
losetup -d $source_loop
losetup -d $destination_loop
}

grub_config()
{
if [ -z "$zenity" ] && [ ! -z "$wsl" ]; then
	read -p "The ChromeOS disk image has been created. If you want to dual boot this disk image with Grub2Win type \"dualboot\" and press ENTER. Otherwise if you want to install this disk image on a usb flashdrive / sdcard just press ENTER:"$'\n' veriftype
	if [ "$veriftype" == "dualboot" ]; then type="Dualboot (create an image)"; else type="Singleboot (install on a disk)"; fi
	echo -e ""
fi
if [ "$type" == "Dualboot (create an image)" ]; then
	if [ ! -z "$wsl" ]; then
		img_uuid=$(su $(getent passwd $SUDO_UID | cut -d: -f1) -c "PATH=$PATH:/mnt/c/Windows/System32 mountvol.exe $(echo ${fullpath:5:1} | tr a-z A-Z): /L | cut -d'{' -f2 | cut -d'}' -f1")
	else
		img_uuid=$(blkid -s PARTUUID -o value "$(df "$fullpath" --output=source | sed 1d)")
	fi
	img_path=$(if [ $(findmnt -n -o TARGET -T "$fullpath") == "/" ]; then echo $(realpath "$fullpath"); else echo $(realpath "$fullpath") | sed "s#$(findmnt -n -o TARGET -T "$fullpath")##g"; fi)
	if [ -z "$wsl" ] && ([ "$(grep -o '^ID=[^,]\+' /etc/os-release | cut -d'=' -f2)" == "ubuntu" ] || [ "$(grep -o '^ID=[^,]\+' /etc/os-release | cut -d'=' -f2)" == "linuxmint" ] || [ "$(grep -o '^ID=[^,]\+' /etc/os-release | cut -d'=' -f2)" == "fedora" ] || [ "$(grep -o '^ID=[^,]\+' /etc/os-release | cut -d'=' -f2)" == "zorin" ]); then remove_tpm="\n	rmmod tpm"; fi
	config="menuentry \"Brunch\" --class \"brunch\" {$remove_tpm
	img_path="$img_path"
	img_uuid="$img_uuid"
	search --no-floppy --set=root --file "\$img_path"
	loopback loop "\$img_path"
	source (loop,12)/efi/boot/settings.cfg
	if [ -z \$verbose ] -o [ \$verbose -eq 0 ]; then
		linux (loop,7)\$kernel boot=local noresume noswap loglevel=7 options=\$options chromeos_bootsplash=\$chromeos_bootsplash \$cmdline_params \\
			cros_secure cros_debug img_uuid="\$img_uuid" img_path="\$img_path" \\
			console= vt.global_cursor_default=0 brunch_bootsplash=\$brunch_bootsplash quiet
	else
		linux (loop,7)\$kernel boot=local noresume noswap loglevel=7 options=\$options chromeos_bootsplash=\$chromeos_bootsplash \$cmdline_params \\
			cros_secure cros_debug img_uuid="\$img_uuid" img_path="\$img_path"
	fi
	initrd (loop,7)/lib/firmware/amd-ucode.img (loop,7)/lib/firmware/intel-ucode.img (loop,7)/initramfs.img
}

menuentry \"Brunch settings\" --class \"brunch-settings\" {$remove_tpm
	img_path="$img_path"
	img_uuid="$img_uuid"
	search --no-floppy --set=root --file "\$img_path"
	loopback loop "\$img_path"
	source (loop,12)/efi/boot/settings.cfg
	linux (loop,7)/kernel boot=local noresume noswap loglevel=7 options= chromeos_bootsplash= edit_brunch_config=1 \\
		cros_secure cros_debug img_uuid="\$img_uuid" img_path="\$img_path"
	initrd (loop,7)/lib/firmware/amd-ucode.img (loop,7)/lib/firmware/intel-ucode.img (loop,7)/initramfs.img
}"
	echo -e "$config" > "$fullpath".grub.txt
	if [ ! -z "$wsl" ]; then
		grubinstall="The ChromeOS dual boot disk image has been created and the config needed to boot ChromeOS from Grub2Win has been generated in the file:\n$(echo ${fullpath:5:1} | tr a-z A-Z):\\\\$(echo ${fullpath:7} | sed 's@\/@\\\\@g').grub.txt\n\nNow, install Grub2Win and launch it, click on \"Manage Boot Menu\" -> \"Add a new entry\" -> set \"Type\" as \"Create user section\", open the file $(echo ${fullpath:5:1} | tr a-z A-Z):\\\\$(echo ${fullpath:7} | sed 's@\/@\\\\@g').grub.txt and copy its content in the Grub2Win notepad window, save and close the Grub2Win notepad window then click \"Apply\" and \"OK\"."
		finalise="Please note that ChromeOS will not be bootable and / or stable if you do not perform the below actions (Refer to Windows online resources if needed):\n- Ensure that bitlocker is disabled on the drive which contains the ChromeOS image or disable it.\n- Disable fast startup.\n- Disable hibernation.\n\nOnce done, reboot your computer and select ChromeOS from the Grub2Win menu."
		if [ ! -z "$zenity" ]; then
			zenity --height=480 --width=640 --title="Brunch installer" --info --text="$grubinstall" --ok-label="Next"
			zenity --height=480 --width=640 --title="Brunch installer" --info --text="$finalise" --ok-label="Exit"
		else
			echo -e "$grubinstall"
			echo -e ""
			echo -e "$finalise"
		fi
	else
		if [ "$(grep -o '^ID=[^,]\+' /etc/os-release | cut -d'=' -f2)" == "fedora" ]; then grub="grub2"; else grub="grub"; fi
		grubinstall="The grub config needed to boot ChromeOS has been generated in the file \"$fullpath.grub.txt\".\n\nIf you have a linux distro installed which uses grub as bootloader, run the below command to generate the grub config automatically:\nsudo cat /etc/grub.d/40_custom $fullpath.grub.txt | sudo tee /etc/grub.d/99_brunch; sudo chmod 0755 /etc/grub.d/99_brunch; sudo $grub-mkconfig -o /boot/$grub/grub.cfg\n\nOtherwise, add this grub config (lines between stars) manually to another grub bootloader:\n\n ****************************************************************************************** \n$config\n ****************************************************************************************** \n\nOnce the above actions are completed, you can reboot your computer and start ChromeOS"
		if [ ! -z "$zenity" ]; then
			zenity --height=480 --width=640 --title="Brunch installer" --info --text="$grubinstall" --ok-label="Exit"
		else
			echo -e "$grubinstall"
		fi
	fi
else
	if [ ! -z "$zenity" ]; then
		zenity --height=480 --width=640 --title="Brunch installer" --info --text="The ChromeOS image has been successfully created at:\n$(echo ${fullpath:5:1} | tr a-z A-Z):\\\\$(echo ${fullpath:7} | sed 's@\/@\\\\@g')\n\nYou can now install it on a USB flashdrive using rufus." --ok-label="Exit"
	else
		echo -e "The ChromeOS image has been successfully created at:\n$(echo ${fullpath:5:1} | tr a-z A-Z):\\\\$(echo ${fullpath:7} | sed 's@\/@\\\\@g')\n\nYou can now install it on a USB flashdrive using rufus."
	fi
fi
}

dualboot()
{
if [ ! -z "$zenity" ]; then
	local path
	if  [ -z "$wsl" ]; then
		path=$(su $(getent passwd "$SUDO_UID" | cut -d: -f1) -c "zenity --height=480 --width=640 --file-selection --save --title=\"Select the path to store the ChromeOS disk image\" --file-filter=*.img --filename=\"$(getent passwd $SUDO_UID | cut -d: -f6)/chromeos.img\"")
	else
		path=$(su $(getent passwd "$SUDO_UID" | cut -d: -f1) -c "zenity --height=480 --width=640 --file-selection --save --title=\"Select the path to store the ChromeOS disk image\" --file-filter=*.img --filename=\"/mnt/c/Users/$(echo $(/mnt/c/Windows/System32/cmd.exe /c echo %username% 2> /dev/null) | sed 's/[^a-zA-Z0-9]//g')/chromeos.img\"")
	fi
	if [ -z "$path" ]; then exit 1; else destination="$path"; fi
	check_args
	if [[ "$destination" == *"/"* ]] && ([ -z "$(realpath $destination 2> /dev/null)" ] || [ ! -d "$(echo $(realpath $destination) | sed 's@[^/]*$@@')" ]); then echo "Desination path does not exist, please provide an existing path."; dualboot; return; fi
	rm -rf "$destination"
	if [[ ! $destination == *"/"* ]]; then path="$PWD"; else path="$(realpath "$destination")"; path="$(echo ${path} | sed 's@[^/]*$@@')"; fi
	fullpath="$path/$(basename $destination)"
	if [ ! -z "$wsl" ] && [ ! -z "${path##/mnt/*/*}" ]; then zenity --height=480 --width=640 --error --text="The ChromeOS disk image has to be installed outside of the WSL VM, please specify a path such as /mnt/\&lt;drive letter\&gt;/..."; dualboot; return; fi
	if [ $(( ($(df -k "$path" | sed 's/  */ /g' | tail -1 | cut -d' ' -f4) / 1024 / 1024) - 14 )) -lt 0 ]; then  zenity --height=480 --width=640 --error --text="Not enough space to create image file, the minimum size is 14 GB. Verify that the path you have selected points to a partition with more than 14GB available."; dualboot; fi
	if [ -z "$image_size" ]; then image_size=$(zenity --height=480 --width=640 --title="Brunch installer" --scale --text "This partition has $(( ($(df -k $path | sed 's/  */ /g' | tail -1 | cut -d' ' -f4) / 1024 / 1024) )) GB available.\n how much would you like to allocate for ChromeOS ?\n (Around 10GB will be occupied by system files, the remaining space will be available).\n" --min-value=14 --max-value=$(( ($(df -k --output=avail $path | sed 1d) / 1024 / 1024) )) --value=14 --step 1); fi
	if [ -z "$image_size" ]; then exit 1; fi
else
	check_args
	if [[ "$destination" == *"/"* ]] && ([ -z "$(realpath $destination 2> /dev/null)" ] || [ ! -d "$(echo $(realpath $destination) | sed 's![^/]*$!!')" ]); then echo "Desination path does not exist, please provide an existing path."; exit 1; fi
	rm -rf "$destination"
	if [[ ! "$destination" == *"/"* ]]; then path="$PWD"; else path="$(echo $(realpath $destination) | sed 's![^/]*$!!')"; fi
	fullpath="$path/$(basename $destination)"
	if [ ! -z "$wsl" ] && [ ! -z "${path##/mnt/*}" ]; then echo "The ChromeOS disk image has to be installed outside of the WSL VM, please specify a path such as /mnt/<drive letter>/..."; exit 1; fi
	if [ $(( ($(df -k "$path" | sed 's/  */ /g' | tail -1 | cut -d' ' -f4) / 1024 / 1024) - $image_size )) -lt 0 ]; then echo "Not enough space to create image file, available space is $(( ($(df -k $path | sed 's/  */ /g' | tail -1 | cut -d' ' -f4) / 1024 / 1024) )) GB. If you think that this is incorrect, verify that you have correctly mounted the destination partition or if the partition is in ext4 format that there is no reserved space (cf. https://odzangba.wordpress.com/2010/02/20/how-to-free-reserved-space-on-ext4-partitions)"; exit 1; fi
fi
if [ ! -z "$macos" ]; then
destination="$fullpath"
echo "Creating image file"
dd if=/dev/zero of="$destination" bs=1073741824 seek=$image_size count=0 2> /dev/null
install_singleboot
echo -e "The ChromeOS image has been successfully created at:\n$fullpath\n\nYou can now install it on a USB flashdrive using balenaEtcher."
else
install_dualboot
grub_config
fi
}

if [ ! -f /proc/version ]; then macos=1; else if grep -qi 'Microsoft' /proc/version; then wsl=1; fi; fi
if [ ! -z "$wsl" ] && [ ! -e /dev/loop-control ]; then echo "WSL1 is not supported, please install WSL2 to use this installer."; exit 1; fi
if [ $# -eq 0 ]; then
	if ! which zenity > /dev/null 2>&1 ; then echo "To use the GUI installer you need a Linux environment with GUI apps support (actual Linux distro or Windows 11 WSL) and to install the \"zenity\" package."; usage; exit 1; fi
	zenity=1
	if ! zenity --height=480 --width=640 --title="Brunch installer" --info --text="Welcome to the Brunch installer.\n\nPlease refer to the brunch github (www.github.com/sebanc/brunch) to identify which recovery image is compatible with your laptop and download it.\n\nUnzip the recovery image and press Next to select it with the file browser." --ok-label="Next"; then exit 0; fi
	source=$(su $(getent passwd "$SUDO_UID" | cut -d: -f1) -c "zenity --height=480 --width=640 --file-selection --title=\"Select the ChromeOS recovery image\" --file-filter=*.bin --filename=\"$(echo $PWD)/\"")
	if [ -z "$source" ]; then exit 0; fi
	if [ ! -f "$source" ]; then echo "ChromeOS recovery image $1 not found"; exit 1; fi
	if [ ! $(dd if="$source" bs=1 count=4 2> /dev/null | od -A n -t x1 | sed 's/ //g') == '33c0fa8e' ] || [ $(cgpt show -i 12 -b "$source") -eq 0 ] || [ $(cgpt show -i 13 -b "$source") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$source") == 'ROOT-A' ]; then echo "$source is not a valid ChromeOS recovey image (have you unzipped it ?)"; exit 1; fi
	type=$(zenity --height=480 --width=640 --title "Brunch installer" --list --column "Do you want to install brunch as Singleboot or Dualboot ?" "Singleboot (install on a disk)" "Dualboot (create an image)" --ok-label="Next")
else
	while [ $# -gt 0 ]; do
		case "$1" in
			-src | --source)
			shift
			if [ ! -f "$1" ]; then echo "ChromeOS recovery image $1 not found"; exit 1; fi
			if [ ! $(dd if="$1" bs=1 count=4 2> /dev/null | od -A n -t x1 | sed 's/ //g') == '33c0fa8e' ] || [ $(cgpt show -i 12 -b "$1") -eq 0 ] || [ $(cgpt show -i 13 -b "$1") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$1") == 'ROOT-A' ]; then echo "$1 is not a valid ChromeOS recovey image (have you unzipped it ?)"; exit 1; fi
			source="$1"
			;;
			-dst | --destination)
			shift
			if [ -z "${1##/dev/*}" ]; then type="Singleboot (install on a disk)"; else type="Dualboot (create an image)"; fi
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
	if [ -z "$image_size" ]; then image_size=14; fi
fi
if [ "$type" == "Singleboot (install on a disk)" ]; then
	singleboot
elif [ "$type" == "Dualboot (create an image)" ]; then
	dualboot
fi
