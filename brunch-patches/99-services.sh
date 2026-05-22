# Disable services that should not be present on non-chromebooks

ret=0
if [ -f /roota/etc/init/console-ttyS0.conf ]; then echo -e "# Disabled" > /roota/etc/init/console-ttyS0.conf; fi
exit $ret
