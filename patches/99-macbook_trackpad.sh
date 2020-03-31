ret=0

cat >/system/etc/gesture/60-macbook-touchpad.conf <<GESTURE
Section "InputClass"
    Identifier      "CMT for Apple Magic Trackpad"
    MatchUSBID      "05ac:02*"
    MatchDevicePath "/dev/input/event*"
    MatchIsTouchpad "on"
    Option          "Touchpad Stack Version" "1"
# We are using raw touch major value as pressure value, so set the Palm
# pressure threshold high.
    Option          "Palm Pressure" "256"
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
GESTURE
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

cat >/system/lib/udev/rules.d/60-evdev-macbook-touchpad.rules <<UDEVEVDEV
ACTION=="remove", GOTO="evdev_apple_end"
KERNEL!="event*", GOTO="evdev_apple_end"

ENV{ID_INPUT_TOUCHPAD}=="1", \
  ATTRS{name}=="bcm5974", \
  ENV{EVDEV_ABS_00}="::94", ENV{EVDEV_ABS_01}="::92", \
  ENV{EVDEV_ABS_35}="::94", ENV{EVDEV_ABS_36}="::92", \
  RUN{builtin}+="keyboard", GOTO="evdev_apple_end"

LABEL="evdev_apple_end"
UDEVEVDEV
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

cat >/system/lib/udev/rules.d/90-powerd-macbook-touchpad.rules <<UDEVPOWERD
SUBSYSTEM=="input", ENV{ID_INPUT_TOUCHPAD}=="1",    ATTRS{name}=="bcm5974",  ENV{POWERD_ROLE}="internal_touchpad"
UDEVPOWERD
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi

exit $ret
