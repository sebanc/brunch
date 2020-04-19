ret=0
cat >/system/usr/sbin/chromeos-update <<UPDATE
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ \$(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi

usage()
{
	echo ""
	echo "Brunch updater: update chromeos or brunch in the running environment."
	echo "Usage: chromeos-update -f (Brunch release archive) -r (ChromeOS recovery image)"
	echo "-f (Brunch release archive), --framework (Brunch release archive)"
	echo "-r (ChromeOS recovery image), --recovery (ChromeOS recovery image)"
	echo "-h, --help					Display this menu"
}

while [ \$# -gt 0 ]; do
	case "\$1" in
		-r | --recovery)
		shift
		if [ ! -f "\$1" ]; then echo "Chromeos recovery image \$1 not found"; exit 1; fi
		if [ \$(cgpt show -i 12 -b "\$1") -eq 0 ] || [ \$(cgpt show -i 13 -b "\$1") -gt 0 ] || [ ! \$(cgpt show -i 3 -l "\$1") == 'ROOT-A' ]; then
			echo "\$1 is not a valid Chromeos image"
			exit 1
		fi
		recovery="\$1"
		;;
		-f | --framework)
		shift
		if [ ! -f "\$1" ]; then echo "Brunch release archive not found"; exit 1; fi
		tar -tf "\$1" | grep rootc.img
		if [ ! "\$?" -eq 0 ]; then
			echo "\$1 is not a valid Brunch release archive"
			exit 1
		fi
		framework="\$1"
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

destination=\$(rootdev -d)
if (expr match "\$destination" ".*[0-9]\$" >/dev/null); then
	partition="\$destination"p
else
	partition="\$destination"
fi

if [[ ! -z \$framework ]]; then
	tar zxvf "\$framework" -C ~ rootc.img
	pv ~/rootc.img > "\$partition"7
	rm ~/rootc.img
	echo "Brunch updated."
fi

if [[ ! -z \$recovery ]]; then
	loopdevice=\$(losetup --show -fP "\$recovery")
	pv "\$loopdevice"p3 > "\$partition"5
	losetup -d "\$loopdevice"
	cgpt add -i 2 -S 0 -T 15 -P 0 "\$destination"
	cgpt add -i 4 -S 0 -T 15 -P 15 "\$destination"
	echo "ChromeOS updated."
fi
UPDATE
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /system/usr/sbin/chromeos-update
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
