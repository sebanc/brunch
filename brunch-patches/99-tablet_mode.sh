# Implement 2 options to manage tablet mode through sysfs:
# - "sysfs_tablet_mode": the device is started in standard mode
# - "force_tablet_mode": the device is started in tablet mode
# A list of udev rules used to detect tablet mode on certain laptops is included

sysfs_tablet_mode=0
force_tablet_mode=0

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "sysfs_tablet_mode" ]; then sysfs_tablet_mode=1; fi
	if [ "$i" == "force_tablet_mode" ]; then force_tablet_mode=1; fi
done

ret=0

if [ "$sysfs_tablet_mode" -eq 1 ]; then
	cat >/roota/etc/init/tablet-mode.conf <<MODULEPROBE
start on starting boot-services

script
	modprobe tablet_mode_switch
end script
MODULEPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/lib/udev/rules.d/99-tablet-mode.rules <<UDEVRULE
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:0416:C141.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:0416:C141.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:044E:1216.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:044E:1216.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:06CB:5711.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:06CB:5711.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:045E:07A9.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:045E:07A9.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:0B05:183B.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:0B05:183B.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:04F3:0C79.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:04F3:0C79.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
UDEVRULE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

if [ "$force_tablet_mode" -eq 1 ]; then
	cat >/roota/etc/init/tablet-mode.conf <<MODULEPROBE
start on starting boot-services

script
	modprobe tablet_mode_switch
	echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode
end script
MODULEPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
	cat >/roota/lib/udev/rules.d/99-tablet-mode.rules <<UDEVRULE
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:0416:C141.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:0416:C141.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:044E:1216.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:044E:1216.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:06CB:5711.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:06CB:5711.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:045E:07A9.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:045E:07A9.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:0B05:183B.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:0B05:183B.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:04F3:0C79.*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:04F3:0C79.*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
UDEVRULE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi

exit $ret
