# Nvidia dual GPUs are not used by ChromeOS, this patch helps certain dGPUs to stay suspended (improves battery life and prevents overheating) 

ret=0
cat >/roota/lib/udev/rules.d/99-nvidia_pm.rules <<NVIDIAPM
ACTION=="add|change", SUBSYSTEM=="pci", ENV{PCI_CLASS}=="30200", ATTR{vendor}=="0x10de", ATTR{power/control}="auto"
NVIDIAPM
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
