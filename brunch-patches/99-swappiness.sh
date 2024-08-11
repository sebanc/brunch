# This option increases swappiness to 100 to allow better performance for low ram devices

increase_swappiness=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "increase_swappiness" ]; then increase_swappiness=1; fi
done

ret=0

if [ "$increase_swappiness" -eq 1 ]; then
	cat >/roota/etc/sysctl.d/99-swappiness.conf <<SWAPPINESS
vm.swappiness=100
SWAPPINESS
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi

exit $ret

