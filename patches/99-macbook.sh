# Load additional modules with macbook kernel

ret=0

if cat /proc/version | grep -q "macbook"; then
	cat >/roota/etc/init/apple_modules.conf <<MODPROBE
start on stopped udev-trigger

script
	modprobe apple-bce
	modprobe apple-ibridge
	modprobe apple-ib-tb
	modprobe apple-ib-als
end script
MODPROBE
fi

exit $ret
