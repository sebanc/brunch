ret=0
echo "exit 0" > /system/usr/sbin/chromeos-firmwareupdate
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
echo "exit 0" > /system/usr/sbin/flashrom
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo "exit 0" > /system/usr/sbin/vpd
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
echo "exit 0" > /system/usr/sbin/vpd_get_value
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
exit $ret
