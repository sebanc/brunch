ret=0
cat >/system/etc/init/preui-probe.conf <<PREUIPROBE
start on starting boot-services

script
	modprobe i915
	udevadm trigger --action=add --subsystem-match=pci --subsystem-match=hid --subsystem-match=iio
end script
PREUIPROBE
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
