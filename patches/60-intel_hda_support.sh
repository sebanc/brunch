# Most chromebooks have specific audio devices, as such the intel hda card is disabled by default in recovery images.
# Allow intel hda soundcards by default and add the "disable_intel_hda" option to be used for chromebooks.
# Keep the original chromebook configuration if "native_chromebook_image" option is used

native_chromebook_image=0
chromebook_audio=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "native_chromebook_image" ]; then native_chromebook_image=1; fi
	if [ "$i" == "chromebook_audio" ]; then chromebook_audio=1; fi
done

if [ "$native_chromebook_image" -eq 1 ]; then exit 0; fi

ret=0

if [ ! "$(ls /roota/etc/modprobe.d | grep 'alsa-.*\.conf' | wc -l)" -eq 0 ]; then rm /roota/etc/modprobe.d/alsa-*.conf; fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
if [ "$chromebook_audio" -eq 0 ]; then
	sed -i '/cards_limit/d' /roota/etc/modprobe.d/alsa.conf
	cat >/roota/etc/modprobe.d/blacklist-snd-hdmi-lpe-audio.conf <<BLACKLIST
blacklist snd_hdmi_lpe_audio
BLACKLIST
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
else
	echo "blacklist snd_hda_intel" > /roota/etc/modprobe.d/alsa-hda.conf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
	rm -f /roota/lib/firmware/intel/fw_sst_*
fi

exit $ret
