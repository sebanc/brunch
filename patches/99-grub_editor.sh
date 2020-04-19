ret=0
cat >/system/usr/sbin/edit-grub-config <<EDITGRUB
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ \$(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi
source=\$(rootdev -d)
if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
	partsource="\$source"p
else
	partsource="\$source"
fi

mkdir -p /root/tmpgrub
mount "\$partsource"12 /root/tmpgrub
nano /root/tmpgrub/efi/boot/grub.cfg
umount /root/tmpgrub
EDITGRUB
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /system/usr/sbin/edit-grub-config
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
