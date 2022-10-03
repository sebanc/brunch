# This patch generates a script to manually update ChromeOS and/or brunch

ret=0
cat >/roota/usr/sbin/chromeos-update <<'UPDATE'
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ $(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi
mkdir -p /mnt/stateful_partition/unencrypted/brunch_updater

usage()
{
	echo ""
	echo "Brunch updater: update ChromeOS or brunch in the running environment."
	echo "Usage: chromeos-update -f (Brunch release archive) -r (ChromeOS recovery image)"
	echo "-f, --framework (Brunch release archive path): Update brunch"
	echo "-e, --efi: Update the efi partition (to be used in addition to -f flag)"
	echo "-r, --recovery (unzipped ChromeOS recovery image path): Update chromeos"
	echo "-h, --help: Display this menu"
}

while [ $# -gt 0 ]; do
	case "$1" in
		-r | --recovery)
		shift
		if [ ! -f "$1" ]; then echo "ChromeOS recovery image $1 not found"; exit 1; fi
		if [ ! $(dd if="$1" bs=1 count=4 status=none | od -A n -t x1 | sed 's/ //g') == '33c0fa8e' ] || [ $(cgpt show -i 12 -b "$1") -eq 0 ] || [ $(cgpt show -i 13 -b "$1") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$1") == 'ROOT-A' ]; then
			echo "$1 is not a valid ChromeOS recovey image (have you unzipped it ?)"
			exit 1
		fi
		recovery="$1"
		;;
		-f | --framework)
		shift
		if [ $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted/brunch_updater | sed 1d) - ( 2 * 1024 * 1024)) )) -lt 0 ]; then echo "Not enough space to update Brunch."; exit 1; fi
		if [ ! -f "$1" ]; then echo "Brunch release archive not found"; exit 1; fi
		if [ ! tar -tf "$1" rootc.img -C /mnt/stateful_partition/unencrypted/brunch_updater >/dev/null 2>&1 ]; then
			echo "$1 is not a valid Brunch release archive"
			exit 1
		fi
		framework="$1"
		;;
		-e | --efi)
		update_efi=1
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

cd /
destination=$(rootdev -d)
if (expr match "$destination" ".*[0-9]$" >/dev/null); then
	partition="$destination"p
else
	partition="$destination"
fi

if [[ ! -z "$framework" ]]; then
	tar zxf "$framework" -C /mnt/stateful_partition/unencrypted/brunch_updater
	pv /mnt/stateful_partition/unencrypted/brunch_updater/rootc.img > "$partition"7
	if [ ! -z $update_efi ]; then pv /mnt/stateful_partition/unencrypted/brunch_updater/efi_secure.img > "$partition"12; fi
	rm -r /mnt/stateful_partition/unencrypted/brunch_updater
	echo "Brunch updated."
fi

if [[ ! -z "$recovery" ]]; then
	loopdevice=$(losetup --show -fP "$recovery")
	pv "$loopdevice"p3 > "$partition"5
	losetup -d "$loopdevice"
	cgpt add -i 2 -S 0 -T 15 -P 0 "$destination"
	cgpt add -i 4 -S 0 -T 15 -P 15 "$destination"
	echo "ChromeOS updated."
fi
UPDATE
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /roota/usr/sbin/chromeos-update
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
