# Cleanup chromebooks specific config files / firmares and add generic ones instead
# Keep the original chromebook configuration if "native_chromebook_image" option is used

native_chromebook_image=0
no_camera_config=0
invert_camera_order=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "native_chromebook_image" ]; then native_chromebook_image=1; fi
	if [ "$i" == "no_camera_config" ]; then no_camera_config=1; fi
	if [ "$i" == "invert_camera_order" ]; then invert_camera_order=1; fi
done

if [ "$native_chromebook_image" -eq 1 ]; then
rm -r /roota/lib/firmware/iwlwifi*
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
tar zxf /rootc/packages/firmwares.tar.gz -C /tmp
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
cp /tmp/iwlwifi* /roota/lib/firmware/
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
exit $ret
fi

ret=0

rm -r /roota/lib/firmware/*
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
tar zxf /rootc/packages/firmwares.tar.gz -C /roota/lib/firmware
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
tar zxf /rootc/packages/alsa-ucm-conf.tar.gz -C /roota/usr/share/alsa
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
if [ "$no_camera_config" -eq 1 ]; then
cat >/roota/etc/init/camera.conf <<CAMERASCRIPT
start on starting boot-services

script
if [ -f /etc/camera/camera_characteristics.conf ]; then rm /etc/camera/camera_characteristics.conf; fi
if [ -f /lib/udev/rules.d/50-camera.rules ]; then rm /lib/udev/rules.d/50-camera.rules; fi

nr=0
for i in \$(dmesg | grep "uvcvideo: Found UVC" | sed 's/^.*(//;s/)\$//' | uniq); do
vendor=\$(echo \$i | cut -d: -f1)
product=\$(echo \$i | cut -d: -f2)
echo "SUBSYSTEM==\"video4linux\", ATTRS{idVendor}==\"\$vendor\", ATTRS{idProduct}==\"\$product\", SYMLINK+=\"camera-internal\$nr\"" >> /lib/udev/rules.d/50-camera.rules
nr=\$((nr+1))
done

end script
CAMERASCRIPT
elif [ "$invert_camera_order" -eq 1 ]; then
cat >/roota/etc/init/camera.conf <<CAMERASCRIPT
start on starting boot-services

script
if [ -f /etc/camera/camera_characteristics.conf ]; then rm /etc/camera/camera_characteristics.conf; fi
if [ -f /lib/udev/rules.d/50-camera.rules ]; then rm /lib/udev/rules.d/50-camera.rules; fi

nr=0
for i in \$(dmesg | grep "uvcvideo: Found UVC" | sed 's/^.*(//;s/)\$//' | uniq | tac); do
vendor=\$(echo \$i | cut -d: -f1)
product=\$(echo \$i | cut -d: -f2)
echo "SUBSYSTEM==\"video4linux\", ATTRS{idVendor}==\"\$vendor\", ATTRS{idProduct}==\"\$product\", SYMLINK+=\"camera-internal\$nr\"" >> /lib/udev/rules.d/50-camera.rules
cat >>/etc/camera/camera_characteristics.conf <<CAMERA
camera\$nr.lens_facing=0
camera\$nr.sensor_orientation=0
camera\$nr.module0.usb_vid_pid=\$vendor:\$product
camera\$nr.module0.constant_framerate_unsupported=true
camera\$nr.module0.horizontal_view_angle_16_9=66.2
camera\$nr.module0.horizontal_view_angle_4_3=56.7
camera\$nr.module0.lens_info_available_apertures=2.0
camera\$nr.module0.lens_info_available_focal_lengths=1.32
camera\$nr.module0.lens_info_minimum_focus_distance=0.18
camera\$nr.module0.lens_info_optimal_focus_distance=0.45
camera\$nr.module0.resolution_1280x960_unsupported=true
camera\$nr.module0.sensor_info_physical_size=3.285x2.549
camera\$nr.module0.sensor_info_pixel_array_size=1280x720
camera\$nr.module0.vertical_view_angle_16_9=41.1
camera\$nr.module0.vertical_view_angle_4_3=41.1
CAMERA
nr=\$((nr+1))
done

end script
CAMERASCRIPT
else
cat >/roota/etc/init/camera.conf <<CAMERASCRIPT
start on starting boot-services

script
if [ -f /etc/camera/camera_characteristics.conf ]; then rm /etc/camera/camera_characteristics.conf; fi
if [ -f /lib/udev/rules.d/50-camera.rules ]; then rm /lib/udev/rules.d/50-camera.rules; fi

nr=0
for i in \$(dmesg | grep "uvcvideo: Found UVC" | sed 's/^.*(//;s/)\$//' | uniq); do
vendor=\$(echo \$i | cut -d: -f1)
product=\$(echo \$i | cut -d: -f2)
echo "SUBSYSTEM==\"video4linux\", ATTRS{idVendor}==\"\$vendor\", ATTRS{idProduct}==\"\$product\", SYMLINK+=\"camera-internal\$nr\"" >> /lib/udev/rules.d/50-camera.rules
cat >>/etc/camera/camera_characteristics.conf <<CAMERA
camera\$nr.lens_facing=0
camera\$nr.sensor_orientation=0
camera\$nr.module0.usb_vid_pid=\$vendor:\$product
camera\$nr.module0.constant_framerate_unsupported=true
camera\$nr.module0.horizontal_view_angle_16_9=66.2
camera\$nr.module0.horizontal_view_angle_4_3=56.7
camera\$nr.module0.lens_info_available_apertures=2.0
camera\$nr.module0.lens_info_available_focal_lengths=1.32
camera\$nr.module0.lens_info_minimum_focus_distance=0.18
camera\$nr.module0.lens_info_optimal_focus_distance=0.45
camera\$nr.module0.resolution_1280x960_unsupported=true
camera\$nr.module0.sensor_info_physical_size=3.285x2.549
camera\$nr.module0.sensor_info_pixel_array_size=1280x720
camera\$nr.module0.vertical_view_angle_16_9=41.1
camera\$nr.module0.vertical_view_angle_4_3=41.1
CAMERA
nr=\$((nr+1))
done

end script
CAMERASCRIPT
fi
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
#if [ -d /roota/etc/cras ]; then
#	rm -r /roota/etc/cras/*
#	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
#fi
if [ -d /roota/etc/dptf ]; then
	rm -r /roota/etc/dptf/*
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
fi

exit $ret
