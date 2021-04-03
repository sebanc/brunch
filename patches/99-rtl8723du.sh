# Add the option "rtl8723du" to make use of the rtl8723du module built-in brunch

rtl8723du=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8723du" ]; then rtl8723du=1; fi
done

ret=0
if [ "$rtl8723du" -eq 1 ]; then
	echo "brunch: $0 rtl8723du enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8723du.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8723du.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
