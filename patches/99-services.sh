# Disable services should not be present on non-chromebooks

ret=0
if [ -f /roota/etc/init/console-ttyS0.conf ]; then
	cat >/roota/etc/init/console-ttyS0.conf <<TTYS0
# Disabled
TTYS0
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
