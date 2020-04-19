ret=0

cat >/system/etc/gesture/40-touchpad-cmt.conf <<TOUCHPAD
# Configure touchpads to use Chromium Multitouch (cmt) X input driver
Section "InputClass"
    Identifier      "touchpad"
    MatchIsTouchpad "on"
    MatchDevicePath "/dev/input/event*"
    Driver          "cmt"
    Option	    "libinput Tapping Enabled" "1"
    Option          "Tap Minimum Pressure" "0.1"
    Option          "AccelerationProfile" "-1"
    Option          "Scroll Buttons" "0"
    Option          "Scroll Axes" "1"

    # CMT devices potentially process keyboard events
    Option          "XkbModel" "pc"
    Option          "XkbLayout" "us"
EndSection

Section "InputClass"
    Identifier      "CMT for Apple Magic Trackpad"
    MatchUSBID      "05ac:030e"
    MatchDevicePath "/dev/input/event*"
    Option          "Touchpad Stack Version" "1"
# We are using raw touch major value as pressure value, so set the Palm
# pressure threshold high.
    Option          "Palm Pressure" "1000"
    Option          "Compute Surface Area from Pressure" "0"
    Option          "IIR b0" "1"
    Option          "IIR b1" "0"
    Option          "IIR b2" "0"
    Option          "IIR b3" "0"
    Option          "IIR a1" "0"
    Option          "IIR a2" "0"
    # TODO(clchiou): Calibrate bias on X-axis
    Option          "Touchpad Device Output Bias on X-Axis" "-283.3226025266607"
    Option          "Touchpad Device Output Bias on Y-Axis" "-283.3226025266607"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    # Disable drumroll suppression
    Option          "Drumroll Suppression Enable" "0"
    Option          "Two Finger Vertical Close Distance Thresh" "35.0"
    Option          "Fling Buffer Suppress Zero Length Scrolls" "0"
EndSection

Section "InputClass"
    Identifier      "CMT for Apple Magic Trackpad 2"
    MatchUSBID      "05ac:0265|004c:0265"
    MatchDevicePath "/dev/input/event*"

    Option          "Pressure Calibration Offset" "30"
    Option          "Palm Pressure" "250.0"
    Option          "Palm Width" "20.0"
    Option          "Multiple Palm Width" "20.0"

    # Enable Stationary Wiggle Filter
    Option          "Stationary Wiggle Filter Enabled" "1"
    Option          "Finger Moving Energy" "0.0008"
    Option          "Finger Moving Hysteresis" "0.0004"

    # Avoid accidental scroll/move on finger lift
    Option          "Max Stationary Move Speed" "47"
    Option          "Max Stationary Move Speed Hysteresis" "1"
    Option          "Max Stationary Move Suppress Distance" "0.2"
EndSection

Section "InputClass"
    Identifier      "CMT for Apple Magic Mouse"
    MatchUSBID      "05ac:030d"
    MatchDevicePath "/dev/input/event*"
    Driver          "cmt"
    Option          "AccelerationProfile" "-1"
    Option          "Scroll X Out Scale" "3"
    Option          "Scroll Y Out Scale" "3"
    Option          "Compute Surface Area from Pressure" "0"
    Option          "Max Allowed Pressure Change Per Sec" "170.0"
    Option          "Max Hysteresis Pressure Per Sec" "170.0"
    Option          "Max Finger Stationary Speed" "94.32"
    Option          "Mouse Accel Curves" "1"
    Option          "Mouse Scroll Curves" "0"
    Option          "Box Width" "8.0"
    Option          "Box Height" "1.0"
    # Resolution overrides:
    Option          "Vertical Resolution" "40"
    Option          "Horizontal Resolution" "45"
    # Assume a frame interval to handle jitter on the bus
    Option          "Accel Min dt" "0.003"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech Wireless Touchpad"
    MatchUSBID      "046d:4011"
    MatchIsTouchpad "on"
    MatchDevicePath "/dev/input/event*"
    Option          "Touchpad Stack Version" "1"
    Option          "IIR b0" "1"
    Option          "IIR b1" "0"
    Option          "IIR b2" "0"
    Option          "IIR b3" "0"
    Option          "IIR a1" "0"
    Option          "IIR a2" "0"
    Option          "Pressure Calibration Offset" "-313.240741792594"
    Option          "Pressure Calibration Slope" "4.39678062436752"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Palm Pressure" "100000.0"
    Option          "Two Finger Vertical Close Distance Thresh" "35.0"
    Option          "Fling Buffer Suppress Zero Length Scrolls" "0"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech T650"
    MatchUSBID      "046d:4101"
    MatchIsTouchpad "on"
    MatchDevicePath "/dev/input/event*"
    Option          "Touchpad Stack Version" "1"
    Option          "IIR b0" "1"
    Option          "IIR b1" "0"
    Option          "IIR b2" "0"
    Option          "IIR b3" "0"
    Option          "IIR a1" "0"
    Option          "IIR a2" "0"
    Option          "Pressure Calibration Offset" "-0.439288351750068"
    Option          "Pressure Calibration Slope" "3.05998553523335"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Two Finger Vertical Close Distance Thresh" "35.0"
    Option          "Fling Buffer Suppress Zero Length Scrolls" "0"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech T651"
    MatchUSBID      "046d:b00c"
    MatchIsTouchpad "on"
    MatchDevicePath "/dev/input/event*"
    Option          "Touchpad Stack Version" "1"
    Option          "IIR b0" "1"
    Option          "IIR b1" "0"
    Option          "IIR b2" "0"
    Option          "IIR b3" "0"
    Option          "IIR a1" "0"
    Option          "IIR a2" "0"
    Option          "Pressure Calibration Offset" "-4.46520447177073"
    Option          "Pressure Calibration Slope" "3.21071719332644"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Two Finger Vertical Close Distance Thresh" "35.0"
    Option          "Fling Buffer Suppress Zero Length Scrolls" "0"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech T620"
    MatchUSBID      "046d:4027"
    MatchDevicePath "/dev/input/event*"
    Driver          "cmt"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Box Width" "6"
    Option          "Box Height" "1"
    Option          "Drumroll Suppression Enable" "0"
    Option          "Input Queue Max Delay" "0.0"
    Option          "Mouse Accel Curves" "1"
    Option          "Mouse Scroll Curves" "0"
    Option          "AccelerationProfile" "-1"
    # Assume a frame interval to handle jitter on the bus
    Option          "Accel Min dt" "0.003"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech T400"
    MatchUSBID      "046d:4026"
    MatchDevicePath "/dev/input/event*"
    Driver          "cmt"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Box Width" "6"
    Option          "Box Height" "1"
    Option          "Drumroll Suppression Enable" "0"
    Option          "Input Queue Max Delay" "0.0"
    Option          "Mouse Accel Curves" "1"
    Option          "Mouse Scroll Curves" "0"
    Option          "AccelerationProfile" "-1"
    # Assume a frame interval to handle jitter on the bus
    Option          "Accel Min dt" "0.003"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech Bluetooth Touchmouse"
    MatchUSBID      "046d:b00d"
    MatchDevicePath "/dev/input/event*"
    Driver          "cmt"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Box Width" "6"
    Option          "Box Height" "1"
    Option          "Drumroll Suppression Enable" "0"
    Option          "Input Queue Max Delay" "0.0"
    Option          "Mouse Accel Curves" "1"
    Option          "Mouse Scroll Curves" "0"
    Option          "AccelerationProfile" "-1"
    # Assume a frame interval to handle jitter on the bus
    Option          "Accel Min dt" "0.003"
