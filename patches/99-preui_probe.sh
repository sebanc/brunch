# This patch has 2 different purposes:
# - Make sure that the i915 drm driver is probed before any other drm driver
# - Make sure iio sensors modules are loaded early, else ChromeOS will consider they do not exist

ret=0
cat >/system/etc/init/preui-probe.conf <<PREUIPROBE
start on starting boot-services

script
	modprobe i915
	udevadm trigger --action=add --subsystem-match=pci --subsystem-match=i2c --subsystem-match=hid --subsystem-match=iio
end script
PREUIPROBE
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
