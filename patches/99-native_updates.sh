enable_updates=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "enable_updates" ]; then enable_updates=1; fi
done

ret=0
if [ "$enable_updates" -eq 1 ]; then
	echo "brunch: $0 updates enabled" > /dev/kmsg
	cp /system/bin/chroot /system/bin/chroot.orig
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/system/bin/chroot <<CHROOT
#!/bin/bash
if [ "\$EUID" -eq 0 ] && [ "\$1" == "." ] && [ "\$2" == "/usr/bin/cros_installer" ]; then
exit 0
else
chroot.orig "\$@"
fi
CHROOT
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	touch /system/.nodelta
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
else
	echo "brunch: $0 updates disabled" > /dev/kmsg
	grep -q 'CHROMEOS_AUSERVER=https:\/\/' /system/etc/lsb-release
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	sed -i 's#CHROMEOS_AUSERVER=https:\/\/#&block-#g' /system/etc/lsb-release
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
