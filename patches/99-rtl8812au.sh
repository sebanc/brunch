# Add the option "rtl8812au" to make use of the rtl8812au module built-in brunch

rtl8812au=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8812au" ]; then rtl8812au=1; fi
done

ret=0
if [ "$rtl8812au" -eq 1 ]; then
	echo "brunch: $0 rtl8812au enabled" > /dev/kmsg
	cat >/system/etc/modprobe.d/rtl8812au.conf <<MODPROBE
blacklist rtl8xxxu
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/system/etc/init/rtl8812au.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/rtl8812au.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
