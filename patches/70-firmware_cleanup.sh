# Cleanup chromebooks specific config files / firmares as they might break things

ret=0

#if [ -d /roota/etc/bluetooth ]; then
#	rm -r /roota/etc/bluetooth/*
#	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
#fi

if [ -d /roota/etc/cras ]; then
	rm -r /roota/etc/cras/*
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

if [ -d /roota/etc/dptf ]; then
	rm -r /roota/etc/dptf/*
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi

#if [ -f /roota/lib/udev/rules.d/99-bluetooth-suspend-owner.rules ]; then
#	rm -r /roota/lib/udev/rules.d/99-bluetooth-suspend-owner.rules
#	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
#fi

exit $ret
