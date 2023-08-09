# This patch attempts to fix the android apps spinning issue on some devices.

android_init_fix=0
android_init_fix2=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "android_init_fix" ]; then android_init_fix=1; fi
	if [ "$i" == "android_init_fix2" ]; then android_init_fix2=1; fi
done

ret=0

if [ "$android_init_fix" -eq 1 ]; then
	sed -i 's@post-start script@post-start script\n  sleep 10@g' /roota/etc/init/ui.conf
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi

if [ "$android_init_fix2" -eq 1 ]; then
	if [ -f /roota/etc/init/arc-ureadahead.conf ]; then rm /roota/etc/init/arc-ureadahead.conf; fi
	if [ -f /roota/etc/init/arc-lifetime.conf ]; then rm /roota/etc/init/arc-lifetime.conf; fi
	if [ -f /roota/etc/init/arc-keymasterd.conf ]; then rm /roota/etc/init/arc-keymasterd.conf; fi
	sed -i "s@start on continue-arc-boot@start on continue-arc-boot and stopped arc-init-wait@g" /roota/etc/init/*
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/etc/init/arc-init-wait.conf <<INIT
start on stopped startup
script
	arc-init-wait
end script
INIT
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	cat >/roota/usr/sbin/arc-init-wait <<SCRIPT
#!/bin/bash
for i in {1..300}
do
        if dmesg | grep -q 'uses 32-bit capabilities'; then
                echo "arc-init-wait: waited until loop \$i" > /dev/kmsg
                break
        fi
        #echo "arc-init-wait: not found until loop \$i" > /dev/kmsg
        sleep 1
done
SCRIPT
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
	chmod 0755 /roota/usr/sbin/arc-init-wait
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
fi

exit $ret

