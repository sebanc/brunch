# Add the option "rtl8821ce" to make use of the rtl8821ce module built-in brunch

rtl8821ce=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8821ce" ]; then rtl8821ce=1; fi
done

ret=0
if [ "$rtl8821ce" -eq 1 ]; then
	echo "brunch: $0 rtl8821ce enabled" > /dev/kmsg
	cat >/system/etc/init/rtl8821ce.conf <<INSMOD
start on stopped udev-trigger

script
	insmod /lib/modules/rtl8821ce.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
