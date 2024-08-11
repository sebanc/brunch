# This patch attempts to fix android apps failing to start on some devices.

android_init_fix=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "android_init_fix" ]; then android_init_fix=1; fi
done

ret=0

if [ "$android_init_fix" -eq 1 ]; then
	sed -i 's@post-start script@post-start script\n  sleep 10@g' /roota/etc/init/ui.conf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi

exit $ret