EndSection

Section "InputClass"
    Identifier      "CMT for Logitech TK820"
    MatchUSBID      "046d:4102"
    MatchDevicePath "/dev/input/event*"
    Driver          "cmt"
    Option          "Touchpad Stack Version" "2"
    # Pressure jumps around a lot on this touchpad, so allow that:
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Pressure Calibration Offset" "-18.8078435"
    Option          "Pressure Calibration Slope" "2.466208137"
EndSection

Section "InputClass"
    Identifier "CMT for Stantum"
    MatchDevicePath "/dev/input/event*"
    MatchProduct    "MTP_USB_Controller"
    Driver          "cmt"
    Option          "SendCoreEvents" "On"
    Option          "IIR b0" "1"
    Option          "IIR b1" "0"
    Option          "IIR b2" "0"
    Option          "IIR b3" "0"
    Option          "IIR a1" "0"
    Option          "IIR a2" "0"
    Option          "IIR Distance Threshold" "1000"
    Option          "Horizontal Resolution" "8"
    Option          "Vertical Resolution" "10"
    Option          "Two Finger Scroll Distance Thresh" "0.5"
    Option          "Pressure Calibration Offset" "1.0"
    Option          "Pressure Calibration Slope" "15.0"
    Option          "Max Allowed Pressure Change Per Sec" "100000.0"
    Option          "Max Hysteresis Pressure Per Sec" "100000.0"
    Option          "Fling Buffer Suppress Zero Length Scrolls" "0"
EndSection

Section "InputClass"
    Identifier      "Whiskers Touchpad"
    MatchIsTouchpad "on"
    MatchDevicePath "/dev/input/event*"
    MatchUSBID    "18D1:5030"

    # Use new touchpad gesture stack
    Option          "Touchpad Stack Version" "2"
    Option          "Integrated Touchpad" "1"

    Option          "Pressure Calibration Offset" "0.0"
    Option          "Pressure Calibration Slope"  "2"

    # Enable Stationary Wiggle Filter
    Option          "Stationary Wiggle Filter Enabled" "1"

    Option          "Box Width" "0.5"
    Option          "Box Height" "0.5"

    # Avoid accidental scroll/move on finger lift
    Option          "Max Stationary Move Speed" "47"
    Option          "Max Stationary Move Speed Hysteresis" "1"
    Option          "Max Stationary Move Suppress Distance" "0.2"

    # Suppress clicks without fingers on the pad.
    Option          "Zero Finger Click Enable" "0"

    Option          "Filter Low Pressure" "1"
    Option          "Pinch Enable" "1"
    Option          "Palm Pressure" "220.0"
    Option          "Palm Filter Top Edge Enable" "1"
    Option          "Smooth Accel" "1"
    Option          "Tap Minimum Pressure" "20.0"
EndSection

Section "InputClass"
    Identifier      "Brydge Touchpad"
    MatchIsTouchpad "on"
    MatchDevicePath "/dev/input/event*"
    MatchUSBID      "03F6:A001"

    Option          "Fake Timestamp Delta" "0.010"
EndSection
TOUCHPAD
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
