# Add the options "ipts_touchscreen" and "ithc_touchscreen" to enable the surface devices touchscreen drivers maintained by linux-surface team

ipts=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "ipts_touchscreen" ] || [ "$i" == "ithc_touchscreen" ]; then ipts=1; fi
done

ret=0
if [ "$ipts" -eq 1 ]; then
	echo "brunch: $0 ipts enabled" > /dev/kmsg
	if [ ! "$(cat /proc/version | cut -d' ' -f3 | cut -c1-4 | sed 's@\.@@g')" -eq 510 ] || [ ! "$(cat /proc/version | cut -d' ' -f3 | cut -c1-4 | sed 's@\.@@g')" -eq 515 ]; then
		cat >/roota/etc/init/ipts.conf <<IPTS
start on stopped udev-trigger
script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ithc.ko
	iptsd
end script
IPTS
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
		tar zxf /rootc/packages/ipts-old.tar.gz -C /roota
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	else
		cat >/roota/etc/init/ipts.conf <<IPTS
start on stopped udev-trigger
script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ithc.ko
	iptsd \$(iptsd-find-hidraw)
end script
IPTS
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
		tar zxf /rootc/packages/ipts-new.tar.gz -C /roota
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
	fi
fi
exit $ret
