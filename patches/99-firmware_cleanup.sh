ret=0
if [ -d /system/opt/intel ]; then
	rm -r /system/opt/intel
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
if [ -d /system/opt/google/cr50 ]; then
	rm -r /system/opt/google/cr50
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
if [ -d /system/opt/google/dsm ]; then
	rm -r /system/opt/google/dsm
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi
if [ -d /system/opt/google/touch ]; then
	rm -r /system/opt/google/touch
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi
exit $ret
