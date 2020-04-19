iwlwifi_backport=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "iwlwifi_backport" ]; then iwlwifi_backport=1; fi
done

ret=0
if [ "$iwlwifi_backport" -eq 1 ]; then
	cp -r /firmware/lib/iwlwifi_backport/$(cat /proc/version |  cut -d' ' -f3)/* /system/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
