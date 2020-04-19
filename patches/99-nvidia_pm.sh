ret=0
cat >/system/lib/udev/rules.d/99-nvidia_pm.rules <<NVIDIAPM
ACTION=="add|change", SUBSYSTEM=="pci", ENV{PCI_CLASS}=="30200", ATTR{vendor}=="0x10de", ATTR{power/control}="auto"
NVIDIAPM
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
