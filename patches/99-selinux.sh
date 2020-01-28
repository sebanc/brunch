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
exit $ret
