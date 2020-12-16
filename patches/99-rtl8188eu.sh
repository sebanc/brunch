# Add the option "rtl8188eu" to make use of the rtl8188eu module built-in brunch

rtl8188eu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8188eu" ]; then rtl8188eu=1; fi
done

ret=0
if [ "$rtl8188eu" -eq 1 ]; then
	echo "brunch: $0 rtl8188eu enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8188eu.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8188eu.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
