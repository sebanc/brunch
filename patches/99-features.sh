ret=0
if [ ! -f "/system/etc/chrome_dev.conf" ]; then ret=$((ret + (2 ** 0))); fi
echo '--ash-debug-shortcuts' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo '--force-tablet-power-button' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
echo '--enable-features=ArcUsbStorageUI' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
echo '!--enable-webgl-image-chromium' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
echo '--gpu-sandbox-failures-fatal=no' >> /system/etc/chrome_dev.conf
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
exit $ret
