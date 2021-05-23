# Add the option "rtl8821cu" to make use of the rtl8821cu module built-in brunch

rtl8821cu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8821cu" ]; then rtl8821cu=1; fi
done

ret=0
if [ "$rtl8821cu" -eq 1 ]; then
	echo "brunch: $0 rtl8821cu enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8821cu.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8821cu.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
