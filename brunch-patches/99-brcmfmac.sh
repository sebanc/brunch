# Disable both SAE (0x80000) and FWSUP (0x02000) for brcmfmac kernel module

ret=0

cat >/roota/etc/modprobe.d/brcmfmac.conf <<MODPROBE
options brcmfmac feature_disable=0x82000
MODPROBE
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret

