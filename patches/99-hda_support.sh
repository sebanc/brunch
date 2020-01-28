disable_intel_hda=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "disable_intel_hda" ]; then disable_intel_hda=1; fi
done

ret=0
if [ -f "/system/etc/modprobe.d/alsa-skl.conf" ]; then rm /system/etc/modprobe.d/alsa-skl.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
if [ "$disable_intel_hda" -eq 1 ]; then echo "blacklist snd_hda_intel" > /system/etc/modprobe.d/alsa-skl.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
