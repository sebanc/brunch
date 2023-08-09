# Add the options "ipts_touchscreen" and "ithc_touchscreen" to enable the surface devices touchscreen drivers maintained by linux-surface team

ipts_touchscreen=0
ithc_touchscreen=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "ipts_touchscreen" ]; then
		ipts_touchscreen=1
	elif [ "$i" == "ithc_touchscreen" ]; then
		ithc_touchscreen=1
	fi
done

ret=0
if [ "$ipts_touchscreen" -eq 1 ]; then
	echo "brunch: $0 ipts enabled" > /dev/kmsg
	cat >/roota/etc/init/ipts.conf <<IPTS
start on stopped udev-trigger
script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko
	iptsd
end script
IPTS
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	tar zxf /rootc/packages/ipts.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

if [ "$ithc_touchscreen" -eq 1 ]; then
	echo "brunch: $0 ithc enabled" > /dev/kmsg
	cat >/roota/etc/init/ithc.conf <<ITHC
start on stopped udev-trigger
script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ithc.ko
	iptsd \$(iptsd-find-hidraw)
end script
ITHC
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
	tar zxf /rootc/packages/ipts.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi

exit $ret
