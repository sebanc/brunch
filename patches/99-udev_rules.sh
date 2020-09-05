# Some devices (notably the surface go type cover) need autosuspend to be enabled on the corresponding usb port to properly suspend/resume.

ret=0

cat >/system/lib/udev/rules.d/99-auto_suspend.rules <<AUTOSUSPEND
ENV{ID_MODEL}=="Surface_Type_Cover", ENV{ID_MODEL_ID}=="096f", TEST=="power/control", ATTR{power/control}="auto"
ENV{ID_MODEL}=="Surface_Keyboard", ENV{ID_MODEL_ID}=="09b5", TEST=="power/control", ATTR{power/control}="auto"
AUTOSUSPEND
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
