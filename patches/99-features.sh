ret=0
if [ ! -f "/system/etc/chrome_dev.conf" ]; then ret=$((ret + (2 ** 0))); fi
echo '--ash-debug-shortcuts' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo '--force-tablet-power-button' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
echo '--enable-features=ArcUsbStorageUI' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
echo '--gpu-sandbox-failures-fatal=no' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
echo '--enable-hardware-overlays="single-fullscreen,single-on-top"' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 6))); fi

youtube_fix=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "youtube_fix" ]; then youtube_fix=1; fi
done
if [ "$youtube_fix" -eq 1 ]; then
echo '!--enable-features=Pepper3DImageChromium' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 7))); fi
fi

exit $ret
