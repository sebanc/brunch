# Add the option "rtl8723bs" to make use of the rtl8723bs module built-in brunch

rtl8723bs=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8723bs" ]; then rtl8723bs=1; fi
done

ret=0
if [ "$rtl8723bs" -eq 1 ]; then
	echo "brunch: $0 rtl8723bs enabled" > /dev/kmsg
	cat >/roota/etc/modprobe.d/rtl8723bs.conf <<MODPROBE
blacklist r8723bs
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/rtl8723bs.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8723bs.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
