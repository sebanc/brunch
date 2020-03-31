ret=0
if [ ! -f "/system/lib/udev/rules.d/60-io-scheduler.rules" ]; then exit 0; fi
rm /system/lib/udev/rules.d/60-io-scheduler.rules
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
