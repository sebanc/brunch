# Disable chromebooks firmware writing mechanisms to avoid the risk (very low) that ChromeOS would flash the firmware of an unsupported device

ret=0
echo "exit 0" > /roota/usr/bin/nvramtool
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
echo "exit 0" > /roota/usr/sbin/chromeos-firmwareupdate
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo "exit 0" > /roota/usr/sbin/ectool
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
echo "exit 0" > /roota/usr/sbin/flashrom
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
echo "exit 0" > /roota/usr/sbin/vpd
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
echo "exit 0" > /roota/usr/sbin/vpd_get_value
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
rm -r /roota/opt/google/touch/scripts/*
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 6))); fi
exit $ret
