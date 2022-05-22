# Add the option "internal_mic_fix" to forecefully enable internal microphone (for some it is disabled by default) 

internal_mic_fix=0
internal_mic_fix2=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "internal_mic_fix" ]; then internal_mic_fix=1; fi
	if [ "$i" == "internal_mic_fix2" ]; then internal_mic_fix2=1; fi
done

ret=0

if [ "$internal_mic_fix" -eq 1 ]; then
	cat >/roota/etc/init/internal-mic-fix.conf <<INTERNALMICFIX
start on stopped udev-trigger and started ui

script
    amixer -c0 set "Internal Mic" toggle
end script
INTERNALMICFIX
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi

if [ "$internal_mic_fix2" -eq 1 ]; then
	cat >/roota/etc/init/internal-mic-fix.conf <<INTERNALMICFIX2
start on stopped udev-trigger and started ui

script
    amixer -c1 set "Internal Mic" toggle
end script
INTERNALMICFIX2
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

exit $ret
