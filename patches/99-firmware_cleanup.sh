ret=0
if [ -f /system/etc/bluetooth/main.conf ]; then
	rm /system/etc/bluetooth/main.conf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
if [ -d /system/etc/cras ]; then
	rm -r /system/etc/cras
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
if [ -d /system/etc/dptf ]; then
	rm -r /system/etc/dptf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi
if [ -d /system/opt/google/cr50 ]; then
	rm -r /system/opt/google/cr50
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi
if [ -d /system/opt/google/disk ]; then
	rm -r /system/opt/google/disk
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
fi
if [ -d /system/opt/google/dsm ]; then
	rm -r /system/opt/google/dsm
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
fi
if [ -d /system/opt/google/touch ]; then
	rm -r /system/opt/google/touch
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 6))); fi
fi
if [ -d /system/opt/intel ]; then
	rm -r /system/opt/intel
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 7))); fi
fi
if [ -d /system/usr/share/chromeos-assets/autobrightness ]; then
	rm -r /system/usr/share/chromeos-assets/autobrightness
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 8))); fi
fi
exit $ret
