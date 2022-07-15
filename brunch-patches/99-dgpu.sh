# This patch adds a udev rule which helps certain dGPUs to stay suspended (improves battery life and prevents overheating)

ret=0

cat >/roota/lib/udev/rules.d/99-nvidia_pm.rules <<NOUVEAUPM
# Remove NVIDIA USB xHCI Host Controller devices, if present
ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10de", ATTR{class}=="0x0c0330", ATTR{power/control}="auto", ATTR{remove}="1"

# Remove NVIDIA USB Type-C UCSI devices, if present
ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10de", ATTR{class}=="0x0c8000", ATTR{power/control}="auto", ATTR{remove}="1"

# Remove NVIDIA Audio devices, if present
ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10de", ATTR{class}=="0x040300", ATTR{power/control}="auto", ATTR{remove}="1"

# Remove NVIDIA VGA/3D controller devices
ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10de", ATTR{class}=="0x03[0-9]*", ATTR{power/control}="auto", ATTR{remove}="1"
NOUVEAUPM
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
cat >/roota/etc/modprobe.d/blacklist-nouveau.conf <<NOUVEAUBLACKLIST
blacklist nouveau
options nouveau modeset=0
NOUVEAUBLACKLIST
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

exit $ret
