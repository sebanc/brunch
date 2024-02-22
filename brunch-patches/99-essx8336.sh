# Add specific alsamixer commands for essx8336

essx8336=0
essx8336_card=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "essx8336_card0" ]; then essx8336=1; essx8336_card=0; fi
	if [ "$i" == "essx8336_card1" ]; then essx8336=1; essx8336_card=1; fi
done

ret=0

if [ "$essx8336" -eq 1 ]; then
	cat >/roota/etc/init/essx8336.conf <<ALSACMD
start on stopped udev-trigger

script
	amixer -c $essx8336_card cset name='Speaker Switch' on
	amixer -c $essx8336_card cset name='Headphone Playback Volume' 3,3
	amixer -c $essx8336_card cset name='Right Headphone Mixer Right DAC Switch' on
	amixer -c $essx8336_card cset name='Left Headphone Mixer Left DAC Switch' on
	amixer -c $essx8336_card cset name='DAC Playback Volume' 999,999
	amixer -c $essx8336_card cset name='Headphone Mixer Volume' 999,999
	amixer -c $essx8336_card sset Headphone 3
	amixer -c $essx8336_card cset name='ADC PGA Gain Volume' 7
	amixer -c $essx8336_card cset name='ADC Capture Volume' 150
	amixer -c $essx8336_card cset name='Internal Mic Switch' on
	amixer -c $essx8336_card cset name='Headset Mic Switch' on
	amixer -c $essx8336_card cset name='Differential Mux' 'lin2-rin2'
end script
ALSACMD
fi

exit $ret
