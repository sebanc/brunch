sysfs_tablet_mode=0
force_tablet_mode=0

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "sysfs_tablet_mode" ]; then sysfs_tablet_mode=1; fi
	if [ "$i" == "force_tablet_mode" ]; then force_tablet_mode=1; fi
done

ret=0
if [ "$sysfs_tablet_mode" -eq 1 ]; then
	cat >/system/etc/init/tablet-mode.conf <<MODULEPROBE
start on starting boot-services

script
	modprobe tablet_mode_switch
end script
MODULEPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/system/lib/udev/rules.d/99-tablet-mode.rules <<UDEVRULE
ACTION=="add", SUBSYSTEMS=="usb", ATTRS{idVendor}=="0416", ATTRS{idProduct}=="c141", RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="usb", ATTRS{idVendor}=="0416", ATTRS{idProduct}=="c141", RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
UDEVRULE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

if [ "$force_tablet_mode" -eq 1 ]; then
	cat >/system/etc/init/tablet-mode.conf <<MODULEPROBE
start on starting boot-services

script
	modprobe tablet_mode_switch
	echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode
end script
MODULEPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi

exit $ret
