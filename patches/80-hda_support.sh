# Most chromebooks have specific audio devices, as such the intel hda card is disabled by default in recovery images.
# Allow intel hda soundcards by default and add the "disable_intel_hda" option to be used for chromebooks.

disable_intel_hda=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "disable_intel_hda" ]; then disable_intel_hda=1; fi
done

ret=0
if [ ! "$(ls /system/etc/modprobe.d | grep 'alsa-.*\.conf' | wc -l)" -eq 0 ]; then rm /system/etc/modprobe.d/alsa-*.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
if [ "$disable_intel_hda" -eq 1 ]; then echo "blacklist snd_hda_intel" > /system/etc/modprobe.d/alsa-hda.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
