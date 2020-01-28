for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "disable_touchpad_fix" ]; then echo "brunch: $0 disabled" > /dev/kmsg; exit 0; fi
done

ret=0
grep -q '    Option          "AccelerationProfile" "-1"' /system/etc/gesture/40-touchpad-cmt.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
sed -i '0,/    Option          "AccelerationProfile" "-1"/{s/    Option          "AccelerationProfile" "-1"/    Option          "Tap Minimum Pressure" "1"/}' /system/etc/gesture/40-touchpad-cmt.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
