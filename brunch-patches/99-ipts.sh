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
if [ "$ithc_touchscreen" -eq 1 ]; then
	echo "brunch: $0 ithc enabled" > /dev/kmsg
	cat >/roota/etc/init/ithc.conf <<ITHC
start on stopped udev-trigger
script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ithc.ko
	sleep 2
	iptsd \$(iptsd-find-hidraw)
end script
ITHC
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/ithc-resume.conf <<ITHCRESUMEFIX
start on stopped udev-trigger
script
	dbus-monitor --system --profile "interface='org.chromium.PowerManager',member='SuspendDone'" | while read -r line; do
		pkill -x iptsd || true
		if lsmod | grep -q '^ithc'; then
			rmmod ithc 2>/dev/null || true
			insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ithc.ko
		fi
		sleep 2
		iptsd \$(iptsd-find-hidraw)
	done
end script
ITHCRESUMEFIX
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	tar zxf /rootc/packages/ipts.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
elif [ "$ipts_touchscreen" -eq 1 ]; then
	echo "brunch: $0 ipts enabled" > /dev/kmsg
	cat >/roota/etc/init/ipts.conf <<IPTS
start on stopped udev-trigger
script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko
	sleep 2
	iptsd \$(iptsd-find-hidraw)
end script
IPTS
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
	cat >/roota/etc/init/ipts-resume.conf <<ITPSRESUMEFIX
start on stopped udev-trigger
script
	dbus-monitor --system --profile "interface='org.chromium.PowerManager',member='SuspendDone'" | while read -r line; do
		pkill -x iptsd || true
		if lsmod | grep -q '^ipts'; then
			rmmod ipts 2>/dev/null || true
			insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko
		fi
		sleep 2
		iptsd \$(iptsd-find-hidraw)
	done
end script
ITPSRESUMEFIX
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
	tar zxf /rootc/packages/ipts.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
fi

exit $ret
