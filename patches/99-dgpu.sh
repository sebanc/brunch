# This patch adds a udev rule which helps certain dGPUs to stay suspended (improves battery life and prevents overheating)

disable_dgpu=0
enable_dgpu=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "disable_dgpu" ]; then disable_dgpu=1; fi
	if [ "$i" == "enable_dgpu" ]; then enable_dgpu=1; fi
done

ret=0

cat >/roota/lib/udev/rules.d/99-nvidia_pm.rules <<NOUVEAUPM
ACTION=="add|change", SUBSYSTEM=="pci", ENV{PCI_CLASS}=="30200", ATTR{vendor}=="0x10de", ATTR{power/control}="auto"
NOUVEAUPM
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

if [ "$disable_dgpu" -eq 0 ] && [ "$enable_dgpu" -eq 0 ]; then
	cat >/roota/etc/modprobe.d/blacklist-nouveau.conf <<NOUVEAUBLACKLIST
blacklist nouveau
NOUVEAUBLACKLIST
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

exit $ret
