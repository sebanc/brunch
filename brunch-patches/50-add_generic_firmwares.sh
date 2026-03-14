# Cleanup chromebooks specific config files / firmwares and add generic ones instead

native_chromebook_image=0
no_camera_config=0
invert_camera_order=0

for i in ${1//,/ }; do
    case "$i" in
        native_chromebook_image) native_chromebook_image=1 ;;
        no_camera_config) no_camera_config=1 ;;
        invert_camera_order) invert_camera_order=1 ;;
    esac
done

ret=0
check() { [ $? -eq 0 ] || ret=$((ret + (1 << $1))); }

if [ "$native_chromebook_image" -eq 1 ]; then
    rm -r /roota/lib/firmware/iwlwifi*; check 0
    tar zxf /rootc/packages/firmwares.tar.gz -C /tmp; check 1
    cp /tmp/iwlwifi* /roota/lib/firmware/; check 2
    exit $ret
fi

rm -r /roota/lib/firmware/*; check 0
tar zxf /rootc/packages/firmwares.tar.gz -C /roota/lib/firmware; check 1
tar zxf /rootc/packages/alsa-ucm-conf.tar.gz -C /roota/usr/share/alsa; check 2

reverse_pipe=""
[ "$invert_camera_order" -eq 1 ] && reverse_pipe="| tac"

# Add loop prolog and create udev rules in the beginning
cat >/roota/etc/init/camera.conf <<CAMERASCRIPT
start on stopped udev-trigger

script
rm -f /etc/camera/camera_characteristics.conf
rm -f /lib/udev/rules.d/99-camera.rules

logger -t "camera.conf" "Starting camera discovery"

nr=0
for i in \$(dmesg | grep "Found UVC" | sed 's/^.*(//;s/)\$//' | uniq $reverse_pipe); do
    vendor=\${i%%:*}
    product=\${i##*:}

    echo "SUBSYSTEM==\\"video4linux\\", ATTRS{idVendor}==\\"\$vendor\\", ATTRS{idProduct}==\\"\$product\\", ATTR{index}==\\"0\\", GROUP=\\"camera\\", SYMLINK+=\\"camera-internal\$nr\\"" >> /lib/udev/rules.d/99-camera.rules
    echo "SUBSYSTEM==\\"video4linux\\", ATTRS{idVendor}==\\"\$vendor\\", ATTRS{idProduct}==\\"\$product\\", ATTR{index}!=\\"0\\", GROUP=\\"camera\\"" >> /lib/udev/rules.d/99-camera.rules
CAMERASCRIPT

# Add camera config creation if it wasn't disabled
if [ "$no_camera_config" -eq 0 ]; then
cat >>/roota/etc/init/camera.conf <<'CAMERA'
    cat >>/etc/camera/camera_characteristics.conf <<EOF
camera$nr.lens_facing=0
camera$nr.sensor_orientation=0
camera$nr.module0.usb_vid_pid=$vendor:$product
camera$nr.module0.constant_framerate_unsupported=true
camera$nr.module0.horizontal_view_angle_16_9=66.2
camera$nr.module0.horizontal_view_angle_4_3=56.7
camera$nr.module0.lens_info_available_apertures=2.0
camera$nr.module0.lens_info_available_focal_lengths=1.32
camera$nr.module0.lens_info_minimum_focus_distance=0.18
camera$nr.module0.lens_info_optimal_focus_distance=0.45
camera$nr.module0.resolution_1280x960_unsupported=true
camera$nr.module0.sensor_info_physical_size=3.285x2.549
camera$nr.module0.sensor_info_pixel_array_size=1280x720
camera$nr.module0.vertical_view_angle_16_9=41.1
camera$nr.module0.vertical_view_angle_4_3=41.1
EOF
CAMERA
fi

# Add loop epilog and increment device number
# Replay udev rules in the end or the effect will be only after second reboot
cat >>/roota/etc/init/camera.conf <<'CAMERASCRIPT'
    nr=$((nr+1))
done

udevadm control --reload-rules
udevadm trigger --subsystem-match=video4linux

end script
CAMERASCRIPT

check 3

[ -d /roota/etc/dptf ] && { rm -r /roota/etc/dptf/*; check 4; }

exit $ret
