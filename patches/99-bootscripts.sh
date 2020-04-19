ret=0

cat >/system/etc/init/bootscripts.conf <<BOOTSCRIPTS
start on stopped udev-trigger

script
	if [ \$(ls -1q /var/brunch/bootscripts/*.sh 2>/dev/null | wc -l) -gt 0 ]; then
		for patch in /var/brunch/bootscripts/*.sh
		do
			/bin/bash "\$patch"
		done
	fi
end script
BOOTSCRIPTS
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
