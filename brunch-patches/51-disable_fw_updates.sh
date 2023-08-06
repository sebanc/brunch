# Disable chromebooks firmware writing mechanisms to avoid the risk (very low) that ChromeOS would flash the firmware of an unsupported device

ret=0

echo "exit 0" > /roota/usr/sbin/chromeos-firmwareupdate
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
echo "exit 0" > /roota/usr/sbin/flashrom
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo "exit 0" > /roota/usr/sbin/gsctool
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
rm -r /roota/opt/google/touch/scripts/*
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi

exit $ret
