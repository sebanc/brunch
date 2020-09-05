# Add the option "rtl8723bu" to make use of the rtl8723bu module built-in brunch

rtl8723bu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8723bu" ]; then rtl8723bu=1; fi
done

ret=0
if [ "$rtl8723bu" -eq 1 ]; then
	echo "brunch: $0 rtl8723bu enabled" > /dev/kmsg
		cat >/system/etc/modprobe.d/rtl8723bu.conf <<MODPROBE
blacklist rtl8xxxu
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/system/etc/init/rtl8723bu.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/rtl8723bu.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
