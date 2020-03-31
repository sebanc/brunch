ret=0
cat >/system/lib/udev/rules.d/99-hdd_fix.rules <<HDDFIX
ACTION=="add|change", SUBSYSTEMS=="block", ATTRS{removable}=="0", ENV{UDISKS_PRESENTATION_HIDE}="1"
ACTION=="add|change", SUBSYSTEMS=="block", ATTRS{removable}=="0", ENV{ID_BUS}=="usb", ENV{UDISKS_PRESENTATION_HIDE}="0"
ACTION=="add|change", SUBSYSTEMS=="mmc", ATTRS{type}=="SD", ENV{UDISKS_PRESENTATION_HIDE}="0"
HDDFIX
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
