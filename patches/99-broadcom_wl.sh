broadcom_wl=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "broadcom_wl" ]; then broadcom_wl=1; fi
done

ret=0
if [ "$broadcom_wl" -eq 1 ]; then
	echo "brunch: $0 broadcom_wl enabled" > /dev/kmsg
	cat >/system/etc/modprobe.d/broadcom_wl.conf <<MODPROBE
blacklist ssb
blacklist bcma
blacklist b43
blacklist brcmsmac
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/system/etc/init/broadcom_wl.conf <<INSMOD
start on stopped udev-trigger

script
	insmod /lib/modules/broadcom_wl.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
