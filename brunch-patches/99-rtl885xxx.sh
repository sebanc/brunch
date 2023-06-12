# Add the option "rtl8852ae" to make use of the rtl8821cu module built-in brunch

rtl8851be=0
rtl8852ae=0
rtl8852be=0
rtl8853ce=0

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8851be" ]; then rtl8851be=1; fi
	if [ "$i" == "rtl8852ae" ]; then rtl8852ae=1; fi
	if [ "$i" == "rtl8852be" ]; then rtl8852be=1; fi
	if [ "$i" == "rtl8853ce" ]; then rtl8853ce=1; fi
done

ret=0

if [ "$rtl8851be" -eq 1 ]; then
	echo "brunch: $0 rtl8851be enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8851be.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89core.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89pci.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852b.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852be.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
elif [ "$rtl8852ae" -eq 1 ]; then
	echo "brunch: $0 rtl8852ae enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8852ae.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89core.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89pci.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852a.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852ae.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
elif [ "$rtl8852be" -eq 1 ]; then
	echo "brunch: $0 rtl8852be enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8852be.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89core.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89pci.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852b.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852be.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
elif [ "$rtl8853ce" -eq 1 ]; then
	echo "brunch: $0 rtl8853ce enabled" > /dev/kmsg
	cat >/roota/etc/init/rtl8853ce.conf <<INSMOD
start on stopped udev-trigger

script
	modprobe mac80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89core.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw89pci.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852c.ko
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/rtl885xxx/rtw_8852ce.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi

exit $ret
