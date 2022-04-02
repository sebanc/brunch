# Add the option "ipts" to make use of the new ipts driver maintained by linux-surface team

ipts=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "ipts" ]; then ipts=1; fi
done

ret=0
if [ "$ipts" -eq 1 ]; then
	echo "brunch: $0 ipts enabled" > /dev/kmsg
	cat >/roota/etc/init/ipts.conf <<IPTS
start on stopped udev-trigger

script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko
	iptsd
end script
IPTS
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	tar zxf /rootc/packages/ipts.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
