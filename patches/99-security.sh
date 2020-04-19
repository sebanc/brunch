ret=0

echo "" > /system/.restorecon
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

cat >/system/etc/init/selinux.conf <<SELINUX
start on starting boot-services

script
	if [ -f /.restorecon ]; then
		restorecon -R /etc
		restorecon -R /lib
		restorecon -R /usr/bin
		restorecon -R /usr/lib64
		rm /.restorecon
	fi
end script
SELINUX
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

for i in $(cat /proc/cmdline); do
	if [ "$i" == "cros_debug" ]; then
		sed -i 's#SELINUX=enforcing#SELINUX=permissive#g' /system/etc/selinux/config
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
		echo "" > /system/usr/share/cros/startup/disable_stateful_security_hardening
		if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
	fi
done

exit $ret
