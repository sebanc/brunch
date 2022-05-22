# Add the option "rtl8852ae" to make use of the rtl8821cu module built-in brunch

rtl8852ae=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8852ae" ]; then rtl8852ae=1; fi
done

ret=0
if [ "$rtl8852ae" -eq 1 ]; then
	echo "brunch: $0 rtl8852ae enabled" > /dev/kmsg
	cat >/roota/etc/modprobe.d/rtl8852ae.conf <<MODPROBE
blacklist rtl8xxxu
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/rtl8852ae.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8852ae.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
