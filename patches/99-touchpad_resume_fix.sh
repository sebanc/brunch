# This is an attempt to fix issues with the touchpad which do not resume on some devices

touchpad_resume_fix=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "touchpad_resume_fix" ]; then touchpad_resume_fix=1; fi
done

ret=0
if [ "$touchpad_resume_fix" -eq 1 ]; then
	cat >/roota/etc/init/touchpad-resume.conf <<TOUCHPADFIX
start on stopped udev-trigger

script
	dbus-monitor --system --profile "interface='org.chromium.PowerManager',member='SuspendDone'" | while read -r line; do
		rmmod i2c_hid
		modprobe i2c_hid
	done
end script
TOUCHPADFIX
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
