if [ -d /system/usr/share/power_manager/board_specific ]; then rm -r /system/usr/share/power_manager/board_specific; fi

ret=0

mkdir -p /system/usr/share/power_manager/board_specific
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

echo 2 > /system/usr/share/power_manager/board_specific/has_ambient_light_sensor
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

if [ $(cat /sys/power/mem_sleep | cut -d' ' -f1) == '[s2idle]' ]; then
	echo 1 > /system/usr/share/power_manager/board_specific/suspend_to_idle
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
fi

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "suspend_s0" ]; then
		echo 1 > /system/usr/share/power_manager/board_specific/suspend_to_idle
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 6))); fi
	fi
	if [ "$i" == "suspend_s3" ]; then
		echo 0 > /system/usr/share/power_manager/board_specific/suspend_to_idle
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 7))); fi
	fi
	if [ "$i" == "advanced_als" ]; then
		cat >/system/usr/share/power_manager/board_specific/internal_backlight_als_steps <<ALS
19.88 19.88 -1 15
29.48 29.48 8 40
37.59 37.59 25 100
47.62 47.62 70 250
60.57 60.57 180 360
71.65 71.65 250 500
85.83 85.83 350 1700
93.27 93.27 1100 6750
100.0 100.0 5250 -1
ALS
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 8))); fi
	fi
done

exit $ret
