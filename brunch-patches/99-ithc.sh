# Add the option "ithc" to make use of the new ithc driver maintained by quo on github

ithc=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "ithc_touchscreen" ]; then ithc=1; fi
done

ret=0
if [ "$ithc" -eq 1 ]; then
	echo "brunch: $0 ithc enabled" > /dev/kmsg
	cat >/roota/etc/init/ipts.conf <<ITHC
start on stopped udev-trigger

script
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ipts.ko gen7mt=1
	insmod /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/ithc.ko poll=1
	iptsd
end script
ITHC
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	tar zxf /rootc/packages/ithc.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi
exit $ret
