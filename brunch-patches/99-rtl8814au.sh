# Add the option "rtl8814au" to make use of the rtl8814au module built-in brunch

rtl8814au=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8814au" ]; then rtl8814au=1; fi
done

ret=0
if [ "$rtl8814au" -eq 1 ]; then
	echo "brunch: $0 rtl8814au enabled" > /dev/kmsg
	cat >/roota/etc/modprobe.d/rtl8814au.conf <<MODPROBE
blacklist rtl8xxxu
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/rtl8814au.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8814au.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
