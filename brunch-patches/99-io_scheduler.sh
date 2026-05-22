# This patch disables the default io-scheduler which appears to cause reboots when ChromeOS is running from a slow usb device.

ret=0
if [ ! -f "/roota/lib/udev/rules.d/60-io-scheduler.rules" ]; then exit 0; fi
rm /roota/lib/udev/rules.d/60-io-scheduler.rules
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
