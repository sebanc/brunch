# This is an ugly hack to delay ChromeOS login screen until all modules have been loaded.
# It at least fixes an issue where the android container would fail to start and might also solve some sound/bluetooth issues.

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "no_ui_delay" ]; then exit 0; fi
done

ret=0
grep -q 'start on started boot-services' /system/etc/init/ui.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
sed -i 's#start on started boot-services#start on started boot-services and stopped udev-trigger#g' /system/etc/init/ui.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
