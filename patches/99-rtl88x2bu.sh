# Add the option "rtl88x2bu" to make use of the rtl88x2bu module built-in brunch

rtl88x2bu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl88x2bu" ]; then rtl88x2bu=1; fi
done

ret=0
if [ "$rtl88x2bu" -eq 1 ]; then
	echo "brunch: $0 rtl88x2bu enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl88x2bu.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl88x2bu.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
