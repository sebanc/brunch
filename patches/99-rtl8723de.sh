# Add the option "rtl8723de" to make use of the rtl8723de module built-in brunch

rtl8723de=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtl8723de" ]; then rtl8723de=1; fi
done

ret=0
if [ "$rtl8723de" -eq 1 ]; then
	echo "brunch: $0 rtl8723de enabled" > /dev/kmsg
	cat >/system/etc/init/rtl8723de.conf <<INSMOD
start on stopped udev-trigger

script
	insmod /lib/modules/rtl8723de.ko
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
