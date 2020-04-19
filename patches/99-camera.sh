ret=0
cat >/system/etc/init/camera.conf <<CAMERASCRIPT
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
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
