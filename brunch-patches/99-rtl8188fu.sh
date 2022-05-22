# Add the option "rtl8188fu" to make use of the rtl8188fu module built-in brunch

rtl8188fu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8188fu" ]; then rtl8188fu=1; fi
done

ret=0
if [ "$rtl8188fu" -eq 1 ]; then
	echo "brunch: $0 rtl8188fu enabled" > /dev/kmsg
	cat >/roota/etc/modprobe.d/rtl8188fu.conf <<MODPROBE
options rtl8188fu rtw_power_mgnt=0 rtw_enusbss=0
blacklist r8188eu
blacklist rtl8xxxu
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/rtl8188fu.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl8188fu.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
