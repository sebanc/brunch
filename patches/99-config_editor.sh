# This patch generates scripts to facilitate the modification of brunch configuration

ret=0
cp /sbin/brunch-setup /roota/usr/sbin/
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /roota/usr/sbin/brunch-setup
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
cat >/roota/usr/sbin/edit-brunch-config <<EDITCONFIG
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ \$(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi

method="brunch-setup"

while [ \$# -gt 0 ]; do
	case "\$1" in
		-m | --manual)
		shift
		method="nano /mnt/stateful_partition/unencrypted/brunch_config/efi/boot/settings.cfg"
		;;
		-g | --grub)
		shift
		method="nano /mnt/stateful_partition/unencrypted/brunch_config/efi/boot/grub.cfg"
		;;
		*)
		echo "\$1 argument is not valid"
		usage
		exit 1
	esac
	shift
done

source=\$(rootdev -d)
if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
	partsource="\$source"p
else
	partsource="\$source"
fi

mkdir -p /mnt/stateful_partition/unencrypted/brunch_config
mount "\$partsource"12 /mnt/stateful_partition/unencrypted/brunch_config
if [ ! -f /mnt/stateful_partition/unencrypted/brunch_config/efi/boot/settings.cfg ]; then
	if [[ "\$source" =~ .*"sdz".* ]]; then
		umount /mnt/stateful_partition/unencrypted/brunch_config
		cat <<DUALBOOT
┌──────────────────────────────────────────────────────────────┐
│ The grub config cannot be edited from ChromeOS in dual boot. │
│ In dual boot, you have to modify the grub config that was    │
│ created at the end of the image file installation process.   │
└──────────────────────────────────────────────────────────────┘
DUALBOOT
		exit 0
	else
		nano /mnt/stateful_partition/unencrypted/brunch_config/efi/boot/grub.cfg
	fi
else
	if [[ "\$method" == "nano /mnt/stateful_partition/unencrypted/brunch_config/efi/boot/grub.cfg" ]] && [[ "\$source" =~ .*"sdz".* ]]; then
		umount /mnt/stateful_partition/unencrypted/brunch_config
		cat <<DUALBOOT
┌──────────────────────────────────────────────────────────────┐
│ The grub config cannot be edited from ChromeOS in dual boot. │
│ In dual boot, you have to modify the grub config that was    │
│ created at the end of the image file installation process.   │
└──────────────────────────────────────────────────────────────┘
DUALBOOT
		exit 0
	else
		\$method
	fi
fi
umount /mnt/stateful_partition/unencrypted/brunch_config
read -p "Press any key to reboot your computer..."
reboot -f
EDITCONFIG
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
chmod 0755 /roota/usr/sbin/edit-brunch-config
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
ln -s edit-brunch-config /roota/usr/sbin/edit-grub-config
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
exit $ret
