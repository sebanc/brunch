# ChromeOS updates might not be safe depending on the changes brought by google so disable them by default
# Allow users to manually enable updates by using the "enable_updates" option if they understand and accept the corresponding risks

enable_updates=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "enable_updates" ]; then enable_updates=1; fi
done

ret=0
if [ "$enable_updates" -eq 1 ]; then
	echo "brunch: $0 updates enabled" > /dev/kmsg
	cp /roota/bin/chroot /roota/bin/chroot.orig
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/bin/chroot <<CHROOT
#!/bin/bash
if [ "\$EUID" -eq 0 ] && [ "\$1" == "." ] && [ "\$2" == "/usr/bin/cros_installer" ]; then
exit 0
else
chroot.orig "\$@"
fi
CHROOT
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	touch /roota/.nodelta
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
else
	echo "brunch: $0 updates disabled" > /dev/kmsg
	grep -q 'CHROMEOS_AUSERVER=https:\/\/' /roota/etc/lsb-release
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	sed -i 's#CHROMEOS_AUSERVER=https:\/\/#&block-#g' /roota/etc/lsb-release
	# For some reason sed runs correctly but does not return 0, ignore this check
	#if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
