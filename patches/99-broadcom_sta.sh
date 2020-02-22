broadcom_sta=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "broadcom_sta" ]; then broadcom_sta=1; fi
done

ret=0
if [ "$broadcom_sta" -eq 1 ]; then
	echo "brunch: $0 broadcom_sta enabled" > /dev/kmsg
	cat >/system/etc/modprobe.d/broadcom_sta.conf <<MODPROBE
blacklist ssb
blacklist bcma
blacklist b43
blacklist brcmsmac
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/system/etc/init/broadcom_sta.conf <<INSMOD
start on stopped udev-trigger

script
	insmod /lib/modules/broadcom_sta.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
