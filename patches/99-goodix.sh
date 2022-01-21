# Add the option "goodix" to use specific kernel modules with goodix touchscreen support

goodix=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "goodix" ]; then goodix=1; fi
done

ret=0
if [ "$goodix" -eq 1 ]; then
	echo "brunch: $0 goodix enabled" > /dev/kmsg
	cp /roota/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/goodix.ko /roota/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/kernel/drivers/input/touchscreen/goodix.ko
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
