ret=0
if [ -f /system/etc/camera/camera_characteristics.conf ]; then
	rm /system/etc/camera/camera_characteristics.conf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
