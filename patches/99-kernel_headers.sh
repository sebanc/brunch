# This patch installs the kernel-devel headers (needed to build dkms modules)

ret=0

rm /system/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/build
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

rm /system/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/source
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

cp -r /firmware/lib/kernel-devel /system/lib/modules/$(cat /proc/version |  cut -d' ' -f3)/build
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi

ln -s /lib/modules/$(cat /proc/version |  cut -d' ' -f3)/build /system/usr/src/linux-headers-$(cat /proc/version |  cut -d' ' -f3)
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi

exit $ret
