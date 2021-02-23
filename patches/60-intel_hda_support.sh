# Most chromebooks have specific audio devices, as such the intel hda card is disabled by default in recovery images.
# Allow intel hda soundcards by default and add the "disable_intel_hda" option to be used for chromebooks.
# Keep the original chromebook configuration if "native_chromebook_image" option is used

disable_intel_hda=0
native_chromebook_image=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "disable_intel_hda" ]; then disable_intel_hda=1; fi
	if [ "$i" == "native_chromebook_image" ]; then native_chromebook_image=1; fi
done

if [ "$native_chromebook_image" -eq 1 ]; then exit 0; fi

ret=0

if [ ! "$(ls /roota/etc/modprobe.d | grep 'alsa-.*\.conf' | wc -l)" -eq 0 ]; then rm /roota/etc/modprobe.d/alsa-*.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
cat >/roota/etc/modprobe.d/alsa-order.conf <<SNDCARDORDER
options snd-hda-intel id=HDMI index=1
options snd-hdmi-lpe-audio index=1
SNDCARDORDER
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
if [ "$disable_intel_hda" -eq 1 ]; then echo "blacklist snd_hda_intel" > /roota/etc/modprobe.d/alsa-hda.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
exit $ret
