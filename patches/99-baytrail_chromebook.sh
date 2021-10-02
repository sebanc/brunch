# Specific patch to allow sound on baytrail chromebooks with the option "baytrail_chromebook"

baytrail_chromebook=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "baytrail_chromebook" ]; then baytrail_chromebook=1; fi
done

ret=0
if [ "$baytrail_chromebook" -eq 1 ]; then
	echo "blacklist snd_hda_intel" > /roota/etc/modprobe.d/alsa-hda.conf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/chtmax98090.conf <<CHTMAX98090
start on stopped udev-trigger

script
	alsaucm -c chtmax98090 set _verb HiFi set _enadev Headphone
	alsaucm -c chtmax98090 set _verb HiFi set _enadev Speakers
	alsaucm -c chtmax98090 set _verb HiFi set _enadev HeadsetMic
	alsaucm -c chtmax98090 set _verb HiFi set _enadev InternalMic
	mkdir -p /var/lib/alsa
	cp /usr/share/alsa/ucm/chtmax98090/asound.state /var/lib/alsa/
	alsactl restore
end script
CHTMAX98090
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
