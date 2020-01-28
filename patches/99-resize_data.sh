ret=0
cat >/system/usr/sbin/resize-data <<RESIZEDATA
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ \$(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi
source=\$(rootdev -d)
if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
	partsource="\$source"p
else
	partsource="\$source"
fi
disk_size=\$(blockdev --getsz "\$source")
cgpt add -i 1 -b \$(cgpt show -i 1 -b "\$source") -s \$(( disk_size - \$(cgpt show -i 1 -b "\$source") - 48 )) -t \$(cgpt show -i 1 -t "\$source") -l \$(cgpt show -i 1 -l "\$source") "\$source"
cgpt show "\$source"
partx -u "\$source"
resize2fs -f "\$partsource"1
read -p "System needs to reboot now, press [Enter] key to do so."
reboot
RESIZEDATA
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /system/usr/sbin/resize-data
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
