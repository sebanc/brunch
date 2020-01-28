ret=0
cat >/system/usr/sbin/chromeos-install <<INSTALL
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi

usage()
{
	echo ""
	echo "Brunch installer: install chromeos on device or create disk image from the running environment."
	echo "Usage: chromeos_install.sh [-s X] -out destination"
	echo "-dst (destination), --destination (destination)	Device (e.g. /dev/sda) or Disk image file (e.g. chromeos.img)"
	echo "-s (disk image size), --size (disk image size)	Disk image output only: final image size in GB (default=14)"
	echo "-h, --help					Display this menu"
}

source=\$(rootdev -d)
image_size=14
while [ \$# -gt 0 ]; do
	case "\$1" in
		-dst | --destination)
		shift
		if [ -z "\${1##/dev/*}" ]; then
			if [ \$(whoami) != "root" ]; then echo "Please run with this script with sudo to write directly to a device"; exit 1; fi
			device=1
		fi
		destination="\$1"
		;;
		-s | --size)
		shift
		if [ ! -z "\${1##*[!0-9]*}" ] ; then
			if [ \$1 -lt 14 ] ; then
				echo "Disk image size cannot be lower than 14 GB"
				exit 1
			fi
		else
			echo "Provided disk image size is not numeric: \$1"
			exit 1
		fi
		image_size="\$1"
		;;
		-h | --help)
		usage
		 ;;
		*)
		echo "\$1 argument is not valid"
		usage
		exit 1
	esac
	shift
done
if [ -z "\$destination" ]; then
	echo "At least the output parameter should be provided."
	usage
	exit 1
fi

adapt_gpt()
{
	local begin=()
	local size=()
	local type=()
	local label=()
	for (( i=1; i<=12; i++ )); do
		begin[\$i]=\$(cgpt show -i \$i -b "\$1")
		size[\$i]=\$(cgpt show -i \$i -s "\$1")
		type[\$i]=\$(cgpt show -i \$i -t "\$1")
		label[\$i]=\$(cgpt show -i \$i -l "\$1")
	done
	disk_size=\$(blockdev --getsz "\$2")
	dd if=/dev/zero of="\$2" conv=notrunc bs=512 count=64
	dd if=/dev/zero of="\$2" conv=notrunc bs=512 count=64 seek=\$(( disk_size - 64 ))
	cgpt create -p 0 "\$2"
	for (( i=2; i<=12; i++ )); do
		cgpt add -i \$i -b \${begin[\$i]} -s \${size[\$i]} -t \${type[\$i]} -l \${label[\$i]} "\$2"
	done
	cgpt add -i 1 -b \${begin[1]} -s \$(( disk_size - begin[1] - 48 )) -t \${type[1]} -l \${label[1]} "\$2"
	cgpt add -i 2 -S 0 -T 15 -P 15 "\$2"
	cgpt add -i 4 -S 0 -T 15 -P 0 "\$2"
	cgpt add -i 6 -S 0 -T 15 -P 0 "\$2"
	dd bs=512 count=1 if="\$1" of=/tmp/gptmbr.bin
	cgpt boot -p -b /tmp/gptmbr.bin -i 12 "\$2"
	rm /tmp/gptmbr.bin
	cgpt add -i 12 -B 1 "\$2"
}

if [[ \$device = 1 ]]; then
	if [ ! -b \$destination ]; then echo "Device \$destination does not exist"; exit 1; fi
	if [ \$(blockdev --getsz "\$destination") -lt 29360128 ]; then echo "Not enought space on device \$destination"; exit 1; fi
	read -rp "All data on device \$destination will be lost, are you sure ? (type yes to continue) " confirm; if [ -z \$confirm ] || [ ! \$confirm == "yes" ]; then exit 0; fi
	umount "\$destination"*
	adapt_gpt "\$source" "\$destination"
	cgpt show "\$destination"
	partx -u "\$destination"
	if (expr match "\$destination" ".*[0-9]\$" >/dev/null); then
		partdest="\$destination"p
	else
		partdest="\$destination"
	fi
	if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
		partsource="\$source"p
	else
		partsource="\$source"
	fi
	for (( i=1; i<=12; i++ )); do
		case \$i in
			1)
			mkfs.ext4 -F -b 4096 -L "H-STATE" "\$partdest""\$i"
			;;
			*)
			dd if="\$partsource""\$i" ibs=1M status=none | pv | dd of="\$partdest""\$i" obs=1M oflag=direct status=none
			;;
		esac
	done
	echo "ChromeOS installed."
else
	if [ -f "\$destination" ]; then rm "\$destination"; fi
	if [ \$(( (\$(df -k --output=avail "\${destination%/*}" | sed 1d) - ( \$image_size * 1024 * 1024)) )) -lt 0 ]; then echo "Not enought space to create image file"; exit 1; fi
	dd if=/dev/zero of="\$destination" bs=1M count=\$(( \$image_size * 1024 )) status=progress
	if [ ! "\$?" -eq 0 ]; then echo "Could not write image here, try with sudo ?"; rm "\$destination"; exit 1; fi
	adapt_gpt "\$source" "\$destination"
	cgpt show "\$destination"
	if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
		partsource="\$source"p
	else
		partsource="\$source"
	fi
	loopdevice=\$(losetup --show -fP "\$destination")
	sleep 5
	for (( i=1; i<=12; i++ )); do
		case \$i in
			1)
			mkfs.ext4 -F -b 4096 -L "H-STATE" "\$loopdevice"p"\$i"
			;;
			*)
			dd if="\$partsource""\$i" ibs=1M status=none | pv | dd of="\$loopdevice"p"\$i" obs=1M oflag=direct status=none
			;;
		esac
	done
	losetup -d "\$loopdevice"
	echo "ChromeOS installed."
	cat <<GRUB
To boot directly from this image file, add the lines between stars to either:
- A brunch usb flashdrive grub config file (then boot from usb and choose boot from disk image in the menu),
- Or your hard disk grub install if you have one (refer to you distro's online resources).
********************************************************************************
menuentry "ChromeOS (boot from disk image)" {
	img_part=\$(df "\$destination" --output=source  | sed -e /Filesystem/d)
	img_path=\$(echo "\$destination" | sed "s#\$(findmnt -n -o TARGET -T "\$destination")##g")
	search --no-floppy --set=root --file \\\$img_path
	loopback loop \\\$img_path
	linux (loop,gpt7)/kernel boot=local noresume noswap loglevel=7 disablevmx=off \\\\
		cros_secure cros_debug loop.max_part=16 img_part=\\\$img_part img_path=\\\$img_path
	initrd (loop,gpt7)/initramfs.img
}
********************************************************************************
GRUB
fi
INSTALL
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /system/usr/sbin/chromeos-install
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
