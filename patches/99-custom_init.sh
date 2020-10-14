# This patch seems to improve android apps compatibility by loading modules before the ui is displayed.

android_init_fix=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "android_init_fix" ]; then android_init_fix=1; fi
done

ret=0
if [ "$android_init_fix" -eq 1 ]; then
grep -q 'start on started boot-services' /system/etc/init/ui.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
sed -i 's#start on started boot-services#start on started boot-services and stopped udev-trigger#g' /system/etc/init/ui.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
grep -q 'start on starting failsafe' /system/etc/init/udev-trigger.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
sed -i 's#start on starting failsafe#start on stopped udev-trigger-early and started network-services#g' /system/etc/init/udev-trigger.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi
exit $ret
