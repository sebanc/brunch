# Add the option "oled_display" to use specific kernel modules with oled backlight support

oled_display=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "oled_display" ]; then oled_display=1; fi
done

ret=0
if [ "$oled_display" -eq 1 ]; then
	echo "brunch: $0 oled_display enabled" > /dev/kmsg
	cp /roota/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/i915-oled.ko /roota/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/kernel/drivers/gpu/drm/i915/i915.ko
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cp /roota/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/drm_kms_helper-oled.ko /roota/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/kernel/drivers/gpu/drm/drm_kms_helper.ko
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
