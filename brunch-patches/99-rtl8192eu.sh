# Add the option "rtl8192eu" to make use of the rtl8192eu module built-in brunch

rtl8192eu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8192eu" ]; then rtl8192eu=1; fi
done

ret=0
if [ "$rtl8192eu" -eq 1 ]; then
	echo "brunch: $0 rtl8192eu enabled" > /dev/kmsg
	cat >/roota/etc/modprobe.d/rtl8192eu.conf <<MODPROBE
blacklist rtl8xxxu
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/rtl8192eu.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8192eu.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
