# This patch created by @midi1996 allows to dynamically detect tablet mode according to the presence of a specific hid device (i.e. usb keyboard)
# Usage: add "options=hid_tablet_mode.<hid_vendor_id>.<hid_product_id>" on the kernel command line

hid_tablet_mode=0
uvid=""
upid=""
noid=0
ret=0


for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$(echo "$i" | cut -d\. -f1)" = "hid_tablet_mode" ]; then
		echo "Tablet Mode Enabled"
		hid_tablet_mode=1
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
		if [ ! -z "$(echo "$i" | cut -d\. -f2)" ] && [ ! -z "$(echo "$i" | cut -d\. -f3)" ]; then
			uvid="$(echo "$i" | cut -d\. -f2)"
			echo "$uvid"
			upid="$(echo "$i" | cut -d\. -f3)"
			echo "$upid"
		else
			noid=1
			echo "No Device IDs have been passed."
		fi
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	fi
done


if [ "$hid_tablet_mode" -eq 1 ]; then
	cat >/system/etc/init/hid-tablet-mode.conf <<MODULEPROBE
start on starting boot-services

script
	modprobe tablet_mode_switch
	echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode
end script
MODULEPROBE
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
	if [ "$noid" -eq 0 ]; then
		cat >/system/lib/udev/rules.d/99-hid-tablet-mode.rules <<UDEVRULE
ACTION=="add", SUBSYSTEMS=="hid", KERNEL=="*:$(echo "$uvid" | tr '[:lower:]' '[:upper:]'):$(echo "$upid" | tr '[:lower:]' '[:upper:]').*" , RUN+="/bin/bash -c 'echo 0 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
ACTION=="remove", SUBSYSTEMS=="hid", KERNEL=="*:$(echo "$uvid" | tr '[:lower:]' '[:upper:]'):$(echo "$upid" | tr '[:lower:]' '[:upper:]').*" , RUN+="/bin/bash -c 'echo 1 > /sys/bus/platform/devices/tablet_mode_switch.0/tablet_mode'"
UDEVRULE
	fi
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi

exit $ret
