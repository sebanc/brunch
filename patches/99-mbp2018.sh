# Add the option "broadcom_wl" to make use of the broadcom_wl module built-in brunch

mbp2018=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "mbp2018" ]; then mbp2018=1; fi
done

ret=0
if [ "$mbp2018" -eq 1 ]; then
	cat >/roota/etc/init/mbp2018.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe industrialio_triggered_buffer
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/apple-ibridge.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/apple-ib-tb.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/apple-ib-als.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/bce.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
