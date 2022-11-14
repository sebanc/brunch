# Add the option "broadcom_wl" to make use of the broadcom_wl module built-in brunch

broadcom_wl=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "broadcom_wl" ]; then broadcom_wl=1; fi
done

ret=0
if [ "$broadcom_wl" -eq 1 ]; then
	echo "brunch: $0 broadcom_wl enabled" > /dev/kmsg
	cat >/roota/etc/modprobe.d/broadcom_wl.conf <<MODPROBE
blacklist b43
blacklist b43legacy
blacklist b44
blacklist ssb
blacklist bcm43xx
blacklist brcm80211
blacklist brcmfmac
blacklist brcmsmac
blacklist bcma
MODPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/broadcom_wl.conf <<INSMOD
start on stopped udev-trigger-early

script
	modprobe cfg80211
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/broadcom_wl.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	tar zxf /rootc/packages/broadcom-wl.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi
exit $ret
